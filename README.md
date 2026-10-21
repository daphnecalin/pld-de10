# pld-de10

## Debug capture flow

The FPGA samples the RALU debug probes after a rising edge of `lab_clk`, then
holds that snapshot in a packet until an SPI read completes.

```text
lab_clk rising edge
        |
        v
FPGA waits for its settle counter, snapshots 44 probe bits,
and increments the 16-bit capture count
        |
        v
data_ready goes high --------------------------+
        |                                       |
        | RP2040 GPIO interrupt                 | remains high
        v                                       | through the read
RP2040 prepares a 128-byte SPI transfer         |
(RX buffer and DMA; dummy TX bytes make clocks) |
        |                                       |
        v                                       |
SPI CS goes low; FPGA shifts packet on MISO     |
        |                                       |
        v                                       |
RP2040 DMA stores received bytes in SRAM        |
        |                                       |
        v                                       |
After all 128 bytes, SPI CS goes high; FPGA releases data_ready
        |
        v
RP2040 processes the packet from its receive buffer
```

### Step by step

1. The FPGA synchronizes `lab_clk` to its 50 MHz clock and detects a rising
   edge.
2. It waits for the settle counter to reach 4, snapshots the 44-bit `probes`
   bus, and increments the 16-bit capture count.
3. `data_ready` goes high when the FPGA is ready to serve that snapshot. It
   stays high throughout the SPI transfer and is released after a complete
   128-byte frame and the end of chip select.
4. Connect `data_ready` to an RP2040 GPIO configured for a rising-edge
   interrupt. The interrupt can arm/start the transfer or signal a task to do
   so. For reliable timing, configure the DMA and buffers ahead of time when
   practical.
5. The RP2040 is the SPI master. It asserts chip select and clocks 128 bytes
   in SPI mode 0 (CPOL=0, CPHA=0). The FPGA is the slave and sends the packet
   on MISO; MOSI is not used by this design. The RP2040 must transmit dummy
   bytes to generate the clocks while receiving.
6. The RP2040 SPI RX DMA channel can write those bytes directly into a
   128-byte buffer in RP2040 SRAM. The DMA destination is the buffer address
   supplied by the RP2040 program; there is no fixed memory address specified
   by the FPGA. Once DMA completes, software can validate and process the
   buffer.

The DMA/interrupt flow above describes how firmware can integrate with this
FPGA interface; this repository contains the FPGA RTL and simulation, not
RP2040 firmware or a board-level wiring configuration. Connect the SPI and
ready signals to suitable GPIOs, with a common ground and compatible I/O
levels.

## SPI packet format

The FPGA always transmits exactly 128 bytes, most-significant bit first within
each byte. Multi-byte fields are little-endian (least-significant byte first).
The 44 probe values are each represented as a whole byte, either `0x00` or
`0x01`.

| Byte offset | Length | Field | Description |
| --- | ---: | --- | --- |
| 0 | 1 | Start marker | `0xA5` |
| 1 | 1 | Protocol version | `0x01` |
| 2–3 | 2 | Capture count | Unsigned 16-bit count; byte 2 is low, byte 3 is high |
| 4–91 | 88 | Probe records | 44 pairs of `{signal_id, value}`; each pair is two bytes |
| 92–126 | 35 | Reserved padding | `0x00` |
| 127 | 1 | Checksum | XOR checksum described below |

Probe records are in ascending ID order. For signal ID `n`, byte `4 + 2*n`
contains the ID and byte `5 + 2*n` contains its 0-or-1 value. The checksum is
the XOR of `0xA5`, `0x01`, both capture-count bytes, and each probe record's ID
and value bytes. Padding bytes are zero.

### Debug IP signal ID map

Each ID identifies one bit of the 44-bit `probes` bus. For vector signals,
bits are listed from least significant to most significant. The narrower
`msa`, `msb`, and `msc` inputs are zero-extended in `debug_demo`; their
unused upper probe bits therefore read as zero.

| ID(s) | Signal bit(s) |
| --- | --- |
| 0 | `lab_clk` (debug IP input `clk`) |
| 1 | `s_l` |
| 2 | `cin` |
| 3 | `cout` |
| 4–7 | `input_data[0]` through `input_data[3]` |
| 8–11 | `msa[0]` through `msa[3]` (`msa[2:3]` are zero-extended) |
| 12–15 | `msb[0]` through `msb[3]` (`msb[2:3]` are zero-extended) |
| 16–19 | `msc[0]` through `msc[3]` (`msc[3]` is zero-extended) |
| 20–23 | `reg_a[0]` through `reg_a[3]` |
| 24–27 | `reg_b[0]` through `reg_b[3]` |
| 28–31 | `output_data[0]` through `output_data[3]` |
| 32–35 | `a_and_b[0]` through `a_and_b[3]` |
| 36–39 | `a_or_b[0]` through `a_or_b[3]` |
| 40–43 | `a_plus_b[0]` through `a_plus_b[3]` |

## RTL and simulation

- `debug_ip.sv` maps signals to probe IDs.
- `spi_slave.sv` captures probes and implements the packet and SPI interface.
- `debug_demo.sv` connects the RALU, debug IP, and SPI slave.
- `debug_tb.sv` simulates receiving and checking a complete 128-byte packet.
