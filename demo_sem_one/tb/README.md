# Exporting simulated SPI packets

Run from the repository root.
The original three-sample regression remains available without `+EXPORT_DIR`.
The control stimulus now zero-extends msa/msb/msc to their documented widths.

## 1. Compile and run in the Questa Transcript window
If a simulation is already loaded, run `quit -sim` first to unload it without
closing Questa. Then run the whole block, including `vlog` and `vsim`, so you
use the updated source and enable export when loading the simulation:

```tcl
file mkdir demo_sem_one/tb/build
file mkdir demo_sem_one/tb/generated

if {![file isdirectory demo_sem_one/tb/build/work]} {
    vlib demo_sem_one/tb/build/work
}
vlog -work demo_sem_one/tb/build/work -sv demo_sem_one/rtl/debug_ip.sv demo_sem_one/rtl/spi_slave.sv demo_sem_one/tb/debug_tb.sv

set captureDir [file normalize demo_sem_one/tb/generated]
puts "Packet output directory: $captureDir"
vsim -lib demo_sem_one/tb/build/work debug_tb "+EXPORT_DIR=$captureDir"
run -all
```

`file mkdir` creates the output directories; the testbench cannot create them.
`file normalize` makes the export path absolute. The library is kept in the
ignored build directory rather than your project's existing `work` library.
`+EXPORT_DIR` enables exporting; without it, the regression can pass without
creating files. `restart` alone does not add that argument to a loaded simulation.

For a new set of captures, unload with `quit -sim` and repeat the block.
Successful exports overwrite `sample_1.txt`, `sample_2.txt`, and `sample_3.txt`
in `demo_sem_one/tb/generated`. Save a copy elsewhere if you need an earlier run.

Require the final `PASS: all 3 samples, interrupted retry, and checksums` message
and no errors before packaging. Simulator exit status alone is insufficient.
There should be three `EXPORTED` messages, one for each of those files. For example:

```text
EXPORTED: <repository-root>/demo_sem_one/tb/generated/sample_1.txt
```

## 2. Package the verified captures

run:

```powershell
python demo_sem_one/tb/export_packets.py demo_sem_one/tb/generated
```
This validates all three captures before writing:

- `debug_packets.h`: C++17 header with `pld_test::fixtures`. Each fixture has
  a 128-byte `frame`, a 16-bit `sample_number`, and 14 expected `signals`.
- `expected_packets.json`: the same expectations with readable signal names.

Each `sample_N.txt` contains the actual SPI receive bytes and expectations from
the saved probe snapshot. The converter checks packet length, marker, version,
sample number, checksum, padding, IDs, widths, value ranges, and expected values.
Generated files and simulator products are ignored by Git;
