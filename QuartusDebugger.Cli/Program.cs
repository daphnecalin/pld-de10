using QuartusDebugger.Core;
if (args.Length != 2)
{
    Console.Error.WriteLine("Usage: dotnet run --project QuartusDebugger.Cli -- <archive.zip> <output-folder>");
    return 2;
}
try
{
    await using var input = File.OpenRead(args[0]);
    var result = await new QuartusProcessingService().ProcessAsync(input);
    Directory.CreateDirectory(args[1]);
    await File.WriteAllTextAsync(Path.Combine(args[1], "debug_ip.sv"), result.Verilog);
    await File.WriteAllTextAsync(Path.Combine(args[1], "debug_map.hpp"), result.CppHeader);
    Console.WriteLine($"SUCCESS: top={result.TopLevel}, signals={result.SignalCount}, bits={result.TotalBits}");
    Console.WriteLine($"Files saved to {Path.GetFullPath(args[1])}");
    return 0;
}
catch (Exception ex)
{
    Console.Error.WriteLine($"ERROR: {ex.Message}");
    return 1;
}
