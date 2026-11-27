# Simulating SPI reads and writes

Run from the repository root.
The testbench covers telemetry reads and retries, firmware input writes,
command validation, and clearing overrides. It also checks that a write does
not replace a telemetry sample that is already pending. It can still export
the three telemetry fixtures from the original read regression.

## 1. Compile and run in the Questa Transcript window
If a simulation is already loaded, run `quit -sim` first to unload it without
closing Questa. Then run the whole block, including `vlog` and `vsim`, so you
use the updated source and enable export when loading the simulation:

```tcl
file mkdir demo_sem_one/tb/build
file mkdir uP/src/test

if {![file isdirectory demo_sem_one/tb/build/work]} {
    vlib demo_sem_one/tb/build/work
}
vlog -work demo_sem_one/tb/build/work -sv demo_sem_one/rtl/debug_ip.sv demo_sem_one/rtl/spi_slave.sv demo_sem_one/tb/debug_tb.sv

set captureDir [file normalize uP/src/test]
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
in `uP/src/test`. Save a copy elsewhere if you need an earlier run.

Require the final `PASS: telemetry reads, retries, writes, validation, pending
samples, and clear` message and no errors. If `+EXPORT_DIR` was supplied,
there should also be three `EXPORTED` messages, one for each fixture. For example:

```text
EXPORTED: <repository-root>/uP/src/test/sample_1.txt
```

## What the test covers

- Reads three telemetry packets and checks IDs, widths, values, padding,
  sample numbers, and XOR checksums.
- Aborts one read after eight bytes, then verifies the same pending sample can
  be read again.
- Writes every supported RALU input ID and checks the override values and
  enable bits.
- Captures telemetry after the writes and checks that it reports the
  overridden inputs.
- Rejects a bad checksum, unwritable ID, width mismatch, out-of-range value,
  and invalid opcode without changing the active override.
- Writes while a sample is pending, verifies that packet stays unchanged, and
  checks that a later sample sees the new value.
- Clears an override and checks that the physical input is used again.

The bench instantiates `debug_ip` and `spi_slave` directly. It checks the SPI
transport and override mapping, but does not model the actual RALU arithmetic
or confirm board-level GPIO wiring.

## 2. Package the verified captures

run:

```powershell
python demo_sem_one/tb/export_packets.py
```
This validates all three captures before writing:

- `debug_packets.h`: C++17 header with `pld_test::fixtures`. Each fixture has
  a `packet::Frame` (128 bytes), a 16-bit `sample_number`, and 14 expected
  `packet::Signal` values. The header includes `"../packet.h"` and reuses the
  firmware types rather than declaring separate signal/frame types.
- `expected_packets.json`: the same expectations with readable signal names.

Each `sample_N.txt` contains the actual SPI receive bytes and expectations from
the saved probe snapshot. The converter checks packet length, marker, version,
sample number, checksum, padding, IDs, widths, value ranges, and expected values.
Generated captures, headers, JSON, and simulator products are ignored by Git.
