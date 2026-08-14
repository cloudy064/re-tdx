from __future__ import annotations

import unittest

import extract_tdx_blocks as blocks
import update_tdx_intraday_funds as funds


COLUMNS = [
    "code", "market", "xj", "zdf", "zd", "q5rjl",
    "jlr_1", "cje_1", "zlzb_1",
]
for suffix in (2, 3, 4, 5, 6, 7):
    COLUMNS.extend([
        f"jlr_{suffix}", f"cje_{suffix}", f"zlzb_{suffix}",
        f"zf_{suffix}", f"cjl_{suffix}",
    ])


def row(code: str, market: str, seed: int = 1) -> list[object]:
    values: dict[str, object] = {
        "code": code,
        "market": market,
        "xj": "10.00",
        "zdf": "2.00",
        "zd": "0.20",
        "q5rjl": "1000",
        "jlr_1": str(seed * 100),
        "cje_1": str(seed * 1000),
        "zlzb_1": "10.00",
    }
    for suffix in (2, 3, 4, 5, 6, 7):
        values.update({
            f"jlr_{suffix}": str(seed * suffix),
            f"cje_{suffix}": str(seed * suffix * 10),
            f"zlzb_{suffix}": "1.00",
            f"zf_{suffix}": "2.00",
            f"cjl_{suffix}": "5000",
        })
    return [values[name] for name in COLUMNS]


def response(rows: list[list[object]]) -> dict[str, object]:
    return {
        "ErrorCode": 0,
        "ResultSets": [{
            "ColDes": [{"Name": name} for name in COLUMNS],
            "Content": rows,
        }],
    }


class IntradayFundTests(unittest.TestCase):
    def setUp(self) -> None:
        self.master = {
            (1, "881001"): blocks.Security(1, "SH", "上海", "881001", "煤炭"),
            (1, "600001"): blocks.Security(1, "SH", "上海", "600001", "测试一"),
            (0, "000002"): blocks.Security(0, "SZ", "深圳", "000002", "测试二"),
        }

    def test_period_mapping_and_detail_volume_units(self) -> None:
        record = dict(zip(COLUMNS, row("600001", "1")))
        normalized = funds.normalize_record(record, self.master, detail=True)
        self.assertEqual(normalized["periods"]["before-09:35"]["name"], "9:35之前")
        self.assertEqual(normalized["periods"]["before-10:30"]["net_main_inflow"], "2")
        self.assertEqual(normalized["periods"]["before-09:35"]["relative_volume"], 100.0)

    def test_builds_industry_and_security_reverse_indexes(self) -> None:
        model = funds.build_model(
            response([row("881001", "1")]),
            {("1", "881001"): response([
                row("600001", "1"),
                row("000002", "0", 2),
            ])},
            self.master,
            generated_at="2026-08-01T12:00:00+08:00",
        )
        self.assertEqual(model["counts"], {
            "market_rows": 1,
            "available_industries": 1,
            "expanded_industries": 1,
            "component_records": 2,
            "securities": 2,
            "multi_industry_securities": 0,
        })
        self.assertEqual(model["industries"][0]["name"], "煤炭")
        self.assertEqual(model["securities"][0]["matches"][0]["industry_code"], "881001")

    def test_selects_bare_or_market_qualified_industry(self) -> None:
        available = [("1", "881001"), ("1", "881006")]
        self.assertEqual(
            funds.parse_requested_industries(["881001", "1:881006"], available),
            available,
        )
        with self.assertRaises(funds.IntradayFundError):
            funds.parse_requested_industries(["881999"], available)

    def test_rejects_bad_row_width(self) -> None:
        with self.assertRaises(funds.IntradayFundError):
            funds.response_rows({
                "ErrorCode": 0,
                "ResultSets": [{
                    "ColDes": [{"Name": "code"}, {"Name": "market"}],
                    "Content": [["600001"]],
                }],
            })


if __name__ == "__main__":
    unittest.main()
