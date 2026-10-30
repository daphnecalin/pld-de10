using System.Collections.Concurrent;
using QuartusDebugger.Core;

var builder = WebApplication.CreateBuilder(args);
builder.WebHost.UseWebRoot("wwwroot");
builder.Services.Configure<Microsoft.AspNetCore.Http.Features.FormOptions>(options =>
    options.MultipartBodyLengthLimit = 256L * 1024 * 1024);
var app = builder.Build();
app.UseDefaultFiles();
app.UseStaticFiles();

var storeRoot = Path.Combine(app.Environment.ContentRootPath, "submissions");
Directory.CreateDirectory(storeRoot);
var jobs = new ConcurrentDictionary<string, Submission>();
foreach (var folder in Directory.GetDirectories(storeRoot))
{
    string metadata = Path.Combine(folder, "submission.json");
    if (!File.Exists(metadata)) continue;
    try
    {
        var item = System.Text.Json.JsonSerializer.Deserialize<Submission>(File.ReadAllText(metadata));
        if (item != null) jobs[item.Id] = item;
    }
    catch { }
}

app.MapGet("/api/submissions", () => Results.Ok(jobs.Values.OrderByDescending(x => x.UploadedAt)));

app.MapPost("/api/submissions", async (HttpRequest request, CancellationToken ct) =>
{
    if (!request.HasFormContentType) return Results.BadRequest(new { error = "Use multipart/form-data." });
    var form = await request.ReadFormAsync(ct);
    var file = form.Files.GetFile("file");
    var student = form["student"].ToString().Trim();
    if (string.IsNullOrWhiteSpace(student) || student.Length > 120)
        return Results.BadRequest(new { error = "Student identifier is required (max 120 characters)." });
    if (file is null || !file.FileName.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
        return Results.BadRequest(new { error = "Upload a .zip containing the Quartus project source files." });
    if (file.Length == 0 || file.Length > 256L * 1024 * 1024)
        return Results.BadRequest(new { error = "ZIP must be between 1 byte and 256 MB." });

    var id = Guid.NewGuid().ToString("N");
    var folder = Path.Combine(storeRoot, id);
    Directory.CreateDirectory(folder);
    var item = new Submission(id, student, DateTimeOffset.UtcNow, "Processing", 0, 0, "", "");
    jobs[id] = item;
    Save(item);
    try
    {
        await using var stream = file.OpenReadStream();
        var result = await new QuartusProcessingService().ProcessAsync(stream, ct);
        await File.WriteAllTextAsync(Path.Combine(folder, "debug_ip.sv"), result.Verilog, ct);
        await File.WriteAllTextAsync(Path.Combine(folder, "debug_map.hpp"), result.CppHeader, ct);
        item = item with { Status = "Complete", Signals = result.SignalCount, Bits = result.TotalBits, TopLevel = result.TopLevel };
    }
    catch (Exception e) when (e is InvalidDataException or NotSupportedException or FileNotFoundException or IOException)
    {
        item = item with { Status = "Failed", Error = e.Message };
    }
    jobs[id] = item;
    Save(item);
    return Results.Ok(item);
});

app.MapGet("/api/submissions/{id}/{name}", (string id, string name) =>
{
    if (!jobs.TryGetValue(id, out var item) || item.Status != "Complete") return Results.NotFound();
    if (name is not ("debug_ip.sv" or "debug_map.hpp")) return Results.NotFound();
    var file = Path.Combine(storeRoot, id, name);
    if (!File.Exists(file)) return Results.NotFound();
    return Results.File(file, "application/octet-stream", name);
});

app.Run();

void Save(Submission s) => File.WriteAllText(Path.Combine(storeRoot, s.Id, "submission.json"),
    System.Text.Json.JsonSerializer.Serialize(s));

public sealed record Submission(string Id, string Student, DateTimeOffset UploadedAt, string Status,
    int Signals, int Bits, string TopLevel, string Error);
