"""Validate Questa packet captures and generate a firmware handoff (Python 3)."""

import argparse
import json
from functools import reduce
from operator import xor
from pathlib import Path

NAMES = (
    "lab_clk", "s_l", "cin", "cout", "input_data", "msa", "msb",
    "msc", "reg_a", "reg_b", "output_data", "a_and_b", "a_or_b", "a_plus_b",
)
WIDTHS = (1, 1, 1, 1, 4, 2, 2, 3, 4, 4, 4, 4, 4, 4)


def read_fixture(path):
    lines = path.read_text(encoding="ascii").splitlines()
    if len(lines) != 17 or lines[0] != "PLD_SPI_FIXTURE_V1":
        raise ValueError(f"{path}: incomplete or unsupported fixture")
    label, sample_text = lines[1].split()
    sample = int(sample_text)
    if label != "sample" or not 0 <= sample <= 65535:
        raise ValueError(f"{path}: invalid sample number")
    parts = lines[2].split()
    frame = bytes(int(token, 16) for token in parts[1:])
    if parts[0] != "bytes" or len(frame) != 128:
        raise ValueError(f"{path}: expected 128 received bytes")
    if frame[:2] != b"\xa5\x01" or int.from_bytes(frame[2:4], "little") != sample:
        raise ValueError(f"{path}: header/sample mismatch")
    if reduce(xor, frame, 0) != 0:
        raise ValueError(f"{path}: checksum mismatch")
    if any(frame[46:127]):
        raise ValueError(f"{path}: nonzero padding")
    signals = []
    for signal_id, (name, width, line) in enumerate(zip(NAMES, WIDTHS, lines[3:])):
        tag, id_text, width_text, value_text = line.split()
        value = int(value_text)
        if (tag, int(id_text), int(width_text)) != ("signal", signal_id, width):
            raise ValueError(f"{path}: unexpected signal ID/width")
        if not 0 <= value < (1 << width):
            raise ValueError(f"{path}: {name} exceeds its declared width")
        offset = 4 + 3 * signal_id
        if frame[offset:offset + 3] != bytes((signal_id, width, value)):
            raise ValueError(f"{path}: {name} differs from captured expectation")
        signals.append(dict(id=signal_id, name=name, width=width, value=value))
    return dict(source=path.name, sample_number=sample, signals=signals), frame


def generate(input_dir, output_dir):
    # This regression exports exactly three complete samples, not the aborted read.
    fixtures = [read_fixture(input_dir / f"sample_{i}.txt") for i in range(1, 4)]
    if [expected["sample_number"] for expected, _ in fixtures] != [1, 2, 3]:
        raise ValueError("expected samples 1, 2, 3 from one regression run")
    header = [
        "// Generated from verified simulated SPI reads; do not edit.",
        "#pragma once", "#include <array>", "#include <cstdint>",
        "namespace pld_test {",
        "struct Signal { std::uint8_t id, width; std::uint32_t value; };",
        "struct Fixture {",
        "    std::array<std::uint8_t, 128> frame;",
        "    std::uint16_t sample_number;",
        "    std::array<Signal, 14> signals;",
        "};",
        "inline constexpr std::array<Fixture, 3> fixtures = {{",
    ]
    for expected, frame in fixtures:
        header.append("    { {{")
        for start in range(0, 128, 16):
            header.append("        " + ", ".join(f"0x{b:02X}" for b in frame[start:start + 16]) + ",")
        header.append("    }}, " + str(expected["sample_number"]) + ", {{")
        for signal in expected["signals"]:
            header.append("        {%d, %d, %d}, // %s" % (
                signal["id"], signal["width"], signal["value"], signal["name"]))
        header.append("    }} },")
    header += ["}};", "} // namespace pld_test", ""]
    # Validate every input before writing either deliverable.
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "debug_packets.h").write_text("\n".join(header), encoding="ascii")
    (output_dir / "expected_packets.json").write_text(
        json.dumps([item[0] for item in fixtures], indent=2) + "\n", encoding="ascii")
    print(f"Validated and packaged {len(fixtures)} simulated packets in {output_dir}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_dir", type=Path)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    try:
        generate(args.input_dir, args.output_dir or args.input_dir)
    except (ValueError, OSError) as error:
        parser.exit(1, f"Packet export failed: {error}\n")
