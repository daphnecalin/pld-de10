"""Converter tests use synthetic data, not claimed RTL simulation results."""
import json
import tempfile
import unittest
from functools import reduce
from operator import xor
from pathlib import Path

from export_packets import WIDTHS, generate, read_fixture


def make_fixture(path, sample):
    frame = bytearray(128)
    frame[:4] = bytes((0xA5, 1, sample & 255, sample >> 8))
    lines = ["PLD_SPI_FIXTURE_V1", f"sample {sample}"]
    signals = []
    for i, width in enumerate(WIDTHS):
        value = (i + sample) % (1 << width)
        frame[4 + i * 3:7 + i * 3] = bytes((i, width, value))
        signals.append(f"signal {i} {width} {value}")
    frame[127] = reduce(xor, frame[:127], 0)
    lines += ["bytes " + frame.hex(" ")] + signals
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


class ExportTests(unittest.TestCase):
    def test_handoff_and_repeatability(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for i in range(1, 4):
                make_fixture(root / f"sample_{i}.txt", i)
            generate(root, root)
            first = (root / "debug_packets.h").read_bytes()
            generate(root, root)
            self.assertEqual(first, (root / "debug_packets.h").read_bytes())
            expected = json.loads((root / "expected_packets.json").read_text())
            self.assertEqual([x["sample_number"] for x in expected], [1, 2, 3])
            self.assertEqual(len(expected[0]["signals"]), 14)

    def test_corruption_and_incomplete_input_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sample_1.txt"
            make_fixture(path, 1)
            original = path.read_text()
            path.write_text(original.replace("bytes a5", "bytes a4"))
            with self.assertRaises(ValueError):
                read_fixture(path)
            # Keep marker valid and corrupt just the checksum.
            lines = original.splitlines()
            tokens = lines[2].split()
            tokens[-1] = f"{int(tokens[-1], 16) ^ 1:02x}"
            lines[2] = " ".join(tokens)
            path.write_text("\n".join(lines))
            with self.assertRaisesRegex(ValueError, "checksum"):
                read_fixture(path)
            path.write_text("\n".join(original.splitlines()[:-1]))
            with self.assertRaises(ValueError):
                read_fixture(path)

    def test_expectations_are_independent_of_packet_bytes(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sample_1.txt"
            make_fixture(path, 1)
            path.write_text(path.read_text().replace("signal 5 2 2", "signal 5 2 3"))
            with self.assertRaisesRegex(ValueError, "differs"):
                read_fixture(path)

    def test_sample_number_keeps_high_byte(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sample_1.txt"
            make_fixture(path, 256)
            expected, _ = read_fixture(path)
            self.assertEqual(expected["sample_number"], 256)


if __name__ == "__main__":
    unittest.main()
