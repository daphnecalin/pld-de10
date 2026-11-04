using System.Security.Claims;
using System.Text;
using Microsoft.EntityFrameworkCore;
using QuartusDebugger.Core;

namespace QuartusDebugger.Web;

public sealed class SubmissionStore(AccountDb db, SubmissionFiles files, ILogger<SubmissionStore> logger)
{
    private IQueryable<Submission> OwnedBy(ClaimsPrincipal user)
    {
        string? owner = user.FindFirstValue(ClaimTypes.NameIdentifier);
        return db.Submissions.Where(item => item.OwnerUserId != null && item.OwnerUserId == owner);
    }

    public Task<List<Submission>> ListAsync(ClaimsPrincipal user, bool all = false, CancellationToken ct = default)
    {
        var query = all && user.IsInRole("TA") ? db.Submissions : OwnedBy(user);
        return query.AsNoTracking().OrderByDescending(item => item.UploadedAt).ToListAsync(ct);
    }

    public Task<Submission?> FindAsync(string id, ClaimsPrincipal user, CancellationToken ct)
    {
        var query = user.IsInRole("TA") ? db.Submissions : OwnedBy(user);
        return query.AsNoTracking().SingleOrDefaultAsync(item => item.Id == id, ct);
    }

    public async Task<Stream?> DownloadAsync(string id, string name, ClaimsPrincipal user, CancellationToken ct)
    {
        var item = await FindAsync(id, user, ct);
        return item != null && item.AvailableFiles().Contains(name, StringComparer.Ordinal)
            ? await files.OpenAsync(item.Id, name, ct) : null;
    }

    public async Task<Submission> UploadAsync(IFormFile file, StudentAccount user, CancellationToken ct)
    {
        if (!file.FileName.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Upload a .zip containing the Quartus project source files.");
        if (file.Length == 0 || file.Length > 20L * 1024 * 1024)
            throw new InvalidDataException("ZIP must be between 1 byte and 20 MB. Exclude build output.");

        string id = Guid.NewGuid().ToString("N");
        var row = new Submission(id, user.UserName!, DateTimeOffset.UtcNow, "Processing", 0, 0, "", "",
            user.Id, $"{user.FirstName} {user.LastName}");
        db.Submissions.Add(row);
        await db.SaveChangesAsync(ct);

        var resultRow = row;
        try
        {
            await using (var source = file.OpenReadStream())
                await files.PutAsync(id, "source.zip", source, ct);
            resultRow = resultRow with { HasSource = true };

            await using var stream = file.OpenReadStream();
            var result = await new QuartusProcessingService().ProcessAsync(stream, ct);
            await using (var rtl = new MemoryStream(Encoding.UTF8.GetBytes(result.Verilog)))
                await files.PutAsync(id, "debug_ip.sv", rtl, ct);
            await using (var map = new MemoryStream(Encoding.UTF8.GetBytes(result.CppHeader)))
                await files.PutAsync(id, "debug_map.hpp", map, ct);

            resultRow = resultRow with
            {
                Status = "Complete", Signals = result.SignalCount, Bits = result.TotalBits,
                TopLevel = result.TopLevel, HasDebugIp = true, HasDebugMap = true
            };
        }
        catch (Exception error) when (error is InvalidDataException or NotSupportedException)
        {
            resultRow = resultRow with { Status = "Failed", Error = error.Message };
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
            resultRow = resultRow with { Status = "Failed", Error = "The upload was interrupted. Please submit it again." };
        }
        catch (Exception error)
        {
            logger.LogError(error, "Could not process submission {SubmissionId}", id);
            resultRow = resultRow with { Status = "Failed", Error = "Processing or file storage failed. Please try again." };
        }

        db.Entry(row).CurrentValues.SetValues(resultRow);
        // Persist the final state even when the submitting client has disconnected.
        await db.SaveChangesAsync(CancellationToken.None);
        return resultRow;
    }
}

public sealed record Submission(string Id, string Student, DateTimeOffset UploadedAt, string Status,
    int Signals, int Bits, string TopLevel, string Error, string? OwnerUserId = null, string? StudentName = null,
    bool HasSource = false, bool HasDebugIp = false, bool HasDebugMap = false)
{
    public IEnumerable<string> AvailableFiles()
    {
        if (HasSource) yield return "source.zip";
        if (Status != "Complete") yield break;
        if (HasDebugIp) yield return "debug_ip.sv";
        if (HasDebugMap) yield return "debug_map.hpp";
    }
}
