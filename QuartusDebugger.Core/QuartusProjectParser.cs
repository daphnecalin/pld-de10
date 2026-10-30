using System.Text.RegularExpressions;
namespace QuartusDebugger.Core;
public sealed class QuartusProjectParser
{
    private static readonly Regex Revision = new(@"^\s*PROJECT_REVISION\s*=\s*""?(?<value>[^""\s]+)""?", RegexOptions.Multiline | RegexOptions.IgnoreCase);
    private static readonly Regex Assignment = new(@"^\s*set_global_assignment\s+-name\s+(?<key>\w+)\s+(?:""(?<quoted>[^""]+)""|(?<plain>\S+))", RegexOptions.Multiline | RegexOptions.IgnoreCase);
    public QuartusProject Parse(string root)
    {
        var projects = Directory.GetFiles(root, "*.qpf", SearchOption.AllDirectories);
        if (projects.Length != 1) throw new InvalidDataException("Expected exactly one .qpf project.");
        string projectDir = Path.GetDirectoryName(projects[0])!;
        var revisions = Revision.Matches(File.ReadAllText(projects[0])).Select(m => m.Groups["value"].Value).Distinct().ToArray();
        if (revisions.Length != 1) throw new InvalidDataException("Expected exactly one project revision.");
        string revision = revisions[0];
        var qsf = Path.Combine(projectDir, revision + ".qsf");
        if (!File.Exists(qsf)) throw new FileNotFoundException("Revision QSF missing", qsf);
        string? top = null; var sources = new List<string>(); var bdfs = new List<string>();
        foreach (Match match in Assignment.Matches(File.ReadAllText(qsf)))
        {
            var key = match.Groups["key"].Value.ToUpperInvariant();
            var val = match.Groups["quoted"].Success ? match.Groups["quoted"].Value : match.Groups["plain"].Value;
            if (key == "TOP_LEVEL_ENTITY") top = val;
            else if (key is "VERILOG_FILE" or "SYSTEMVERILOG_FILE" or "BDF_FILE")
            {
                var path = Path.GetFullPath(Path.Combine(projectDir, val.Replace('/', Path.DirectorySeparatorChar)));
                string prefix = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root)) + Path.DirectorySeparatorChar;
                if (!path.StartsWith(prefix, OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
                    throw new InvalidDataException("QSF source escapes project root.");
                if (!File.Exists(path)) throw new FileNotFoundException("QSF source missing", path);
                (key == "BDF_FILE" ? bdfs : sources).Add(path);
            }
        }
        if (string.IsNullOrWhiteSpace(top)) throw new InvalidDataException("TOP_LEVEL_ENTITY not found in QSF.");
        if (sources.Count + bdfs.Count == 0)
            throw new NotSupportedException("No BDF or Verilog/SystemVerilog QSF sources found.");
        return new QuartusProject(revision, top, sources.Distinct().ToArray(), bdfs.Distinct().ToArray(), projectDir);
    }
}
