using System.Formats.Tar;
using System.IO.Compression;
using System.Text;
namespace QuartusDebugger.Core;

public sealed class ArchiveExtractor
{
    private const int MaxEntries = 10000;
    private const long MaxSize = 512L * 1024 * 1024;
    private const long MaxOne = 128L * 1024 * 1024;

    public async Task ExtractAsync(string archive, string destination, CancellationToken ct = default)
    {
        Directory.CreateDirectory(destination);
        await using var input = File.OpenRead(archive);
        var header = new byte[512];
        int n = await input.ReadAsync(header, ct);
        input.Position = 0;
        if (n >= 4 && header[0] == 0x50 && header[1] == 0x4b && header[2] is 0x03 or 0x05 or 0x07)
        {
            using var zip = new ZipArchive(input, ZipArchiveMode.Read, leaveOpen: true);
            await ExtractZip(zip, destination, ct);
        }
        else if (n >= 2 && header[0] == 0x1f && header[1] == 0x8b)
        {
            await using var gzip = new GZipStream(input, CompressionMode.Decompress, leaveOpen: true);
            await ExtractTar(gzip, destination, ct);
        }
        else if (n >= 262 && Encoding.ASCII.GetString(header, 257, 5) == "ustar")
            await ExtractTar(input, destination, ct);
        else
            throw new NotSupportedException("Unknown QAR format. Restore native QAR using Quartus 'quartus_sh --restore' first; this demo supports ZIP, TAR, and TAR.GZ containers.");
    }

    private static string SafePath(string root, string entry)
    {
        var normalized = entry.Replace('\\', '/');
        if (string.IsNullOrWhiteSpace(normalized) || normalized.StartsWith('/') || normalized.Contains(':') ||
            normalized.Split('/').Any(part => part is "." or ".."))
            throw new InvalidDataException($"Unsafe archive entry: {entry}");
        string basePath = Path.GetFullPath(root);
        string candidate = Path.GetFullPath(Path.Combine(basePath, normalized));
        if (!candidate.StartsWith(Path.TrimEndingDirectorySeparator(basePath) + Path.DirectorySeparatorChar,
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
            throw new InvalidDataException($"Entry escapes output directory: {entry}");
        return candidate;
    }
    private static async Task CopyBounded(Stream input, Stream output, long maximum, CancellationToken ct)
    {
        byte[] buffer = new byte[81920];
        long count = 0;
        int n;
        while ((n = await input.ReadAsync(buffer, ct)) != 0)
        {
            count += n;
            if (count > maximum) throw new InvalidDataException("Decompressed entry exceeds limit.");
            await output.WriteAsync(buffer.AsMemory(0, n), ct);
        }
    }
    private static async Task ExtractZip(ZipArchive zip, string root, CancellationToken ct)
    {
        if (zip.Entries.Count > MaxEntries) throw new InvalidDataException("Too many entries.");
        var seen = new HashSet<string>(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        long total = 0;
        foreach (var entry in zip.Entries)
        {
            ct.ThrowIfCancellationRequested();
            string path = SafePath(root, entry.FullName);
            if (!seen.Add(path)) throw new InvalidDataException("Duplicate archive entry.");
            int mode = (entry.ExternalAttributes >> 16) & 0xF000;
            if (mode != 0 && mode != 0x8000 && mode != 0x4000)
                throw new InvalidDataException("Unsupported ZIP file type (possibly a link).");
            if (entry.FullName.EndsWith('/')) { Directory.CreateDirectory(path); continue; }
            if (entry.Length > MaxOne || entry.Length > MaxSize - total) throw new InvalidDataException("Archive too large.");
            total += entry.Length;
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            await using var src = entry.Open();
            await using var dst = new FileStream(path, FileMode.CreateNew, FileAccess.Write);
            await CopyBounded(src, dst, Math.Min(entry.Length, MaxOne), ct);
        }
    }
    private static async Task ExtractTar(Stream stream, string root, CancellationToken ct)
    {
        using var tar = new TarReader(stream, leaveOpen: true);
        long total = 0; int entries = 0;
        var seen = new HashSet<string>(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        TarEntry? entry;
        while ((entry = tar.GetNextEntry()) != null)
        {
            ct.ThrowIfCancellationRequested();
            if (++entries > MaxEntries) throw new InvalidDataException("Too many entries.");
            var path = SafePath(root, entry.Name);
            if (!seen.Add(path)) throw new InvalidDataException("Duplicate archive entry.");
            if (entry.EntryType == TarEntryType.Directory) { Directory.CreateDirectory(path); continue; }
            if (entry.EntryType is not (TarEntryType.RegularFile or TarEntryType.V7RegularFile))
                throw new InvalidDataException("Links and special TAR entries are not allowed.");
            if (entry.Length > MaxOne || entry.Length > MaxSize - total) throw new InvalidDataException("Archive too large.");
            total += entry.Length;
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            await using var dst = new FileStream(path, FileMode.CreateNew, FileAccess.Write);
            if (entry.DataStream != null) await CopyBounded(entry.DataStream, dst, entry.Length, ct);
        }
    }
}
