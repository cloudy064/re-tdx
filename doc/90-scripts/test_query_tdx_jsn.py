from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import query_tdx_jsn as query_tool


class QueryTests(unittest.TestCase):
    def test_resolves_futures_and_price_theme_context(self) -> None:
        futures = [{
            "colheader": ["SPQH", "$ZQDM", "$SC", "DATE", "xhjg"],
            "data": [["聚丙烯", "PPL8", "29", "20260730", "8900"]],
        }]
        themes = [{
            "colheader": ["name", "$ZQDM", "qdlj", "title", "date1", "gldm"],
            "data": [["环氧树脂", "1089", "逻辑", "事件", "20260515", "X1"]],
        }]
        commodities = [{
            "colheader": ["mc", "$ZQDM", "zxjg", "jjdw", "yjhy", "bjrq"],
            "data": [["环氧树脂", "X1", "100", "元/吨", "化工", "20260731"]],
        }]
        with tempfile.TemporaryDirectory() as temporary:
            input_dir = Path(temporary)
            list_dir = input_dir / "list"
            list_dir.mkdir()
            for name, value in (
                ("func_qhtj101_1.jsn", futures),
                ("func_zjtc101_1.jsn", themes),
                ("func_zjtc103_1.jsn", commodities),
            ):
                (list_dir / name).write_bytes(
                    json.dumps(value, ensure_ascii=False).encode("gb18030")
                )
            lookups = query_tool.load_relation_lookups(input_dir)

        self.assertEqual(lookups["futures"]["29PPL8"]["name"], "聚丙烯")
        self.assertEqual(
            lookups["price_themes"]["1089"]["commodity_id"],
            "X1",
        )
        self.assertEqual(lookups["commodities"]["X1"]["unit"], "元/吨")

    def test_resolves_indicator_and_news_event_context(self) -> None:
        indicators = [{
            "colheader": ["date", "NAME", "lb", "sz", "dw", "bgq", "$ZQDM"],
            "data": [["20260731", "PVC结算价", "价格", "4479", "元/吨", "20260731", "M1"]],
        }]
        events = [{
            "colheader": ["date", "title", "$ZQDM", "type", "bw"],
            "data": [["20260731", "产业事件", "20115", "利好", ""]],
        }]
        with tempfile.TemporaryDirectory() as temporary:
            input_dir = Path(temporary)
            list_dir = input_dir / "list"
            list_dir.mkdir()
            (list_dir / "func_jjzb101_1.jsn").write_bytes(
                json.dumps(indicators, ensure_ascii=False).encode("gb18030")
            )
            (list_dir / "func_sjqd101_1.jsn").write_bytes(
                json.dumps(events, ensure_ascii=False).encode("gb18030")
            )
            lookups = query_tool.load_relation_lookups(input_dir)

        self.assertEqual(
            lookups["economic_indicators"]["M1"]["name"],
            "PVC结算价",
        )
        self.assertEqual(
            lookups["news_events"]["20115"]["title"],
            "产业事件",
        )

    def test_resolves_earnings_forecast_group_context(self) -> None:
        groups = [{
            "colheader": ["$ZQDM", "$ZQDM1", "$SC1", "BGQ", "HYSL"],
            "data": [["88147720260630", "881477", "1", "20260630", "18"]],
        }]
        with tempfile.TemporaryDirectory() as temporary:
            input_dir = Path(temporary)
            list_dir = input_dir / "list"
            list_dir.mkdir()
            (list_dir / "func_yjygtj101_1.jsn").write_bytes(
                json.dumps(groups).encode("gb18030")
            )
            lookups = query_tool.load_relation_lookups(input_dir)

        self.assertEqual(
            lookups["forecast_groups"]["88147720260630"],
            {
                "forecast_group_key": "88147720260630",
                "industry_market": "1",
                "industry_code": "881477",
                "report_period": "20260630",
                "industry_company_count": "18",
            },
        )

    def test_matches_primary_and_reference_security(self) -> None:
        headers = ["$SC", "$ZQDM", "$SC1", "$ZQDM1"]
        row = ["0", "000001", "1", "600000"]
        self.assertEqual(
            query_tool.row_matches(headers, row, "0", "000001"),
            [["$SC", "$ZQDM"]],
        )
        self.assertEqual(
            query_tool.row_matches(headers, row, "1", "600000"),
            [["$SC1", "$ZQDM1"]],
        )

    def test_matches_security_inside_member_list(self) -> None:
        headers = ["title", "$S_ZQDM"]
        row = ["事件", "0|000001,1|600000"]
        self.assertEqual(
            query_tool.row_matches(headers, row, "1", "600000"),
            [["$S_ZQDM"]],
        )

    def test_matches_lowercase_industry_market_field(self) -> None:
        self.assertEqual(
            query_tool.row_matches(
                ["$ZQDM", "sc", "value"],
                ["880471", "1", "x"],
                "1",
                "880471",
            ),
            [["sc", "$ZQDM"]],
        )

    def test_queries_regular_and_bound_dynamic_rows(self) -> None:
        xml = """<?xml version="1.0" encoding="gbk"?>
<root>
  <gridcol name="$ZQDM" caption="代码"/>
  <gridcol name="$SC" caption="市场"/>
  <gridcol name="value" caption="数值"/>
  <datasource reqformat="11" body="list/sample.jsn"/>
  <datasource reqformat="11" body="ggxc/$$$SC$$$$$ZQDM$$.jsn"/>
</root>
"""
        regular = [
            {
                "colheader": ["$ZQDM", "$SC", "value"],
                "data": [
                    ["000001", "0", "a"],
                    ["600000", "1", "b"],
                ],
            }
        ]
        dynamic = [
            {
                "colheader": ["name"],
                "data": [["Alice"], ["Bob"]],
            }
        ]
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "tdx"
            config = root / "T0002" / "cloud_cfg"
            config.mkdir(parents=True)
            (config / "sample.xml").write_bytes(xml.encode("gb18030"))
            input_dir = base / "downloads"
            regular_path = input_dir / "list" / "sample.jsn"
            dynamic_path = input_dir / "ggxc" / "0000001.jsn"
            regular_path.parent.mkdir(parents=True)
            dynamic_path.parent.mkdir(parents=True)
            regular_path.write_bytes(
                json.dumps(regular).encode("gb18030")
            )
            dynamic_path.write_bytes(
                json.dumps(dynamic).encode("gb18030")
            )
            result = query_tool.query(root, input_dir, "0", "000001")

        self.assertEqual(result["counts"], {"resources": 2, "records": 3})
        self.assertEqual(
            result["resources"][0]["template_parameters"],
            {"market": "0", "code": "000001"},
        )
        self.assertEqual(
            result["resources"][1]["records"][0]["value"],
            "a",
        )
        self.assertIsNone(result["resources"][0]["theme"])

    def test_binds_composite_stock_connect_holding_key(self) -> None:
        master = [{
            "colheader": [
                "$ZQDM1", "$SC1", "N001", "jylx", "$ZQDM",
            ],
            "data": [["601369", "1", "20260630", "沪股通", "jd601369"]],
        }]
        detail = [{
            "colheader": ["date", "cgsl"],
            "data": [["20260630", "11184857"], ["20260331", "9980000"]],
        }]
        cfg = """<?xml version="1.0" encoding="gbk"?>
<root>
  <unit file="func_hsgt212_1.jsn"/>
  <unit file="hsgtcg1"/>
</root>
"""
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "tdx"
            config = root / "T0002" / "cloud_cfg"
            config.mkdir(parents=True)
            (config / "func_hsgt200_1.cfg").write_bytes(
                cfg.encode("gb18030")
            )
            input_dir = base / "downloads"
            master_path = input_dir / "list" / "func_hsgt212_1.jsn"
            detail_path = input_dir / "hsgtcg1" / "jd601369.jsn"
            master_path.parent.mkdir(parents=True)
            detail_path.parent.mkdir(parents=True)
            master_path.write_bytes(json.dumps(master).encode("gb18030"))
            detail_path.write_bytes(json.dumps(detail).encode("gb18030"))
            result = query_tool.query(root, input_dir, "1", "601369")

        self.assertEqual(result["counts"], {"resources": 2, "records": 3})
        dynamic = next(
            item for item in result["resources"]
            if item["resource"].startswith("hsgtcg1/")
        )
        self.assertEqual(
            dynamic["stock_connect_holding"],
            {
                "holding_key": "jd601369",
                "market": "1",
                "code": "601369",
                "report_date": "20260630",
                "channel": "沪股通",
            },
        )

    def test_binds_convertible_bond_detail_to_underlying_stock(self) -> None:
        master = [{
            "colheader": [
                "$ZQDM", "$SC", "ZQJC", "$ZQDM1", "$SC1", "ZGDM",
                "ZGJ", "DQRQ", "ZQYE",
            ],
            "data": [[
                "110076", "1", "华海转债", "600521", "1", "190076",
                "16.50", "20260618", "100000000",
            ]],
        }]
        detail = [{
            "colheader": ["$ZQDM", "$SC", "TZRQ", "TZZGJG", "TZYY"],
            "data": [["110076", "1", "20260618", "16.50", "调整转股价"]],
        }]
        cfg = """<?xml version="1.0" encoding="gbk"?>
<root><unit file="kzz_xztk"/></root>
"""
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "tdx"
            config = root / "T0002" / "cloud_cfg"
            config.mkdir(parents=True)
            (config / "func_kzz_xztk201_1.cfg").write_bytes(
                cfg.encode("gb18030")
            )
            input_dir = base / "downloads"
            master_path = input_dir / "list" / "kzz_kzzsy201_1.jsn"
            detail_path = input_dir / "kzz_xztk" / "1110076.jsn"
            master_path.parent.mkdir(parents=True)
            detail_path.parent.mkdir(parents=True)
            master_path.write_bytes(json.dumps(master).encode("gb18030"))
            detail_path.write_bytes(json.dumps(detail).encode("gb18030"))
            result = query_tool.query(root, input_dir, "1", "600521")

        self.assertEqual(result["counts"], {"resources": 2, "records": 2})
        dynamic = next(
            item for item in result["resources"]
            if item["resource"].startswith("kzz_xztk/")
        )
        self.assertEqual(dynamic["template_parameters"], {
            "market": "1", "code": "110076",
        })
        self.assertEqual(dynamic["convertible_bond"], {
            "market": "1",
            "code": "110076",
            "name": "华海转债",
            "underlying_market": "1",
            "underlying_code": "600521",
            "conversion_code": "190076",
            "conversion_price": "16.50",
            "maturity_date": "20260618",
            "balance": "100000000",
        })


if __name__ == "__main__":
    unittest.main()
