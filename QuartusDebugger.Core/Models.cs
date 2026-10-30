namespace QuartusDebugger.Core;
public sealed record SignalDefinition(string Name, string Direction, int Width);
public sealed record DebugSignal(int Id, string Name, string Direction, int Width, int Lsb, int Msb);
public sealed record ProcessingResult(string TopLevel, string Verilog, string CppHeader, int SignalCount, int TotalBits);
public sealed record QuartusProject(string Revision, string TopLevel, IReadOnlyList<string> SourceFiles,
    IReadOnlyList<string> BdfFiles, string ProjectDirectory);
