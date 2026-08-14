from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import extract_tdx_blocks as extractor
import update_tdx_blocks as updater


class RootDetectionTests(unittest.TestCase):
    def test_validates_minimal_cache_layout(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "T0002" / "hq_cache"
            cache.mkdir(parents=True)
            for filename in (
                "tdxzs3.cfg",
                "tdxhy.cfg",
                "infoharbor_block.dat",
            ):
                (cache / filename).touch()

            self.assertTrue(updater.is_tdx_root(root))
            self.assertEqual(updater.find_tdx_root(root), root.resolve())

    def test_rejects_non_installation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(updater.UpdateError):
                updater.find_tdx_root(Path(temporary))


class AtomicWriteTests(unittest.TestCase):
    def test_replaces_existing_text_without_temporary_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "viewer.html"
            path.write_text("old", encoding="utf-8")

            updater.atomic_write_text(path, "新版板块", "utf-8")

            self.assertEqual(path.read_text(encoding="utf-8"), "新版板块")
            self.assertEqual(
                list(Path(temporary).glob(".viewer.html.*.tmp")),
                [],
            )

    @unittest.skipUnless(os.name == "nt", "Windows installation discovery")
    def test_current_installation_can_be_auto_detected(self) -> None:
        root = updater.find_tdx_root()
        self.assertTrue(updater.is_tdx_root(root))


class StableSnapshotTests(unittest.TestCase):
    def test_retries_a_temporarily_partial_source(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "T0002" / "hq_cache"
            cache.mkdir(parents=True)
            for path in updater.required_source_paths(
                root,
                set(extractor.FAMILY_CHOICES),
            ):
                path.touch()

            with patch.object(
                updater.extractor,
                "extract",
                side_effect=[
                    extractor.BlockFormatError("partial"),
                    ({}, [], []),
                ],
            ), patch.object(updater.time, "sleep"):
                _, blocks, members, attempts = (
                    updater.extract_stable_snapshot(
                        root,
                        set(extractor.FAMILY_CHOICES),
                        retries=3,
                    )
                )

            self.assertEqual(blocks, [])
            self.assertEqual(members, [])
            self.assertEqual(attempts, 2)


if __name__ == "__main__":
    unittest.main()
