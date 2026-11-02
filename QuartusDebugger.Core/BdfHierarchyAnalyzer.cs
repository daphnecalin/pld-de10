using System.Text.RegularExpressions;

namespace QuartusDebugger.Core;

// Source-level discovery, not a Quartus netlist or electrical connectivity engine.
// Named wires and pin declarations are stable probe candidates; unnamed wires are not.
public sealed class BdfHierarchyAnalyzer
{
    private static readonly Regex TextPattern = new(@"\(text\s+""(?<name>(?:[^""]|"""")*)""", RegexOptions.Compiled);
    private static readonly Regex RangePattern = new(@"^(?<name>[A-Za-z_][A-Za-z_0-9$]*)\[(?<a>\d+)\.\.(?<b>\d+)\]$", RegexOptions.Compiled);
    private static readonly Regex BitPattern = new(@"^(?<name>[A-Za-z_][A-Za-z_0-9$]*)\[(?<index>\d+)\]$", RegexOptions.Compiled);
    private static readonly Regex Identifier = new(@"^[A-Za-z_][A-Za-z_0-9$]*$", RegexOptions.Compiled);

    // Keep source indices until coverage is resolved; width alone loses [7..6].
    private sealed record Candidate(SignalDefinition Signal, string BaseName,
        int? Left, int? Right, int? Index);

    public IReadOnlyList<SignalDefinition> Analyze(IEnumerable<string> bdfFiles, string topLevel)
    {
        var files = bdfFiles.ToDictionary(x => Path.GetFileNameWithoutExtension(x), x => x, StringComparer.OrdinalIgnoreCase);
        if (!files.ContainsKey(topLevel))
            throw new InvalidDataException($"BDF top-level '{topLevel}' not included in QSF BDF_FILE sources.");
        var candidates = new List<Candidate>();
        var seen = new Dictionary<string, int>(StringComparer.Ordinal);
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
                string baseName = name;
                int width = 1;
                int? left = null, right = null, index = null;
                var bus = RangePattern.Match(name);
                var bit = BitPattern.Match(name);
                if (bus.Success)
                {
                    baseName = name = bus.Groups["name"].Value;
                    left = ParseIndex(bus.Groups["a"].Value);
                    right = ParseIndex(bus.Groups["b"].Value);
                    long rangeWidth = Math.Abs((long)left.Value - right.Value) + 1;
                    if (rangeWidth > int.MaxValue)
                        throw new InvalidDataException($"BDF bus range too wide: '{rawName}'.");
                    width = (int)rangeWidth;
                }
                else if (bit.Success)
                {
                    baseName = bit.Groups["name"].Value;
                    index = ParseIndex(bit.Groups["index"].Value);
                    name = $"{baseName}[{index.Value}]";
                }
                else if (!Identifier.IsMatch(name))
                    throw new NotSupportedException($"Unsupported BDF net name '{rawName}' in {path}.");
                string fullName = prefix + "." + name;
                var candidate = new Candidate(new SignalDefinition(fullName, direction, width),
                    prefix + "." + baseName, left, right, index);
                if (!seen.TryGetValue(fullName, out int position))
                {
                    seen.Add(fullName, candidates.Count);
                    candidates.Add(candidate);
                }
                else
                {
                    var current = candidates[position];
                    // Equal widths do not imply equal source ranges or bit order.
                    if (current.Signal.Width != width || current.Left != left || current.Right != right)
                        throw new InvalidDataException($"Conflicting bus ranges or widths for '{fullName}'.");
                    // A connector label must not hide the direction of an I/O pin.
                    if (current.Signal.Direction == "internal" && direction != "internal")
                        candidates[position] = candidate;
                }
            }
        }
        Visit(topLevel, topLevel, new HashSet<string>(StringComparer.OrdinalIgnoreCase));
        var buses = candidates.Where(c => c.Left.HasValue)
            .ToDictionary(c => c.BaseName, StringComparer.Ordinal);
        // Resolve after discovery so bits preceding a bus are handled identically.
        // Fully qualified base names prevent coverage across instance boundaries.
        var output = candidates.Where(c => !(c.Index.HasValue &&
            buses.TryGetValue(c.BaseName, out var bus) &&
            c.Index.Value >= Math.Min(bus.Left!.Value, bus.Right!.Value) &&
            c.Index.Value <= Math.Max(bus.Left.Value, bus.Right.Value)))
            .Select(c => c.Signal).ToList();
        if (output.Count == 0) throw new InvalidDataException("No named nets or I/O pins discovered in BDF hierarchy.");
        return output;
    }

    private static int ParseIndex(string text)
    {
        if (!int.TryParse(text, out int index))
            throw new InvalidDataException($"BDF index '{text}' exceeds supported integer range.");
        return index;
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
