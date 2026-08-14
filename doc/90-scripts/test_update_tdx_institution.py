from __future__ import annotations

import unittest

import update_tdx_institution as institution


class InstitutionUpdateTests(unittest.TestCase):
    def test_builds_dynamic_detail_resource(self) -> None:
        self.assertEqual(
            institution.detail_resource(
                institution.HOLDERS_TEMPLATE, "1", "603221"
            ),
            "cgfxmx2/1603221.jsn",
        )

    def test_parses_holder_reference_and_variant(self) -> None:
        result = institution.parse_holder_reference(
            "http://page1.tdx.com.cn:7615/site/x.html?"
            "gdname=%E9%A6%99%E6%B8%AF%E4%B8%AD%E5%A4%AE%E7%BB%93%E7%AE%97&"
            "gdid=GD011907&gp=000063&tdxid=8000002"
        )
        self.assertEqual(result["holder_id"], "GD011907")
        self.assertEqual(result["holder_variant_id"], "8000002")
        self.assertEqual(result["holder_name"], "香港中央结算")
        self.assertEqual(
            institution.holder_key(
                result["holder_id"], result["holder_variant_id"]
            ),
            "GD011907:8000002",
        )

    def test_normalizes_holder_history(self) -> None:
        response = {
            "ResultSets": [
                {
                    "ColName": ["rq", "sc", "zqdm", "zqjc", "T006", "T009"],
                    "Content": [
                        ["2026-03-31", 1, "603221", "爱丽家居", 10, 3]
                    ],
                }
            ]
        }
        result = institution.normalize_holder_history(
            response,
            {
                "holder_key": "QF000034",
                "holder_id": "QF000034",
                "holder_variant_id": "",
                "name": "高盛公司有限责任公司",
                "holder_type": "QFII",
                "reference_code": "603221",
            },
        )
        self.assertEqual(
            result["counts"],
            {
                "records": 1,
                "stocks": 1,
                "detailed_stocks": 0,
                "detail_records": 0,
            },
        )
        self.assertEqual(result["records"][0]["report_date"], "2026-03-31")
        self.assertEqual(result["records"][0]["holding_shares"], 10)
        self.assertEqual(result["records"][0]["change_status"], "增持")

    def test_normalizes_per_stock_holder_history(self) -> None:
        response = {
            "ResultSets": [
                {
                    "ColName": ["rq", "T006", "T007", "T009"],
                    "Content": [
                        ["2026-03-31", 1359308, 0.56, 3],
                        ["2025-12-31", 835808, 0.34, 3],
                    ],
                }
            ]
        }
        records = institution.normalize_holder_stock_detail(
            response, "603221"
        )
        self.assertEqual(len(records), 2)
        self.assertEqual(records[0]["code"], "603221")
        self.assertEqual(records[1]["holding_shares"], 835808)

    def test_repairs_gdjcxq_mojibake(self) -> None:
        self.assertEqual(
            institution.repair_detail_text("\u93c2\u62cc\u7e58"),
            "新进",
        )
        self.assertEqual(institution.repair_detail_text("2026-03-31"), "2026-03-31")


if __name__ == "__main__":
    unittest.main()
