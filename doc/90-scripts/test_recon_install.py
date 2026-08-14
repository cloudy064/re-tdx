from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import recon_install


class DumpbinParserTests(unittest.TestCase):
    def test_parse_exports(self) -> None:
        output = """
              ordinal hint RVA      name
                    1    0 00003F90 fn_TGetImageData
                    2    1 00003F40 fn_sync_getdata
        """
        exports = recon_install.parse_exports(output)
        self.assertEqual(
            [item.name for item in exports],
            ["fn_TGetImageData", "fn_sync_getdata"],
        )
        self.assertEqual(exports[0].rva, 0x3F90)

    def test_parse_dependencies_deduplicates_case_insensitively(self) -> None:
        output = """
            KERNEL32.dll
            WS2_32.dll
            kernel32.DLL
        """
        self.assertEqual(
            recon_install.parse_dependencies(output),
            ["KERNEL32.dll", "WS2_32.dll"],
        )


class InventoryTests(unittest.TestCase):
    def test_inventory_finds_aliases_without_reading_contents(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "SEPlugins").mkdir()
            (root / "TdxW.exe").write_bytes(b"exe")
            (root / "SEPlugins" / "TAsioComm.dll").write_bytes(b"dll")
            (root / "user.ini").write_text("secret=ignored", encoding="utf-8")

            artifacts, counts, directories = recon_install.inventory(root)

        self.assertEqual(counts["total_files"], 3)
        self.assertEqual(counts["dll_files"], 1)
        self.assertEqual(directories, ["SEPlugins"])
        self.assertEqual(
            {(item.group, item.path.name) for item in artifacts},
            {("main", "TdxW.exe"), ("network plugin", "TAsioComm.dll")},
        )


if __name__ == "__main__":
    unittest.main()
