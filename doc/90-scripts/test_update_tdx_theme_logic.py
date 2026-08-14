from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import update_tdx_theme_logic as themes


def write_jsn(path: Path, headers: list[str], rows: list[list[object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    value = [{"colheader": headers, "data": rows}]
    path.write_bytes(json.dumps(value, ensure_ascii=False).encode("gb18030"))


class ThemeLogicTests(unittest.TestCase):
    def test_parses_spaced_member_list(self) -> None:
        self.assertEqual(
            themes.parse_members("0 |000001,1|600000,0 |000001"),
            ((0, "000001"), (1, "600000")),
        )

    def test_rejects_declared_member_count_mismatch(self) -> None:
        category = themes.ThemeCategory("Z01", "测试", "test", "list/test.jsn")
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "test.jsn"
            write_jsn(
                path,
                ["gname", "$S_ZQDM", "S_NUM", "$ZQDM"],
                [["主题", "0|000001", "2", "7"]],
            )
            with self.assertRaisesRegex(themes.ThemeLogicError, "声明 2"):
                themes.load_master(path, category)

    def test_preserves_declared_count_but_deduplicates_master_members(self) -> None:
        category = themes.ThemeCategory("Z01", "测试", "test", "list/test.jsn")
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "test.jsn"
            write_jsn(
                path,
                ["gname", "$S_ZQDM", "S_NUM", "$ZQDM"],
                [["主题", "0|000001,0|000001", "2", "7"]],
            )
            row = themes.load_master(path, category)[0]

        self.assertEqual(row["declared_member_count"], 2)
        self.assertEqual(row["duplicate_member_count"], 1)
        self.assertEqual(row["members"], ((0, "000001"),))

    def test_detail_supersedes_master_members_and_builds_reverse_index(self) -> None:
        category = themes.ThemeCategory("Z01", "测试", "test", "list/test.jsn")
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "tdx"
            cache = root / "T0002" / "hq_cache"
            cache.mkdir(parents=True)
            # Avoid constructing TNF fixtures; the model only requires the
            # security lookup contract, so replace it for this focused test.
            write_jsn(
                base / "downloads" / "list" / "test.jsn",
                ["gname", "$S_ZQDM", "S_NUM", "$ZQDM"],
                [["主题", "0|000001", "1", "7"]],
            )
            write_jsn(
                base / "downloads" / "zttzty" / "7.jsn",
                [
                    "$ZQDM", "$SC", "fqprice_d3", "fqprice_d5",
                    "fqprice_d20", "fqprice_d60", "price1", "tzlj", "xxsm",
                ],
                [
                    ["000001", "0", "1", "1", "1", "1", "1", "逻辑A", "说明A"],
                    ["920001", "2", "2", "2", "2", "2", "2", "逻辑B", "说明B"],
                ],
            )
            original = themes.blocks.load_security_master
            themes.blocks.load_security_master = lambda _path: {}
            try:
                model = themes.build_model(
                    root,
                    base / "downloads",
                    (category,),
                )
            finally:
                themes.blocks.load_security_master = original

        self.assertEqual(model["counts"]["themes"], 1)
        self.assertEqual(model["counts"]["detailed_records"], 2)
        self.assertEqual(model["themes"][0]["master_member_count"], 1)
        self.assertEqual(model["themes"][0]["member_count"], 2)
        self.assertEqual(model["themes"][0]["details"][1]["logic"], "逻辑B")
        self.assertEqual(len(model["stocks"]), 2)

    def test_selects_by_id_name_or_category(self) -> None:
        values = [
            {"theme_id": "7", "name": "七号主题", "categories": ["A"]},
            {"theme_id": "8", "name": "八号主题", "categories": ["B"]},
        ]
        self.assertEqual(len(themes.select_themes(values, ["7"], [], False)), 1)
        self.assertEqual(len(themes.select_themes(values, ["八号"], [], False)), 1)
        self.assertEqual(len(themes.select_themes(values, [], ["A"], False)), 1)
        self.assertEqual(len(themes.select_themes(values, [], [], True)), 2)


if __name__ == "__main__":
    unittest.main()
