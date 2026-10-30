# Quartus Debugger – Website / Direct BDF Source Analysis (No Quartus Required)

This is a .NET 8 ASP.NET Core website for uploading **source ZIPs**, assigning one student ID per upload, and producing `debug_ip.sv` and `debug_map.hpp`.

**No Quartus installation is required on the server.** The server parses the text of `.bdf` files directly. It does NOT call `quartus_map` and does not actually compile/elaborate the Quartus schematic. See limitations below.

## Student submission

Students ZIP the Quartus **source project**, maintaining relative paths:

```
student.zip
  lab.qpf
  lab.qsf
  top.bdf
  alu.bdf
  registers.bdf
  ... any referenced .bdf/.v/.sv/.bsf files
```

The `.qsf` must include `TOP_LEVEL_ENTITY` and a `BDF_FILE` assignment for **each** `.bdf` source. The ZIP must include exactly one `.qpf` with exactly one revision. Do not include `db/`, `incremental_db/`, `.sof` or output files. No `.qar` required.

## Run on Windows

1. Install [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0).
2. Extract the ZIP, open PowerShell inside `QuartusDebugger`.
3. Run tests:

```powershell
dotnet run --project .\QuartusDebugger.Tests
```

4. Start the website:

```powershell
dotnet run --project .\QuartusDebugger.Web --urls http://localhost:5080
```

5. Open `http://localhost:5080`, enter a student ID, select source ZIP and upload. The TA table gives downloadable files per submission.

To run CLI against your own source ZIP:

```powershell
dotnet run --project .\QuartusDebugger.Cli -- "C:\labs\student.zip" .\output
```

To try included source HDL example:

```powershell
dotnet run --project .\QuartusDebugger.Cli -- .\samples\demo.zip .\output
```

## What the direct BDF analyzer supports

- Top-level schematic `pin` records (input/output), including bus names `DATA[7..0]`.
- **Named** `connector` records, including bus ranges.
- Recursion through `symbol` records where the symbol type matches another submitted `.bdf` filename, collecting names like `top.alu0.carry`.
- Signal deduplication by hierarchical name; generated SystemVerilog uses safe aliases `sig_0000`, etc.
- C++17 `constexpr std::array` for metadata with bounds-checked `at()` and non-throwing `try_get_signal()`.

## Critical limitations

- **Not all internal signals are recovered.** In particular, **unnamed wires** are not represented. Collecting every electrical net requires interpreting connector coordinates, junctions, symbol port positions, rotations, pin offsets and bus tap semantics. Built-in symbol primitive outputs are not traversed as HDL signals.
- `.bdf` hierarchy is resolved by matching symbol type to `.bdf` filename. Custom `.bsf` hierarchy resolution and mixed BDF/HDL submodule traversal are not implemented.
- Only direct QSF assignment lines and simple BDF naming patterns are supported; the tool fails on unsupported names rather than producing an incorrect width. The parser does not validate electrical connectivity or prove a net is synthesizable.
- `debug_ip.sv` is an **unwired interface**. The fact that `debug_map.hpp` describes `top.u0.internal` does not make it accessible from the top without adding instrumentation/wiring. The outputs are metadata and a probe-packing module, not a drop-in working hardware trace design.
- Uploaded source is untrusted; the starter site has no authentication, per-user authorization, request throttling, job queue, or production isolation. Do not deploy publicly until these are added.
- For mixed .bdf/.sv designs with a BDF top, the direct analyzer does not yet recurse into HDL-only child instances.

## Architecture

`QuartusDebugger.Web`: ASP.NET Core minimal API + static HTML student/TA interface.

`QuartusDebugger.Core`: safe archive extraction, QPF/QSF lookup, direct BDF analyzer, basic Verilog hierarchy analyzer, debug IP + C++ map generator.

`QuartusDebugger.Tests`: checks generator, HDL hierarchy, BDF hierarchy, and ZIP path validation.

`QuartusDebugger.Cli`: local command-line inspection.

For real student projects, test with representative BDFs first. A truly complete extraction of every internal net requires expanding the geometric connectivity analyzer or using an elaboration tool; this package does not claim full coverage.
