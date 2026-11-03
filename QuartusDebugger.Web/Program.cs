using System.Security.Claims;
using Microsoft.AspNetCore.Identity;
using QuartusDebugger.Web;

var builder = WebApplication.CreateBuilder(args);
AccountEndpoints.Configure(builder);
builder.Services.AddRazorPages(options => options.Conventions.AuthorizeFolder("/")
    .AllowAnonymousToPage("/Login").AllowAnonymousToPage("/Register"));
builder.Services.AddSingleton<SubmissionStore>();
builder.WebHost.ConfigureKestrel(options => options.Limits.MaxRequestBodySize = 21L * 1024 * 1024);
builder.Services.Configure<Microsoft.AspNetCore.Http.Features.FormOptions>(options =>
    options.MultipartBodyLengthLimit = 21L * 1024 * 1024);
var app = builder.Build();
if (await AccountEndpoints.InitializeAsync(app, args)) return;
app.UseStaticFiles();
AccountEndpoints.MapAccounts(app);
app.MapRazorPages();

app.MapGet("/api/submissions", (ClaimsPrincipal user, SubmissionStore store) => Results.Ok(store.List(user)))
    .RequireAuthorization();
app.MapGet("/api/ta/submissions", (ClaimsPrincipal user, SubmissionStore store) => Results.Ok(store.List(user, all: true)))
    .RequireAuthorization(policy => policy.RequireRole("TA"));
app.MapGet("/api/submissions/{id}", (string id, ClaimsPrincipal user, SubmissionStore store) =>
    store.Find(id, user) is { } item ? Results.Ok(item) : Results.NotFound()).RequireAuthorization();
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
app.MapGet("/api/submissions/{id}/{name}", (string id, string name, ClaimsPrincipal user, SubmissionStore store) =>
    store.DownloadPath(id, name, user) is { } path
        ? Results.File(path, "application/octet-stream", name) : Results.NotFound()).RequireAuthorization();
app.Run();
