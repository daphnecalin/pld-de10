using System.Text;

namespace QuartusDebugger.Core;
public sealed class DebugIpGenerator
{
    public ProcessingResult Generate(string top, IReadOnlyList<SignalDefinition> ports)
    {
        var mapped = new List<DebugSignal>(); int offset = 0;
        foreach (var p in ports)
        {
            if (p.Width < 1) throw new InvalidDataException("Width must be positive.");
            int msb = checked(offset + p.Width - 1);
            mapped.Add(new DebugSignal(mapped.Count, p.Name, p.Direction, p.Width, offset, msb));
            offset = checked(msb + 1);
        }
        if (offset == 0) throw new InvalidDataException("No ports.");
        var sb = new StringBuilder();
        sb.AppendLine("module debug_ip (");
        foreach (var s in mapped)
            sb.AppendLine($"    input logic {(s.Width == 1 ? "" : $"[{s.Width - 1}:0] ")}{Alias(s)},");
        sb.AppendLine($"    output logic [{offset - 1}:0] probes");
        sb.AppendLine(");").AppendLine("    always_comb begin").AppendLine("        probes = '0;");
        foreach (var s in mapped)
            sb.AppendLine($"        probes{(s.Width == 1 ? $"[{s.Lsb}]" : $"[{s.Msb}:{s.Lsb}]")} = {Alias(s)};");
        sb.AppendLine("    end").AppendLine("endmodule");
        string header = GenerateCppHeader(mapped, offset);
        return new ProcessingResult(top, sb.ToString(), header, mapped.Count, offset);
    }

    private static string Alias(DebugSignal signal) => "sig_" + signal.Id.ToString("D4");

    private static string GenerateCppHeader(IReadOnlyList<DebugSignal> signals, int totalBits)
    {
        if (signals.Count > ushort.MaxValue)
            throw new InvalidDataException("Too many signals for 16-bit packet IDs.");
        if (signals.Any(s => s.Width > ushort.MaxValue || s.Lsb > uint.MaxValue || s.Msb > uint.MaxValue))
            throw new InvalidDataException("Signal metadata exceeds header field limits.");

        var sb = new StringBuilder();
        sb.AppendLine("#pragma once")
          .AppendLine("#include <array>")
          .AppendLine("#include <cstddef>")
          .AppendLine("#include <cstdint>")
          .AppendLine("#include <stdexcept>")
          .AppendLine("#include <string_view>")
          .AppendLine()
          .AppendLine("namespace debug {")
          .AppendLine("enum class Direction : std::uint8_t { Input, Output, Internal };")
          .AppendLine("struct Signal {")
          .AppendLine("    std::uint16_t id;")
          .AppendLine("    std::string_view name;")
          .AppendLine("    Direction direction;")
          .AppendLine("    std::uint16_t width;")
          .AppendLine("    std::uint32_t lsb;")
          .AppendLine("    std::uint32_t msb;")
          .AppendLine("};")
          .AppendLine()
          .AppendLine($"inline constexpr std::array<Signal, {signals.Count}> signals = {{{{");
        foreach (var s in signals)
        {
            string direction = s.Direction.ToLowerInvariant() switch
            {
                "input" => "Input",
                "output" => "Output",
                "internal" => "Internal",
                _ => throw new InvalidDataException($"Unsupported signal direction: {s.Direction}")
            };
            // C++-escape arbitrary signal names, including control characters.
            string name = EscapeCppString(s.Name);
            sb.AppendLine($"    {{{s.Id}, \"{name}\", Direction::{direction}, {s.Width}, {s.Lsb}, {s.Msb}}},");
        }
        sb.AppendLine("}};")
          .AppendLine($"inline constexpr std::size_t signal_count = {signals.Count};")
          .AppendLine($"inline constexpr std::size_t total_probe_bits = {totalBits};")
          .AppendLine()
          .AppendLine("// Throws std::out_of_range when exceptions are enabled.")
          .AppendLine("constexpr const Signal& get_signal(std::size_t id) {")
          .AppendLine("    return signals.at(id);")
          .AppendLine("}")
          .AppendLine()
          .AppendLine("// Safe for firmware built without C++ exceptions.")
          .AppendLine("constexpr const Signal* try_get_signal(std::size_t id) noexcept {")
          .AppendLine("    return id < signals.size() ? &signals[id] : nullptr;")
          .AppendLine("}")
          .AppendLine("} // namespace debug");
        return sb.ToString();
    }

    private static string EscapeCppString(string value)
    {
        var sb = new StringBuilder();
        foreach (char c in value)
        {
            if (c == (char)92) sb.Append((char)92).Append((char)92);
            else if (c == (char)34) sb.Append((char)92).Append((char)34);
            else if (c == (char)10) sb.Append((char)92).Append('n');
            else if (c == (char)13) sb.Append((char)92).Append('r');
            else if (c == (char)9) sb.Append((char)92).Append('t');
            else if (char.IsControl(c))
                throw new InvalidDataException("Control character in signal name.");
            else sb.Append(c);
        }
        return sb.ToString();
    }
}
