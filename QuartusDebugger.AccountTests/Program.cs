using System.Diagnostics;
using System.IO.Compression;
using System.Net;
using System.Net.Http.Json;
using System.Net.Sockets;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

// Real HTTP + Razor form tests, with a disposable account database and uploads.
string root = Path.GetFullPath(args.FirstOrDefault() ?? Directory.GetCurrentDirectory());
string web = Path.Combine(root, "QuartusDebugger.Web");
string dll = Path.Combine(web, "bin", "Debug", "net8.0", "QuartusDebugger.Web.dll");
if (!File.Exists(dll)) throw new Exception("Build QuartusDebugger.Web first and run this test from the repository root.");
string temp = Path.Combine(root, "QuartusDebugger.AccountTests", "bin", "smoke-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temp);
var listener = new TcpListener(IPAddress.Loopback, 0);
listener.Start();
int port = ((IPEndPoint)listener.LocalEndpoint).Port;
listener.Stop();
string url = $"http://127.0.0.1:{port}";
int passed = 0;
Process? server = null;
var logs = new StringBuilder();
string password = "test-" + Guid.NewGuid().ToString("N");

void Check(bool condition, string label)
{
    if (!condition) throw new Exception("FAIL: " + label);
    Console.WriteLine("PASS: " + label);
    passed++;
}
ProcessStartInfo Info(params string[] commandArgs)
{
    var info = new ProcessStartInfo("dotnet") { WorkingDirectory = web, UseShellExecute = false,
        CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
    info.ArgumentList.Add(dll);
    foreach (string arg in commandArgs) info.ArgumentList.Add(arg);
    info.Environment["ASPNETCORE_ENVIRONMENT"] = "Development";
    info.Environment["ASPNETCORE_URLS"] = url;
    info.Environment["Storage__Root"] = temp;
    info.Environment["Logging__LogLevel__Default"] = "Warning";
    return info;
}
HttpClient Client() => new(new HttpClientHandler { CookieContainer = new CookieContainer(),
    AllowAutoRedirect = false, UseProxy = false }) { BaseAddress = new Uri(url), Timeout = TimeSpan.FromSeconds(30) };
async Task Start()
{
    server = new Process { StartInfo = Info() };
    server.OutputDataReceived += (_, e) => { if (e.Data != null) lock (logs) logs.AppendLine(e.Data); };
    server.ErrorDataReceived += (_, e) => { if (e.Data != null) lock (logs) logs.AppendLine(e.Data); };
    server.Start(); server.BeginOutputReadLine(); server.BeginErrorReadLine();
    using var probe = Client();
    string lastResponse = "No response";
    for (int attempt = 0; attempt < 150; attempt++)
    {
        if (server.HasExited) break;
        try
        {
            using var response = await probe.GetAsync("/Login");
            if (response.StatusCode == HttpStatusCode.OK) return;
            lastResponse = $"HTTP {(int)response.StatusCode}: " + await response.Content.ReadAsStringAsync();
        }
        catch (HttpRequestException error) { lastResponse = error.Message; }
        await Task.Delay(100);
    }
    throw new Exception("Server did not start: " + lastResponse + "\n" + logs);
}
void Stop()
{
    if (server is { HasExited: false }) { server.Kill(entireProcessTree: true); server.WaitForExit(); }
    server?.Dispose(); server = null;
}
async Task Role(string command)
{
    using var process = Process.Start(Info(command, "33333333"))!;
    var output = process.StandardOutput.ReadToEndAsync();
    var errors = process.StandardError.ReadToEndAsync();
    await process.WaitForExitAsync();
    Check(process.ExitCode == 0, command + " operator command: " + (process.ExitCode == 0 ? "success" : await errors));
    await output;
}
async Task<JsonNode> Json(HttpResponseMessage response) => JsonNode.Parse(await response.Content.ReadAsStringAsync())!;
async Task Token(HttpClient client)
{
    var data = await Json(await client.GetAsync("/api/auth/csrf"));
    client.DefaultRequestHeaders.Remove("X-CSRF-TOKEN");
    client.DefaultRequestHeaders.Add("X-CSRF-TOKEN", data["token"]!.GetValue<string>());
}
async Task<bool> Login(HttpClient client, string id)
{
    await Token(client);
    var response = await client.PostAsJsonAsync("/api/auth/login", new { studentId = id, password });
    await Token(client);
    return response.StatusCode == HttpStatusCode.OK;
}
async Task<FormUrlEncodedContent> Form(HttpClient client, string page, Dictionary<string, string> fields)
{
    string html = await client.GetStringAsync(page);
    var match = Regex.Match(html, "name=\"__RequestVerificationToken\"[^>]*value=\"([^\"]+)\"");
    if (!match.Success) throw new Exception("Missing HTML antiforgery field on " + page);
    fields["__RequestVerificationToken"] = WebUtility.HtmlDecode(match.Groups[1].Value);
    return new FormUrlEncodedContent(fields);
}
try
{
    string legacy = Path.Combine(temp, "submissions", "legacy");
    Directory.CreateDirectory(legacy);
    await File.WriteAllTextAsync(Path.Combine(legacy, "submission.json"),
        """{"Id":"legacy","Student":"11111111","UploadedAt":"2026-01-01T00:00:00Z","Status":"Complete","Signals":1,"Bits":1,"TopLevel":"legacy","Error":""}""");
    await File.WriteAllTextAsync(Path.Combine(legacy, "debug_map.hpp"), "// fixture");
    await Start();
    using var anonymous = Client(); using var alice = Client(); using var bob = Client(); using var ta = Client();
    Check((await anonymous.GetAsync("/")).StatusCode == HttpStatusCode.Redirect, "Anonymous page redirects to sign in");
    Check((await anonymous.GetAsync("/api/submissions")).StatusCode == HttpStatusCode.Unauthorized, "Anonymous API list is protected");
    Check((await anonymous.GetAsync("/api/submissions/legacy/debug_map.hpp")).StatusCode == HttpStatusCode.Unauthorized, "Anonymous downloads are protected");
    string loginHtml = await anonymous.GetStringAsync("/Login");
    Check(!loginHtml.Contains("<script", StringComparison.OrdinalIgnoreCase), "Login renders without JavaScript");
    Check((await anonymous.GetAsync("/Login")).Headers.GetValues("Content-Security-Policy")
        .Any(value => value.Contains("script-src 'none'")), "Browser policy explicitly disables JavaScript");
    var registration = new Dictionary<string, string> { ["StudentId"] = "11111111", ["FirstName"] = "Demo",
        ["LastName"] = "Alice", ["Password"] = password, ["ConfirmPassword"] = password, ["Role"] = "TA" };
    var registered = await alice.PostAsync("/Register", await Form(alice, "/Register", registration));
    Check(registered.StatusCode == HttpStatusCode.Redirect, "HTML registration form succeeds without JavaScript");
    var signedIn = await alice.PostAsync("/Login", await Form(alice, "/Login",
        new() { ["StudentId"] = "11111111", ["Password"] = password }));
    Check(signedIn.StatusCode == HttpStatusCode.Redirect, "HTML login form succeeds without JavaScript");
    Check(signedIn.Headers.GetValues("Set-Cookie").Any(cookie => cookie.Contains("httponly", StringComparison.OrdinalIgnoreCase)), "Login uses an HttpOnly cookie");
    Check((await Json(await alice.GetAsync("/api/auth/me")))["isTA"]!.GetValue<bool>() == false, "Registration cannot assign TA access");
    foreach (var (client, id) in new[] { (bob, "22222222"), (ta, "33333333") })
    {
        await Token(client);
        var response = await client.PostAsJsonAsync("/api/auth/register", new { studentId = id, firstName = "Demo",
            lastName = "Student", password, confirmPassword = password, role = "TA" });
        Check(response.IsSuccessStatusCode && await Login(client, id), "Additional student can register and sign in");
    }
    await Token(anonymous);
    foreach (var (student, secret, confirmation, label) in new[]
    {
        ("11111111", password, password, "Duplicate student IDs are rejected"),
        ("1234567", password, password, "Student IDs must contain exactly eight digits"),
        ("44444444", "short1", "short1", "Weak passwords are rejected"),
        ("44444444", password, "different", "Password confirmation must match")
    })
    {
        var rejected = await anonymous.PostAsJsonAsync("/api/auth/register", new { studentId = student,
            firstName = "Demo", lastName = "Student", password = secret, confirmPassword = confirmation });
        Check(rejected.StatusCode == HttpStatusCode.BadRequest, label);
    }
    var zeroAccount = await anonymous.PostAsJsonAsync("/api/auth/register", new { studentId = "00001234",
        firstName = "Leading", lastName = "Zero", password, confirmPassword = password });
    Check(zeroAccount.IsSuccessStatusCode && await Login(anonymous, "00001234") &&
        (await Json(await anonymous.GetAsync("/api/auth/me")))["studentId"]!.GetValue<string>() == "00001234",
        "Leading zeros in IDs survive registration and login");
    Check((await alice.GetAsync("/?all=true")).StatusCode == HttpStatusCode.Forbidden, "Student cannot open TA page mode");
    Check((await alice.GetAsync("/api/ta/submissions")).StatusCode == HttpStatusCode.Forbidden, "Student cannot list TA API data");
    Check((await alice.GetAsync("/Details/legacy")).StatusCode == HttpStatusCode.NotFound, "Typed ID does not claim legacy submissions");
    Check((await alice.PostAsync("/Logout", new FormUrlEncodedContent(new Dictionary<string, string>()))).StatusCode == HttpStatusCode.BadRequest,
        "HTML mutation rejects missing CSRF token");

    string dashboard = await alice.GetStringAsync("/");
    Check(!dashboard.Contains("<script", StringComparison.OrdinalIgnoreCase), "Student workspace renders without JavaScript");
    var tokenMatch = Regex.Match(dashboard, "name=\"__RequestVerificationToken\"[^>]*value=\"([^\"]+)\"");
    using var memory = new MemoryStream();
    using (var zip = new ZipArchive(memory, ZipArchiveMode.Create, leaveOpen: true))
    {
        void Add(string name, string data) { using var writer = new StreamWriter(zip.CreateEntry(name).Open()); writer.Write(data); }
        Add("demo.qpf", "PROJECT_REVISION = \"demo\"\n");
        Add("demo.qsf", "set_global_assignment -name TOP_LEVEL_ENTITY top\nset_global_assignment -name BDF_FILE top.bdf\n");
        Add("top.bdf", "(pin (input) (text \"INPUT\") (text \"clk\"))\n");
    }
    using var upload = new MultipartFormDataContent();
    upload.Add(new StringContent(WebUtility.HtmlDecode(tokenMatch.Groups[1].Value)), "__RequestVerificationToken");
    upload.Add(new StringContent("22222222"), "student");
    upload.Add(new ByteArrayContent(memory.ToArray()), "ProjectZip", "source.zip");
    var uploaded = await alice.PostAsync("/", upload);
    Check(uploaded.StatusCode == HttpStatusCode.Redirect && uploaded.Headers.Location!.ToString().StartsWith("/Details/"),
        "HTML upload processes and redirects to details without JavaScript");
    string detailsPath = uploaded.Headers.Location!.ToString();
    string idValue = detailsPath.Split('/').Last();
    var item = await Json(await alice.GetAsync("/api/submissions/" + idValue));
    Check(item["status"]!.GetValue<string>() == "Complete" && item["student"]!.GetValue<string>() == "11111111",
        "Processed upload belongs to authenticated user despite forged student field");
    Check((await Json(await alice.GetAsync("/api/submissions"))).AsArray().Count == 1, "Student sees own submission");
    Check((await Json(await bob.GetAsync("/api/submissions"))).AsArray().Count == 0, "Other student sees no unrelated submissions");
    Check((await bob.GetAsync(detailsPath)).StatusCode == HttpStatusCode.NotFound, "Other student cannot open HTML details");
    foreach (string name in new[] { "source.zip", "debug_ip.sv", "debug_map.hpp" })
    {
        Check((await alice.GetAsync($"/api/submissions/{idValue}/{name}")).IsSuccessStatusCode, "Owner can download " + name);
        Check((await bob.GetAsync($"/api/submissions/{idValue}/{name}")).StatusCode == HttpStatusCode.NotFound, "Other student cannot download " + name);
    }
    Stop(); await Role("--set-ta"); await Start();
    Check(await Login(alice, "11111111"), "Account persists after restart");
    Check((await Json(await alice.GetAsync("/api/submissions"))).AsArray().Count == 1, "Ownership persists after restart");
    Check(await Login(ta, "33333333") && (await Json(await ta.GetAsync("/api/auth/me")))["isTA"]!.GetValue<bool>(), "TA assignment takes effect at login");
    Check((await ta.GetAsync("/?all=true")).IsSuccessStatusCode, "TA can open all-submissions page");
    Check((await Json(await ta.GetAsync("/api/ta/submissions"))).AsArray().Count == 2, "TA can list owned and legacy submissions");
    Check((await ta.GetAsync(detailsPath)).IsSuccessStatusCode, "TA can open student details");
    Check((await ta.GetAsync($"/api/submissions/{idValue}/source.zip")).IsSuccessStatusCode, "TA can download student source ZIP");
    Check((await ta.GetAsync("/api/submissions/legacy/debug_map.hpp")).IsSuccessStatusCode, "TA can review legacy output");
    Stop(); await Role("--remove-ta"); await Start();
    Check((await ta.GetAsync("/api/auth/me")).StatusCode == HttpStatusCode.Unauthorized, "Role change invalidates existing session");
    Check(await Login(ta, "33333333") && (await ta.GetAsync("/api/ta/submissions")).StatusCode == HttpStatusCode.Forbidden,
        "Removed TA signs in with student permissions");
    alice.DefaultRequestHeaders.Remove("X-CSRF-TOKEN");
    var logout = await alice.PostAsync("/Logout", await Form(alice, "/", new()));
    Check(logout.StatusCode == HttpStatusCode.Redirect && (await alice.GetAsync("/api/submissions")).StatusCode == HttpStatusCode.Unauthorized,
        "HTML logout ends access without JavaScript");
    await Token(bob);
    for (int attempt = 0; attempt < 5; attempt++)
        await bob.PostAsJsonAsync("/api/auth/login", new { studentId = "22222222", password = "incorrect-password1" });
    Check(!await Login(bob, "22222222"), "Five failed passwords lock the account even for a subsequent correct password");
    Console.WriteLine($"ALL {passed} ACCOUNT CHECKS PASSED");
}
finally
{
    Stop();
    // Only remove this test's uniquely created directory under AccountTests/bin.
    Directory.Delete(temp, recursive: true);
}
