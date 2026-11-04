using System.Security.Claims;
using System.Text.RegularExpressions;
using System.Threading.RateLimiting;
using Microsoft.AspNetCore.Antiforgery;
using Microsoft.AspNetCore.Identity;
using Microsoft.AspNetCore.RateLimiting;

namespace QuartusDebugger.Web;

public static class AccountEndpoints
{
    public static void Configure(WebApplicationBuilder builder)
    {
        Persistence.Configure(builder);
        builder.Services.AddIdentity<StudentAccount, IdentityRole>(options =>
        {
            options.Password.RequiredLength = 10;
            options.Password.RequireNonAlphanumeric = false;
            options.Password.RequireUppercase = false;
            options.Lockout.MaxFailedAccessAttempts = 5;
            options.Lockout.DefaultLockoutTimeSpan = TimeSpan.FromMinutes(5);
        }).AddEntityFrameworkStores<AccountDb>().AddDefaultTokenProviders();
        builder.Services.ConfigureApplicationCookie(options =>
        {
            options.Cookie.Name = "QuartusDebugger.Session";
            options.Cookie.HttpOnly = true;
            options.Cookie.SameSite = SameSiteMode.Lax;
            options.Cookie.SecurePolicy = builder.Environment.IsDevelopment()
                ? CookieSecurePolicy.SameAsRequest : CookieSecurePolicy.Always;
            options.ExpireTimeSpan = TimeSpan.FromHours(8);
            options.LoginPath = "/Login";
            options.Events.OnRedirectToLogin = context =>
            {
                if (context.Request.Path.StartsWithSegments("/api")) context.Response.StatusCode = 401;
                else context.Response.Redirect("/Login");
                return Task.CompletedTask;
            };
            options.Events.OnRedirectToAccessDenied = context => { context.Response.StatusCode = 403; return Task.CompletedTask; };
        });
        // Role assignment/removal invalidates existing sessions on the next request.
        builder.Services.Configure<SecurityStampValidatorOptions>(options => options.ValidationInterval = TimeSpan.Zero);
        builder.Services.AddAuthorization();
        builder.Services.AddAntiforgery(options => options.HeaderName = "X-CSRF-TOKEN");
        builder.Services.AddRateLimiter(options =>
        {
            options.RejectionStatusCode = 429;
            options.AddPolicy("accounts", context => HttpMethods.IsGet(context.Request.Method)
                ? RateLimitPartition.GetNoLimiter("read") : RateLimitPartition.GetFixedWindowLimiter(
                context.Connection.RemoteIpAddress?.ToString() ?? "unknown", _ => new FixedWindowRateLimiterOptions
                { PermitLimit = 20, Window = TimeSpan.FromMinutes(1), QueueLimit = 0 }));
            options.AddPolicy("uploads", context => HttpMethods.IsGet(context.Request.Method)
                ? RateLimitPartition.GetNoLimiter("read") : RateLimitPartition.GetFixedWindowLimiter(
                context.User.FindFirstValue(ClaimTypes.NameIdentifier) ?? "anonymous", _ => new FixedWindowRateLimiterOptions
                { PermitLimit = 10, Window = TimeSpan.FromMinutes(1), QueueLimit = 0 }));
        });
    }

    public static async Task<bool> InitializeAsync(WebApplication app, string[] args)
    {
        using var scope = app.Services.CreateScope();
        if (args.FirstOrDefault() is not ("--set-ta" or "--remove-ta"))
            await scope.ServiceProvider.GetRequiredService<SubmissionFiles>().CheckPrivateBucketAsync();
        var roles = scope.ServiceProvider.GetRequiredService<RoleManager<IdentityRole>>();
        if (!await roles.RoleExistsAsync("TA"))
        {
            var created = await roles.CreateAsync(new IdentityRole("TA"));
            if (!created.Succeeded) throw new InvalidOperationException("Could not initialize TA role.");
        }
        if (args.Length == 0 || args[0] is not ("--set-ta" or "--remove-ta")) return false;
        // Operator-only command; role changes are never exposed via registration/HTTP.
        if (args.Length != 2)
        { Console.Error.WriteLine("Usage: --set-ta <8-digit-ID> or --remove-ta <8-digit-ID>"); Environment.ExitCode = 2; return true; }
        var users = scope.ServiceProvider.GetRequiredService<UserManager<StudentAccount>>();
        var user = await users.FindByNameAsync(args[1]);
        if (user == null)
        { Console.Error.WriteLine("Account not found. Register first."); Environment.ExitCode = 2; return true; }
        bool grant = args[0] == "--set-ta";
        bool current = await users.IsInRoleAsync(user, "TA");
        var result = grant == current ? IdentityResult.Success : grant
            ? await users.AddToRoleAsync(user, "TA") : await users.RemoveFromRoleAsync(user, "TA");
        if (!result.Succeeded)
        { Console.Error.WriteLine("Role update failed."); Environment.ExitCode = 1; return true; }
        var stamp = await users.UpdateSecurityStampAsync(user);
        if (!stamp.Succeeded) throw new InvalidOperationException("Could not invalidate old sessions.");
        Console.WriteLine(grant ? "TA access assigned. Sign in again." : "TA access removed. Sign in again.");
        return true;
    }

