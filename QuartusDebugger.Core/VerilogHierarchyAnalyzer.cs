using System.Text.RegularExpressions;

namespace QuartusDebugger.Core;

// Restricted parser for simple structural Verilog emitted by Quartus BDF conversion.
// Does not elaborate parameters, generate loops, preprocessor branches or interfaces.
public sealed class VerilogHierarchyAnalyzer
{
    private sealed record Module(string Name, List<SignalDefinition> Signals, List<(string Module, string Instance)> Instances);
    private static readonly Regex ModuleRegex = new(@"\bmodule\s+(?<name>[A-Za-z_]\w*)\b(?<body>.*?)\bendmodule\b", RegexOptions.Singleline);
    private static readonly Regex Declaration = new(@"\b(?<dir>input|output|inout|wire|reg|logic)\s+(?:(?:wire|reg|logic|signed|unsigned)\s+)*(?<range>\[\s*\d+\s*:\s*\d+\s*\]\s*)?(?<names>[A-Za-z_$][\w$]*(?:\s*,\s*[A-Za-z_$][\w$]*)*)\s*;", RegexOptions.Singleline);
    private static readonly Regex InstanceRegex = new(@"(?<![\w$])(?<module>[A-Za-z_]\w*)\s+(?<instance>[A-Za-z_]\w*)\s*\(", RegexOptions.Singleline);
    private static readonly Regex HeaderRegex = new(@"^\s*(?:#\s*\(.*?\)\s*)?\((?<ports>.*?)\)\s*;", RegexOptions.Singleline);
    private static readonly Regex WidthRegex = new(@"\[\s*(\d+)\s*:\s*(\d+)\s*\]");
    private static readonly HashSet<string> Keywords = new(StringComparer.OrdinalIgnoreCase)
    { "if", "while", "case", "for", "assign", "always", "module", "wire", "input", "output", "not", "and", "or", "xor", "buf" };

    public IReadOnlyList<SignalDefinition> Analyze(IEnumerable<string> files, string top)
    {
        var modules = new Dictionary<string, Module>(StringComparer.Ordinal);
        foreach (string file in files)
        {
            string source = File.ReadAllText(file);
            source = Regex.Replace(source, @"/\*.*?\*/|//[^\r\n]*", "", RegexOptions.Singleline);
            foreach (Match mm in ModuleRegex.Matches(source))
            {
                string name = mm.Groups["name"].Value;
                string body = mm.Groups["body"].Value;
                var signals = new List<SignalDefinition>();
                var header = HeaderRegex.Match(body);
                if (header.Success)
                {
                    string? direction = null; int width = 1;
                    foreach (string raw in header.Groups["ports"].Value.Split(','))
                    {
                        string part = raw.Trim(); if (part.Length == 0) continue;
                        var dir = Regex.Match(part, @"^(input|output|inout)\b");
                        if (dir.Success)
                        {
                            direction = dir.Value; width = 1;
                            part = part[dir.Length..].Trim();
                            part = Regex.Replace(part, @"^(?:(?:wire|logic|reg|signed|unsigned)\s+)*", "");
                            var range = WidthRegex.Match(part);
                            if (range.Success && range.Index == 0)
                            {
                                width = RangeWidth(range);
                                part = part[range.Length..].Trim();
                            }
                        }
                        if (direction != null && Regex.IsMatch(part, @"^[A-Za-z_]\w*$"))
                            signals.Add(new(part, direction, width));
                        else if (part.Length != 0 && (dir.Success || direction != null))
                            throw new NotSupportedException($"Unsupported ANSI port '{part}' in module {name}.");
                    }
                }
                // Parses non-ANSI ports, internal nets, registers, and plain Verilog declarations.
                foreach (Match dm in Declaration.Matches(body))
                {
                    string dir = dm.Groups["dir"].Value.ToLowerInvariant();
                    if (dir == "inout") throw new NotSupportedException("Inout ports are unsupported.");
                    int width = dm.Groups["range"].Success ? RangeWidth(WidthRegex.Match(dm.Groups["range"].Value)) : 1;
                    foreach (string item in dm.Groups["names"].Value.Split(','))
                        signals.Add(new(item.Trim(), dir is "input" or "output" ? dir : "internal", width));
                }
                var unique = signals.GroupBy(s => s.Name, StringComparer.Ordinal)
                    .Select(g => g.OrderBy(s => s.Direction == "internal" ? 1 : 0).First()).ToList();
                // Instance references are resolved against project modules, not a hard-coded primitive list.
                var instances = new List<(string, string)>();
                string withoutHeader = header.Success ? body[header.Length..] : body;
                foreach (Match im in InstanceRegex.Matches(withoutHeader))
                {
                    string child = im.Groups["module"].Value, inst = im.Groups["instance"].Value;
                    if (!Keywords.Contains(child)) instances.Add((child, inst));
                }
                modules[name] = new Module(name, unique, instances);
            }
        }
        if (!modules.ContainsKey(top)) throw new InvalidDataException($"Top-level module '{top}' missing after conversion.");
        var output = new List<SignalDefinition>();
        void Visit(string moduleName, string path, HashSet<string> active)
        {
            if (!modules.TryGetValue(moduleName, out var module))
                throw new NotSupportedException($"Unresolved module '{moduleName}' at '{path}'. Built-in primitives or IP may require manual exclusions.");
            if (!active.Add(moduleName)) throw new InvalidDataException("Recursive module hierarchy: " + moduleName);
            foreach (var s in module.Signals)
            {
                if (s.Direction == "inout") throw new NotSupportedException("Inout signals unsupported.");
                if (s.Width <= 0 || s.Width > 4096) throw new InvalidDataException("Invalid signal width.");
                // Unique flattening: names here are debug-module input names, and path remains in C++ map.
                output.Add(new SignalDefinition(path + "." + s.Name,
                    path == top ? s.Direction : "internal", s.Width));
            }
            foreach (var (child, instance) in module.Instances)
                Visit(child, path + "." + instance, active);
            active.Remove(moduleName);
        }
        Visit(top, top, new HashSet<string>());
        if (output.Count == 0) throw new InvalidDataException("No signals found in hierarchy.");
        return output;
    }
    private static int RangeWidth(Match match)
    {
        if (!match.Success) return 1;
        return checked(Math.Abs(int.Parse(match.Groups[1].Value) - int.Parse(match.Groups[2].Value)) + 1);
    }
}
