using System.Text.RegularExpressions;

namespace QuartusDebugger.Core;

// Source-level discovery, not a Quartus netlist or electrical connectivity engine.
// Named wires and pin declarations are stable probe candidates; unnamed wires are not.
public sealed class BdfHierarchyAnalyzer
{
    private static readonly Regex TextPattern = new(@"\(text\s+""(?<name>(?:[^""]|"""")*)""", RegexOptions.Compiled);
    private static readonly Regex RangePattern = new(@"^(?<name>[A-Za-z_][A-Za-z_0-9$]*)\[(?<a>\d+)\.\.(?<b>\d+)\]$", RegexOptions.Compiled);
    private static readonly Regex Identifier = new(@"^[A-Za-z_][A-Za-z_0-9$]*$", RegexOptions.Compiled);

    public IReadOnlyList<SignalDefinition> Analyze(IEnumerable<string> bdfFiles, string topLevel)
    {
        var files = bdfFiles.ToDictionary(x => Path.GetFileNameWithoutExtension(x), x => x, StringComparer.OrdinalIgnoreCase);
        if (!files.ContainsKey(topLevel))
            throw new InvalidDataException($"BDF top-level '{topLevel}' not included in QSF BDF_FILE sources.");
        var output = new List<SignalDefinition>();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        void Visit(string design, string prefix, HashSet<string> stack)
        {
            if (!files.TryGetValue(design, out string? path))
                throw new InvalidDataException($"Referenced BDF '{design}' was not submitted.");
            if (!stack.Add(design))
                throw new InvalidDataException($"Recursive BDF instance hierarchy at '{prefix}'.");
            string content = File.ReadAllText(path);
            foreach (var record in TopLevelRecords(content))
            {
                if (record.Kind == "pin")
                {
                    var direction = Regex.Match(record.Body, @"^\(pin\s*\((input|output|bidirectional)\)", RegexOptions.IgnoreCase);
                    if (!direction.Success) continue;
                    var texts = TextPattern.Matches(record.Body);
                    if (texts.Count < 2)
                        throw new InvalidDataException($"Pin without a signal name in '{path}'.");
                    Add(texts[1].Groups["name"].Value, direction.Groups[1].Value.ToLowerInvariant());
                }
                else if (record.Kind == "connector")
                {
                    var texts = TextPattern.Matches(record.Body);
                    if (texts.Count != 0)
                        Add(texts[0].Groups["name"].Value, "internal");
                }
                else if (record.Kind == "symbol")
                {
                    var texts = TextPattern.Matches(record.Body);
                    if (texts.Count < 2) continue;
                    string type = texts[0].Groups["name"].Value;
                    string instance = texts[1].Groups["name"].Value;
                    if (files.ContainsKey(type))
                    {
                        if (!Identifier.IsMatch(instance))
                            throw new NotSupportedException($"Unsupported BDF instance name '{instance}'.");
                        Visit(type, prefix + "." + instance, stack);
                    }
                }
            }
            stack.Remove(design);

            void Add(string rawName, string direction)
            {
                if (direction == "bidirectional")
                    throw new NotSupportedException($"Bidirectional pin '{rawName}' needs a probing policy.");
                string name = rawName.Trim();
                int width = 1;
                var bus = RangePattern.Match(name);
                if (bus.Success)
                {
                    name = bus.Groups["name"].Value;
                    width = checked(Math.Abs(int.Parse(bus.Groups["a"].Value) - int.Parse(bus.Groups["b"].Value)) + 1);
                }
                if (!Identifier.IsMatch(name))
                    throw new NotSupportedException($"Unsupported BDF net name '{rawName}' in {path}.");
                string fullName = prefix + "." + name;
                if (seen.Add(fullName)) output.Add(new SignalDefinition(fullName, direction, width));
                else
                {
                    var current = output.First(x => x.Name == fullName);
                    if (current.Width != width)
                        throw new InvalidDataException($"Conflicting bus widths for '{fullName}'.");
                }
            }
        }
        Visit(topLevel, topLevel, new HashSet<string>(StringComparer.OrdinalIgnoreCase));
        if (output.Count == 0) throw new InvalidDataException("No named nets or I/O pins discovered in BDF hierarchy.");
        return output;
    }

    // Parse balanced top-level S-expressions without matching nested (text)/(port) records.
    private static IEnumerable<(string Kind, string Body)> TopLevelRecords(string source)
    {
        int depth = 0, start = -1;
        bool inString = false, escaped = false;
        for (int i = 0; i < source.Length; i++)
        {
            char c = source[i];
            if (inString)
            {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') inString = false;
                continue;
            }
            if (c == '"') { inString = true; continue; }
            if (c == '(') { if (depth == 0) start = i; depth++; }
            else if (c == ')')
            {
                depth--;
                if (depth < 0) throw new InvalidDataException("Unbalanced BDF parentheses.");
                if (depth == 0 && start >= 0)
                {
                    string block = source[start..(i + 1)];
                    var type = Regex.Match(block, @"^\(\s*(\w+)");
                    if (type.Success) yield return (type.Groups[1].Value.ToLowerInvariant(), block);
                }
            }
        }
        if (depth != 0 || inString) throw new InvalidDataException("Unterminated BDF record.");
    }
}
