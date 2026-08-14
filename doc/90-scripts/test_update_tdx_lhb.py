from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import update_tdx_lhb as lhb


def write_jsn(path: Path, headers: list[str], rows: list[list[object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    value = [{"colheader": headers, "data": rows}]
    path.write_bytes(json.dumps(value, ensure_ascii=False).encode("gb18030"))


class LhbUpdateTests(unittest.TestCase):
    def test_aggregates_views_by_event_id_and_selects_stock(self) -> None:
        views = (
            lhb.LhbView("全部", "list/a.jsn"),
            lhb.LhbView("机构", "list/b.jsn"),
        )
        headers = ["$ZQDM1", "$SC1", "date", "lx", "$ZQDM"]
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            write_jsn(
                base / "list" / "a.jsn",
                headers,
                [["000001", "0", "20260801", "异动", "7"]],
            )
            write_jsn(
                base / "list" / "b.jsn",
                headers,
                [["000001", "0", "20260801", "异动", "7"]],
            )
            events = lhb.load_master_events(base, views)

        self.assertEqual(len(events), 1)
        self.assertEqual(events[0]["views"], ["全部", "机构"])
        self.assertEqual(
            lhb.select_events(events, [], "0", "000001", False),
            events,
        )

    def test_detail_requires_one_security_and_date(self) -> None:
        headers = [
            "$ZQDM", "sc", "date", "lb", "yyb", "yyb1", "yyb2",
            "bje", "sje", "jmr", "zb", "mrcgl1", "mrcgl3", "mrcgl5",
            "ygcb", "ygsy",
        ]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "7.jsn"
            write_jsn(
                path,
                headers,
                [
                    ["000001", "0", "20260801", "1", "A", "B", "1", "1", "0", "1", "1", "1", "1", "1", "", ""],
                    ["600000", "1", "20260801", "1", "B", "S", "1", "0", "1", "-1", "1", "1", "1", "1", "", ""],
                ],
            )
            with self.assertRaisesRegex(lhb.LhbUpdateError, "多个证券"):
                lhb.load_detail(path)


if __name__ == "__main__":
    unittest.main()