    public static void MapAccounts(WebApplication app)
    {
        app.UseAuthentication();
        app.UseAuthorization();
        app.UseRateLimiter();
        app.Use(async (context, next) =>
        {
            context.Response.Headers.CacheControl = "no-store";
            context.Response.Headers["Content-Security-Policy"] =
                "default-src 'self'; script-src 'none'; style-src 'self'; form-action 'self'; base-uri 'self'; frame-ancestors 'none'";
            if (context.Request.Path.StartsWithSegments("/api") &&
                context.Request.Method is not ("GET" or "HEAD" or "OPTIONS"))
            {
                try { await context.RequestServices.GetRequiredService<IAntiforgery>().ValidateRequestAsync(context); }
                catch (AntiforgeryValidationException)
                { context.Response.StatusCode = 400; await context.Response.WriteAsJsonAsync(new { error = "Session check failed. Reload the page and try again." }); return; }
            }
            await next(context);
        });
        app.MapGet("/api/auth/csrf", (HttpContext context, IAntiforgery antiforgery) =>
            Results.Ok(new { token = antiforgery.GetAndStoreTokens(context).RequestToken }));
        app.MapPost("/api/auth/register", async (Registration input, UserManager<StudentAccount> users) =>
        {
            if (!ValidId(input.StudentId) || string.IsNullOrWhiteSpace(input.FirstName) ||
                string.IsNullOrWhiteSpace(input.LastName) || input.FirstName.Length > 80 || input.LastName.Length > 80)
                return Results.BadRequest(new { error = "Enter an eight-digit student ID and first and last names (up to 80 characters each)." });
            if (input.Password == null || input.Password.Length > 128 || input.Password != input.ConfirmPassword)
                return Results.BadRequest(new { error = "Passwords must match and be at most 128 characters." });
            var result = await users.CreateAsync(new StudentAccount
            { UserName = input.StudentId, FirstName = input.FirstName.Trim(), LastName = input.LastName.Trim() }, input.Password);
            return result.Succeeded ? Results.Ok(new { message = "Account created. Sign in to continue." })
                : Results.BadRequest(new { error = string.Join(" ", result.Errors.Select(error => error.Code == "DuplicateUserName"
                    ? "That student ID is already registered." : error.Description)) });
        }).RequireRateLimiting("accounts");
        app.MapPost("/api/auth/login", async (Login input, SignInManager<StudentAccount> signIn) =>
        {
            if (!ValidId(input.StudentId) || input.Password == null || input.Password.Length > 128)
                return Results.Json(new { error = "Unable to sign in. Check credentials or try again later." }, statusCode: 401);
            var result = await signIn.PasswordSignInAsync(input.StudentId!, input.Password, false, lockoutOnFailure: true);
            return result.Succeeded ? Results.Ok(new { message = "Signed in." })
                : Results.Json(new { error = "Unable to sign in. Check credentials or try again later." }, statusCode: 401);
        }).RequireRateLimiting("accounts");
        app.MapPost("/api/auth/logout", async (SignInManager<StudentAccount> signIn) =>
        { await signIn.SignOutAsync(); return Results.Ok(new { message = "Signed out." }); }).RequireAuthorization();
        app.MapGet("/api/auth/me", async (ClaimsPrincipal principal, UserManager<StudentAccount> users) =>
        {
            var user = await users.GetUserAsync(principal);
            return user == null ? Results.Unauthorized() : Results.Ok(new
            { studentId = user.UserName, user.FirstName, user.LastName, isTA = principal.IsInRole("TA") });
        }).RequireAuthorization();
    }

    private static bool ValidId(string? value) => Regex.IsMatch(value ?? "", @"^[0-9]{8}$");
}
