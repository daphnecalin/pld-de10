# DE-10 Lite Debug Backpack Demo

This demo connects a DE-10 Lite RALU design to an RP2040 over SPI. The FPGA
sends captured signal values to the RP2040. The RP2040 can also override a
small set of RALU inputs and return them to the DE-10 Lite's physical inputs
when the override is cleared.

## Reading data from the FPGA

1. After a rising edge of `lab_clk`, the FPGA captures the debug probes and
   builds a 128-byte telemetry packet.
2. The FPGA raises `data_ready` and holds that packet until it receives a
   complete read.
3. The RP2040 polls `data_ready`, then clocks the packet out over SPI. It sends
   zero bytes on MOSI to provide the clocks and receives the packet on MISO.
4. A complete read lowers `data_ready`. If a read stops early, the FPGA keeps
   the packet so the RP2040 can retry it.

The SPI link uses mode 0 (CPOL 0, CPHA 0) at 1 MHz in the current RP2040
firmware.

### Telemetry packet

| Byte(s) | Contents |
| --- | --- |
| 0 | Start marker `0xA5` |
| 1 | Version `0x01` |
| 2-3 | 16-bit sample number, low byte first |
| 4-45 | 14 signal records: ID, width in bits, value |
| 46-126 | Reserved, set to zero |
| 127 | XOR checksum of bytes 0-126 |

The current signals are at most 4 bits wide, so each value takes one byte.
The [signal map](#demo-signal-map) lists the IDs and widths.

## Writing RALU inputs from the RP2040

Writes use a separate SPI transaction. The RP2040 sends an 11-byte command on
MOSI. The FPGA validates it, updates the requested override, then sets
`command_done` and places a result code on `command_status[2:0]`. The MISO data
during a write is ignored. A write does not replace or consume a pending
telemetry packet. `command_done` stays high until the next SPI transaction
starts.

| Byte | Contents |
| --- | --- |
| 0 | Command marker `0xC3` |
| 1 | Version `0x01` |
| 2 | Opcode: `0x01` set override, `0x02` clear override |
| 3 | Sequence byte (sent by the RP2040; not used for acknowledgement matching) |
| 4 | Signal ID |
| 5 | Signal width in bits |
| 6-9 | 32-bit value, least-significant byte first |
| 10 | XOR checksum of bytes 0-9 |

The FPGA only allows writes to the following demo inputs:

| ID | Input | Width |
| ---: | --- | ---: |
| 1 | `s_l` | 1 bit |
| 2 | `cin` | 1 bit |
| 4 | `input_data` | 4 bits |
| 5 | `msa` | 2 bits |
| 6 | `msb` | 2 bits |
| 7 | `msc` | 3 bits |

For each input, the wrapper uses the firmware value while its override is
enabled; otherwise it uses the DE-10 Lite input. Clock and reset cannot be
overridden. A validated change is transferred to the RALU's `lab_clk` domain
and takes effect there.

`command_status` uses these values:

| Value | Meaning |
| ---: | --- |
| 0 | No command result yet |
| 1 | Command accepted |
| 2 | Bad length, version, or checksum |
| 3 | Signal ID is not writable |
| 4 | Width does not match the signal |
| 5 | Value is too large for the signal |
| 6 | Unknown opcode |

## RP2040 firmware

The current firmware displays the latest RALU telemetry on its OLED. For
bring-up, enter commands over the Pico's USB serial connection:

```text
set 4 A
clear 4
```

Values are entered in hexadecimal. The first command sets `input_data` to
`0xA`; the second returns it to the DE-10 Lite input. Other writable IDs are
shown in the startup message. Physical menu buttons have not been assigned in
this demo.

Default Pico GPIO assignments are:

| Pico GPIO | Signal |
| ---: | --- |
| GP16 | SPI MISO |
| GP17 | SPI chip select (software controlled) |
| GP18 | SPI SCK |
| GP19 | SPI MOSI |
| GP20 | `data_ready` input |
| GP21 | `command_done` input |
| GP22-GP24 | `command_status[0:2]` inputs |

These are firmware defaults for the demo, not a finished PCB pinout. Route the
matching FPGA signals to these pins (or change the firmware definitions), and
share ground. Use compatible 3.3 V logic.

## Demo signal map

| ID | Signal | Width |
| ---: | --- | ---: |
| 0 | `lab_clk` | 1 bit |
| 1 | `s_l` | 1 bit |
| 2 | `cin` | 1 bit |
| 3 | `cout` | 1 bit |
| 4 | `input_data` | 4 bits |
| 5 | `msa` | 2 bits |
| 6 | `msb` | 2 bits |
| 7 | `msc` | 3 bits |
| 8 | `reg_a` | 4 bits |
| 9 | `reg_b` | 4 bits |
| 10 | `output_data` | 4 bits |
| 11 | `a_and_b` | 4 bits |
| 12 | `a_or_b` | 4 bits |
| 13 | `a_plus_b` | 4 bits |

## Simulation

The Questa testbench checks telemetry reads and retries, all writable input
IDs, command validation, writes while a telemetry sample is pending, and
clearing an override. See [the simulation guide](demo_sem_one/tb/README.md) for
the commands to compile and run it.

The testbench instantiates `debug_ip` and `spi_slave` directly. It checks the
SPI packet and override mapping, but does not model the RALU arithmetic or
verify the board-level pin wiring.

## RTL files

- [`debug_ip.sv`](demo_sem_one/rtl/debug_ip.sv) maps the demo signals to probe IDs.
- [`spi_slave.sv`](demo_sem_one/rtl/spi_slave.sv) captures telemetry and handles SPI reads and write commands.
- [`debug_demo.sv`](demo_sem_one/rtl/debug_demo.sv) connects the RALU, input overrides, debug IP, and SPI slave.
