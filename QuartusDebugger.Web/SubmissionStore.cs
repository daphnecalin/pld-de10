using System.Collections.Concurrent;
using System.Security.Claims;
using System.Text.Json;
using QuartusDebugger.Core;

namespace QuartusDebugger.Web;

public sealed class SubmissionStore
{
    private readonly string root;
    private readonly ConcurrentDictionary<string, Submission> jobs = new();

    public SubmissionStore(IConfiguration configuration, IWebHostEnvironment environment)
    {
        root = Path.Combine(Path.GetFullPath(configuration["Storage:Root"] ?? environment.ContentRootPath), "submissions");
        Directory.CreateDirectory(root);
        foreach (var folder in Directory.GetDirectories(root))
        {
            string metadata = Path.Combine(folder, "submission.json");
            if (!File.Exists(metadata)) continue;
            try
            {
                var item = JsonSerializer.Deserialize<Submission>(File.ReadAllText(metadata));
                if (item != null && item.Id == Path.GetFileName(folder)) jobs[item.Id] = item;
            }
            catch (Exception error) when (error is IOException or JsonException) { }
        }
    }

    public IEnumerable<Submission> List(ClaimsPrincipal user, bool all = false) => jobs.Values
        .Where(item => all && user.IsInRole("TA") || item.OwnerUserId != null &&
            item.OwnerUserId == user.FindFirstValue(ClaimTypes.NameIdentifier))
        .OrderByDescending(item => item.UploadedAt);

    public Submission? Find(string id, ClaimsPrincipal user) => jobs.TryGetValue(id, out var item) &&
        (user.IsInRole("TA") || item.OwnerUserId != null &&
            item.OwnerUserId == user.FindFirstValue(ClaimTypes.NameIdentifier)) ? item : null;

    public string? DownloadPath(string id, string name, ClaimsPrincipal user)
    {
        var item = Find(id, user);
        if (item == null || name is not ("debug_ip.sv" or "debug_map.hpp" or "source.zip")) return null;
        if (name != "source.zip" && item.Status != "Complete") return null;
        string path = Path.Combine(root, item.Id, name);
        return File.Exists(path) ? path : null;
    }

    public async Task<Submission> UploadAsync(IFormFile file, StudentAccount user, CancellationToken ct)
    {
        if (!file.FileName.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Upload a .zip containing the Quartus project source files.");
        if (file.Length == 0 || file.Length > 20L * 1024 * 1024)
            throw new InvalidDataException("ZIP must be between 1 byte and 20 MB. Exclude build output.");
        string id = Guid.NewGuid().ToString("N");
        string folder = Path.Combine(root, id);
        Directory.CreateDirectory(folder);
        var item = new Submission(id, user.UserName!, DateTimeOffset.UtcNow, "Processing", 0, 0, "", "",
            user.Id, $"{user.FirstName} {user.LastName}");
        Save(item);
        try
        {
            await using (var source = file.OpenReadStream())
            await using (var destination = File.Create(Path.Combine(folder, "source.zip")))
                await source.CopyToAsync(destination, ct);
            await using var stream = File.OpenRead(Path.Combine(folder, "source.zip"));
            var result = await new QuartusProcessingService().ProcessAsync(stream, ct);
            await File.WriteAllTextAsync(Path.Combine(folder, "debug_ip.sv"), result.Verilog, ct);
            await File.WriteAllTextAsync(Path.Combine(folder, "debug_map.hpp"), result.CppHeader, ct);
            item = item with { Status = "Complete", Signals = result.SignalCount, Bits = result.TotalBits, TopLevel = result.TopLevel };
        }
        catch (Exception error) when (error is InvalidDataException or NotSupportedException or IOException)
        { item = item with { Status = "Failed", Error = error.Message }; }
        Save(item);
        return item;
    }

    private void Save(Submission item)
    {
        string path = Path.Combine(root, item.Id, "submission.json");
        File.WriteAllText(path + ".tmp", JsonSerializer.Serialize(item));
        File.Move(path + ".tmp", path, overwrite: true);
        jobs[item.Id] = item;
    }
}

public sealed record Submission(string Id, string Student, DateTimeOffset UploadedAt, string Status,
    int Signals, int Bits, string TopLevel, string Error, string? OwnerUserId = null, string? StudentName = null);
