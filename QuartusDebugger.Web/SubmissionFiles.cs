using System.Net;
using System.Net.Http.Headers;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace QuartusDebugger.Web;

public sealed class SubmissionFiles(HttpClient client, IConfiguration configuration)
{
    private readonly string bucket = configuration["Supabase:Bucket"] ?? "submissions";

    public static void Configure(WebApplicationBuilder builder)
    {
        if (!Uri.TryCreate(builder.Configuration["Supabase:Url"], UriKind.Absolute, out var url) ||
            url.Scheme != "https" || !url.Host.EndsWith(".supabase.co", StringComparison.OrdinalIgnoreCase) ||
            url.AbsolutePath != "/" || url.UserInfo.Length != 0 || url.Query.Length != 0 || url.Fragment.Length != 0)
            throw new InvalidOperationException("Set Supabase:Url to the HTTPS project URL (https://<ref>.supabase.co).");
        string? key = builder.Configuration["Supabase:SecretKey"];
        if (key == null || !key.StartsWith("sb_secret_", StringComparison.Ordinal))
            throw new InvalidOperationException("Set Supabase:SecretKey to a server-only Supabase secret key (sb_secret_...).");
        if (!Regex.IsMatch(builder.Configuration["Supabase:Bucket"] ?? "submissions", @"\A[a-z0-9][a-z0-9_-]{0,62}\z"))
            throw new InvalidOperationException("Supabase:Bucket must be a lowercase bucket ID (letters, digits, hyphens, or underscores).");
        builder.Services.AddHttpClient<SubmissionFiles>(client =>
        {
            client.BaseAddress = url;
            client.DefaultRequestHeaders.Add("apikey", key);
        }).ConfigurePrimaryHttpMessageHandler(() => new HttpClientHandler { AllowAutoRedirect = false })
            .RedactLoggedHeaders(new[] { "apikey" });
    }

    public async Task CheckPrivateBucketAsync()
    {
        using var response = await client.GetAsync($"storage/v1/bucket/{bucket}");
        if (!response.IsSuccessStatusCode)
            throw new InvalidOperationException($"Cannot read the Supabase bucket (HTTP {(int)response.StatusCode}). Check the project URL, secret key, and bucket ID.");
        using var payload = JsonDocument.Parse(await response.Content.ReadAsStringAsync());
        if (!payload.RootElement.TryGetProperty("public", out var isPublic) || isPublic.ValueKind != JsonValueKind.False)
            throw new InvalidOperationException("The Supabase submission bucket must be private.");
    }

    private string ObjectPath(string id, string name, bool authenticated = false) =>
        $"storage/v1/object/{(authenticated ? "authenticated/" : "")}{bucket}/{Uri.EscapeDataString(id)}/{Uri.EscapeDataString(name)}";

    public async Task PutAsync(string id, string name, Stream content, CancellationToken ct)
    {
        using var body = new StreamContent(content);
        body.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
        using var response = await client.PostAsync(ObjectPath(id, name), body, ct);
        response.EnsureSuccessStatusCode();
    }

    public async Task<Stream?> OpenAsync(string id, string name, CancellationToken ct)
    {
        try { return await client.GetStreamAsync(ObjectPath(id, name, authenticated: true), ct); }
        catch (HttpRequestException error) when (error.StatusCode == HttpStatusCode.NotFound) { return null; }
    }
}
