using System.Text.RegularExpressions;
namespace QuartusDebugger.Core;
// A deliberately conservative ANSI-style parser, NOT a complete SystemVerilog elaborator.
public sealed class VerilogPortParser
{
    public IReadOnlyList<SignalDefinition> Parse(IEnumerable<string> files, string top)
    {
        foreach (var file in files)
        {
            var source = Regex.Replace(File.ReadAllText(file), @"/\*.*?\*/|//[^\r\n]*", "", RegexOptions.Singleline);
            // Numeric ANSI headers, optionally with a simple parameter list (parameter use in widths is unsupported).
            var regex = new Regex(@"\bmodule\s+" + Regex.Escape(top) + @"\s*(?:#\s*\(.*?\)\s*)?\((?<ports>.*?)\)\s*;", RegexOptions.Singleline);
            var match = regex.Match(source);
            if (!match.Success) continue;
            return ParsePorts(match.Groups["ports"].Value);
        }
        throw new InvalidDataException($"Top-level module '{top}' not found in QSF source files.");
    }
    private static IReadOnlyList<SignalDefinition> ParsePorts(string text)
    {
        var result = new List<SignalDefinition>();
        string? direction = null; int width = 1;
        foreach (var raw in text.Split(','))
        {
            string item = raw.Trim();
            if (item.Length == 0) continue;
            var match = Regex.Match(item, @"^(input|output|inout)\b", RegexOptions.IgnoreCase);
            if (match.Success)
            {
                direction = match.Value.ToLowerInvariant(); width = 1;
                item = item[match.Length..].Trim();
                item = Regex.Replace(item, @"^(wire|logic|reg)\b\s*", "", RegexOptions.IgnoreCase);
                var range = Regex.Match(item, @"^\[\s*(\d+)\s*:\s*(\d+)\s*\]\s*");
                if (range.Success)
                {
                    int a = int.Parse(range.Groups[1].Value), b = int.Parse(range.Groups[2].Value);
                    width = checked(Math.Abs(a - b) + 1);
                    item = item[range.Length..].Trim();
                }
            }
            if (direction is null) throw new NotSupportedException("Non-ANSI module header not supported.");
            if (direction == "inout") throw new NotSupportedException("Inout ports require a dedicated probing policy.");
            if (!Regex.IsMatch(item, @"^[a-zA-Z_][a-zA-Z_0-9$]*$"))
                throw new NotSupportedException($"Unsupported port syntax: '{item}'. Numeric ANSI declarations only.");
            if (result.Any(s => s.Name == item)) throw new InvalidDataException($"Duplicate port: {item}");
            result.Add(new SignalDefinition(item, direction, width));
        }
        if (result.Count == 0) throw new InvalidDataException("No supported ports found.");
        return result;
    }
}
