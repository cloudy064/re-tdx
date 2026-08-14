from __future__ import annotations

import struct
import tempfile
import unittest
from pathlib import Path

import extract_tcalc_formulas as extractor


def synthetic_pe() -> bytes:
    data = bytearray(0x400)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"

    coff = 0x84
    struct.pack_into("<H", data, coff, 0x14C)
    struct.pack_into("<H", data, coff + 2, 1)
    struct.pack_into("<H", data, coff + 16, 0xE0)

    optional = coff + 20
    struct.pack_into("<H", data, optional, 0x10B)
    struct.pack_into("<I", data, optional + 28, 0x400000)
    struct.pack_into("<I", data, optional + 60, 0x200)

    section = optional + 0xE0
    data[section : section + 8] = b".rdata\0\0"
    struct.pack_into("<I", data, section + 8, 0x200)
    struct.pack_into("<I", data, section + 12, 0x1000)
    struct.pack_into("<I", data, section + 16, 0x200)
    struct.pack_into("<I", data, section + 20, 0x200)
    data[0x220:0x224] = b"TDX!"
    return bytes(data)


class PEImageTests(unittest.TestCase):
    def test_maps_rva_through_section_table(self) -> None:
        image = extractor.PEImage(synthetic_pe())
        self.assertEqual(image.image_base, 0x400000)
        self.assertEqual(image.rva_to_offset(0x1020, 4), 0x220)
        self.assertEqual(image.read_rva(0x1020, 4), b"TDX!")

    def test_rejects_unmapped_rva(self) -> None:
        image = extractor.PEImage(synthetic_pe())
        with self.assertRaises(extractor.TCalcFormatError):
            image.read_rva(0x3000, 1)


class MetadataTests(unittest.TestCase):
    def test_formula_source_precedence(self) -> None:
        self.assertEqual(extractor.formula_source(0x01), "system")
        self.assertEqual(extractor.formula_source(0x10), "temporary")
        self.assertEqual(extractor.formula_source(0x800), "default")
        self.assertEqual(extractor.formula_source(0), "user")

    def test_decodes_gb18030_fixed_string(self) -> None:
        data = "随机指标".encode("gb18030") + b"\0ignored"
        self.assertEqual(extractor.decode_fixed_string(data), "随机指标")


class CurrentTCalcIntegrationTests(unittest.TestCase):
    DLL = Path(__file__).resolve().parents[2] / "ida" / "TCalc.dll"

    @unittest.skipUnless(DLL.is_file(), "local ignored TCalc.dll is unavailable")
    def test_current_build_counts_and_categories(self) -> None:
        image = extractor.PEImage.from_path(self.DLL)
        digest = extractor.sha256_bytes(image.data)
        profile = extractor.profile_by_hash(digest)
        self.assertIsNotNone(profile)
        assert profile is not None

        categories, formulas = extractor.extract_formulas(
            image, profile, {0, 1, 2, 3}
        )
        counts = {
            kind: sum(item.kind == kind for item in formulas)
            for kind in range(4)
        }
        self.assertEqual(counts, {0: 222, 1: 107, 2: 15, 3: 35})
        self.assertEqual(formulas[0].code, "MA")
        self.assertEqual(formulas[0].name, "均线")
        self.assertEqual(categories[0][0].name, "大势型")
        self.assertEqual(categories[1][0].name, "指标条件")

    @unittest.skipUnless(DLL.is_file(), "local ignored TCalc.dll is unavailable")
    def test_cli_writes_utf8_csv(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "indicators.csv"
            result = extractor.main(
                [
                    "--dll",
                    str(self.DLL),
                    "--kind",
                    "technical",
                    "--format",
                    "csv",
                    "--output",
                    str(output),
                ]
            )
            data = output.read_bytes()

        self.assertEqual(result, 0)
        self.assertTrue(data.startswith(b"\xef\xbb\xbf"))
        self.assertIn("随机指标".encode("utf-8"), data)


if __name__ == "__main__":
    unittest.main()
