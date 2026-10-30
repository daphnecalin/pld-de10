namespace QuartusDebugger.Core;
public sealed class QuartusProcessingService
{
    public async Task<ProcessingResult> ProcessAsync(Stream archiveStream, CancellationToken ct = default)
    {
        string folder = Path.Combine(Path.GetTempPath(), "QuartusDebugger", Guid.NewGuid().ToString("N"));
        string archive = Path.Combine(folder, "input.zip"), project = Path.Combine(folder, "project");
        Directory.CreateDirectory(folder);
        try
        {
            await using (var dest = File.Create(archive))
            {
                byte[] buffer = new byte[81920]; long count = 0; int n;
                while ((n = await archiveStream.ReadAsync(buffer, ct)) != 0)
                {
                    count += n;
                    if (count > 256L * 1024 * 1024) throw new InvalidDataException("ZIP exceeds 256 MB upload limit.");
                    await dest.WriteAsync(buffer.AsMemory(0, n), ct);
                }
            }
            await new ArchiveExtractor().ExtractAsync(archive, project, ct);
            var metadata = new QuartusProjectParser().Parse(project);
            IReadOnlyList<SignalDefinition> signals;
            if (metadata.BdfFiles.Count > 0 &&
                metadata.BdfFiles.Any(f => Path.GetFileNameWithoutExtension(f).Equals(metadata.TopLevel, StringComparison.OrdinalIgnoreCase)))
            {
                // Direct schematic analysis. No Quartus executable required.
                signals = new BdfHierarchyAnalyzer().Analyze(metadata.BdfFiles, metadata.TopLevel);
            }
            else
            {
                if (metadata.SourceFiles.Count == 0)
                    throw new NotSupportedException("BDF top-level missing; include it in QSF BDF_FILE assignments.");
                signals = new VerilogHierarchyAnalyzer().Analyze(metadata.SourceFiles, metadata.TopLevel);
            }
            return new DebugIpGenerator().Generate(metadata.TopLevel, signals);
        }
        finally
        {
            try { Directory.Delete(folder, recursive: true); } catch (IOException) { } catch (UnauthorizedAccessException) { }
        }
    }
}
