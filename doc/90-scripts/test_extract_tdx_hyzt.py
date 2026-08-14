from __future__ import annotations

import unittest

import extract_tdx_hyzt as extractor


class HyztModelTests(unittest.TestCase):
    def test_builds_industry_stock_and_theme_indexes(self) -> None:
        member_text = "0|000001,1|600000"
        rows = [
            {
                "$ZQDM": "000001",
                "$SC": "0",
                "TDXHY": "银行",
                "$ZQDM1": "880471",
                "$SC1": "1",
                "hyPE": "5.2",
                "hyPB": "0.5",
                "$S_ZQDM": member_text,
                "sszt": "高股息、银行",
            },
            {
                "$ZQDM": "600000",
                "$SC": "1",
                "TDXHY": "银行",
                "$ZQDM1": "880471",
                "$SC1": "1",
                "hyPE": "5.2",
                "hyPB": "0.5",
                "$S_ZQDM": member_text,
                "sszt": "银行、国企改革",
            },
        ]
        model = extractor.build_model(rows)
        self.assertEqual(
            model["counts"],
            {
                "stocks": 2,
                "industry_blocks": 1,
                "industry_leaves": 1,
                "themes": 3,
                "theme_memberships": 4,
            },
        )
        self.assertEqual(
            model["industry_blocks"][0]["members"],
            ["SH600000", "SZ000001"],
        )
        themes = {
            item["name"]: item["members"]
            for item in model["themes"]
        }
        self.assertEqual(themes["银行"], ["SH600000", "SZ000001"])

    def test_rejects_declared_membership_mismatch(self) -> None:
        rows = [
            {
                "$ZQDM": "000001",
                "$SC": "0",
                "TDXHY": "银行",
                "$ZQDM1": "880471",
                "$SC1": "1",
                "hyPE": "5.2",
                "hyPB": "0.5",
                "$S_ZQDM": "0|000001,1|600000",
                "sszt": "银行",
            }
        ]
        with self.assertRaisesRegex(
            extractor.HyztFormatError,
            "行业成分不一致",
        ):
            extractor.build_model(rows)


if __name__ == "__main__":
    unittest.main()
