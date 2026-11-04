using System.Security.Claims;
using Microsoft.AspNetCore.Identity;
using QuartusDebugger.Web;

var builder = WebApplication.CreateBuilder(args);
AccountEndpoints.Configure(builder);
builder.Services.AddRazorPages(options => options.Conventions.AuthorizeFolder("/")
    .AllowAnonymousToPage("/Login").AllowAnonymousToPage("/Register"));
builder.Services.AddScoped<SubmissionStore>();
builder.WebHost.ConfigureKestrel(options => options.Limits.MaxRequestBodySize = 21L * 1024 * 1024);
builder.Services.Configure<Microsoft.AspNetCore.Http.Features.FormOptions>(options =>
    options.MultipartBodyLengthLimit = 21L * 1024 * 1024);
var app = builder.Build();
if (await AccountEndpoints.InitializeAsync(app, args)) return;
app.UseStaticFiles();
AccountEndpoints.MapAccounts(app);
app.MapRazorPages();

app.MapGet("/api/submissions", async (ClaimsPrincipal user, SubmissionStore store, CancellationToken ct) =>
    Results.Ok(await store.ListAsync(user, ct: ct)))
    .RequireAuthorization();
app.MapGet("/api/ta/submissions", async (ClaimsPrincipal user, SubmissionStore store, CancellationToken ct) =>
    Results.Ok(await store.ListAsync(user, all: true, ct: ct)))
    .RequireAuthorization(policy => policy.RequireRole("TA"));
app.MapGet("/api/submissions/{id}", async (string id, ClaimsPrincipal user, SubmissionStore store, CancellationToken ct) =>
    await store.FindAsync(id, user, ct) is { } item ? Results.Ok(item) : Results.NotFound()).RequireAuthorization();
app.MapPost("/api/submissions", async (HttpRequest request, ClaimsPrincipal principal,
    UserManager<StudentAccount> users, SubmissionStore store, CancellationToken ct) =>
{
    var user = await users.GetUserAsync(principal);
    if (user == null) return Results.Unauthorized();
    if (!request.HasFormContentType) return Results.BadRequest(new { error = "Use multipart/form-data." });
    var file = (await request.ReadFormAsync(ct)).Files.GetFile("file");
    if (file == null) return Results.BadRequest(new { error = "Choose a project ZIP." });
    try { return Results.Ok(await store.UploadAsync(file, user, ct)); }
    catch (InvalidDataException error) { return Results.BadRequest(new { error = error.Message }); }
}).RequireAuthorization().RequireRateLimiting("uploads");
app.MapGet("/api/submissions/{id}/{name}", async (string id, string name, ClaimsPrincipal user, SubmissionStore store, CancellationToken ct) =>
    await store.DownloadAsync(id, name, user, ct) is { } stream
        ? Results.File(stream, "application/octet-stream", name) : Results.NotFound()).RequireAuthorization();
app.Run();
