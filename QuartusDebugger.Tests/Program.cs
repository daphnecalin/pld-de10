using System.IO.Compression;
using QuartusDebugger.Core;

int passed = 0;
void Check(bool condition, string message)
{
    if (!condition) throw new Exception("FAIL: " + message);
    Console.WriteLine("PASS: " + message);
    passed++;
}

var r = new DebugIpGenerator().Generate("top", new[]
{
    new SignalDefinition("clk", "input", 1),
    new SignalDefinition("data", "output", 5),
    new SignalDefinition("done", "output", 1)
});
Check(r.TotalBits == 7 && r.SignalCount == 3, "Generator sums widths");
Check(r.Verilog.Contains("probes[5:1] = sig_0001;") && r.Verilog.Contains("output logic [6:0] probes"), "Generated RTL slices");
Check(r.CppHeader.Contains("std::array<Signal, 3>") && r.CppHeader.Contains("{1, \"data\", Direction::Output, 5, 1, 5}"), "C++ map agrees with RTL");
Check(r.CppHeader.Contains("signals.at(id)") && r.CppHeader.Contains("try_get_signal"), "Checked and non-throwing C++ accessors");

string temp = Path.Combine(Path.GetTempPath(), "quartus-test-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temp);
try
{
    var archive = Path.Combine(temp, "test.qar");
    using (var zip = ZipFile.Open(archive, ZipArchiveMode.Create))
    {
        void Add(string path, string data)
        {
            var e = zip.CreateEntry(path);
            using var writer = new StreamWriter(e.Open());
            writer.Write(data);
        }
        Add("demo.qpf", "PROJECT_REVISION = \"demo\"\n");
        Add("demo.qsf", "set_global_assignment -name TOP_LEVEL_ENTITY top\nset_global_assignment -name SYSTEMVERILOG_FILE rtl/top.sv\n");
        Add("rtl/top.sv", "module top(input logic clk, input logic [3:0] a, b, output logic [4:0] result); endmodule\n");
    }
    await using (var stream = File.OpenRead(archive))
    {
        var result = await new QuartusProcessingService().ProcessAsync(stream);
        Check(result.TotalBits == 14 && result.SignalCount == 4, "ZIP-QAR end-to-end processing");
        Check(result.Verilog.Contains("probes[13:9] = sig_0003;"), "Output width 5 correctly mapped");
        Check(result.CppHeader.Contains("top.result") && result.CppHeader.Contains("Direction::Output, 5, 9, 13"), "Pipeline produces hierarchical C++ header");
    }
    var nested = Path.Combine(temp, "nested.zip");
    using (var zip = ZipFile.Open(nested, ZipArchiveMode.Create))
    {
        void Add(string path, string data) { var e = zip.CreateEntry(path); using var w = new StreamWriter(e.Open()); w.Write(data); }
        Add("demo.qpf", "PROJECT_REVISION = \"demo\"\n");
        Add("demo.qsf", "set_global_assignment -name TOP_LEVEL_ENTITY top\nset_global_assignment -name VERILOG_FILE top.v\nset_global_assignment -name VERILOG_FILE child.v\n");
        Add("top.v", "module top(input wire clk, output wire ready); wire [3:0] bus_a; child u0(.clk(clk)); endmodule\n");
        Add("child.v", "module child(input wire clk); reg [7:0] count; wire valid; endmodule\n");
    }
    await using (var stream = File.OpenRead(nested))
    {
        var result = await new QuartusProcessingService().ProcessAsync(stream);
        Check(result.SignalCount == 6 && result.TotalBits == 16, "Hierarchy parser includes child internal nets");
        Check(result.CppHeader.Contains("top.u0.count") && result.CppHeader.Contains("top.bus_a"), "Hierarchy names included in header");
        Check(result.Verilog.Contains("sig_0005") && result.Verilog.Contains("output logic [15:0] probes"), "Generated SV flattens hierarchy safely");
    }
    var bdf = Path.Combine(temp, "bdf.zip");
    using (var zip = ZipFile.Open(bdf, ZipArchiveMode.Create))
    {
        void Add(string path, string data) { var e = zip.CreateEntry(path); using var w = new StreamWriter(e.Open()); w.Write(data); }
        Add("demo.qpf", "PROJECT_REVISION = \"demo\"\n");
        Add("demo.qsf", "set_global_assignment -name TOP_LEVEL_ENTITY top\nset_global_assignment -name BDF_FILE top.bdf\nset_global_assignment -name BDF_FILE child.bdf\n");
        Add("top.bdf", "(header \"graphic\")\n(pin (input) (text \"INPUT\") (text \"clk\"))\n(pin (output) (text \"OUTPUT\") (text \"data[3..0]\"))\n(connector (text \"carry\") (pt 2 2) (pt 8 2))\n(symbol (rect 1 1 2 2) (text \"child\") (text \"u0\") (port (text \"SHOULD_NOT_COUNT\")))\n");
        Add("child.bdf", "(pin (input) (text \"INPUT\") (text \"a[7..0]\"))\n(connector (text \"temp[2..0]\") (pt 0 0) (pt 1 0))\n");
    }
    await using (var stream = File.OpenRead(bdf))
    {
        var result = await new QuartusProcessingService().ProcessAsync(stream);
        Check(result.SignalCount == 5 && result.TotalBits == 17, "BDF-only ZIP processes without Quartus");
        Check(result.CppHeader.Contains("top.u0.temp") && result.CppHeader.Contains("top.carry"), "BDF hierarchy and named connector discovery");
        Check(!result.CppHeader.Contains("SHOULD_NOT_COUNT"), "Nested symbol port labels not interpreted as independent nets");
    }
    var bad = Path.Combine(temp, "bad.zip");
    using (var zip = ZipFile.Open(bad, ZipArchiveMode.Create))
        zip.CreateEntry("../escape.txt");
    try
    {
        await new ArchiveExtractor().ExtractAsync(bad, Path.Combine(temp, "out"));
        throw new Exception("FAIL: Zip slip accepted");
    }
    catch (InvalidDataException)
    {
        Check(true, "Traversal path rejected");
    }
}
finally
{
    Directory.Delete(temp, true);
}
Console.WriteLine($"ALL {passed} TESTS PASSED");
