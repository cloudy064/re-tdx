from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import download_tdx_jsn as downloader


class ProtocolTests(unittest.TestCase):
    def test_builds_metadata_request(self) -> None:
        data = downloader.build_file_info_request(
            "bi/list/func_gx_hyzt101_1.jsn"
        )
        self.assertEqual(len(data), 40)
        self.assertTrue(data.startswith(b"bi/list/func_gx_hyzt101_1.jsn\0"))

    def test_parses_metadata_response(self) -> None:
        data = (
            (8_901_469).to_bytes(4, "little")
            + b"\x01"
            + b"825d3fc338d08e8fe47d45593a3024ae"
            + b"\0"
        )
        info = downloader.parse_file_info(data)
        self.assertEqual(info.size, 8_901_469)
        self.assertEqual(info.md5, "825d3fc338d08e8fe47d45593a3024ae")
        self.assertTrue(info.has_md5)

    def test_zero_length_metadata_is_a_skippable_missing_resource(self) -> None:
        data = (0).to_bytes(4, "little") + bytes(34)
        with self.assertRaises(downloader.JsnResourceMissingError):
            downloader.parse_file_info(data)

    def test_builds_chunk_request_and_parses_response(self) -> None:
        data = downloader.build_file_chunk_request(
            "bi/list/example.jsn",
            30_000,
            123,
        )
        self.assertEqual(len(data), 308)
        self.assertEqual(int.from_bytes(data[:4], "little"), 30_000)
        self.assertEqual(int.from_bytes(data[4:8], "little"), 123)
        self.assertEqual(
            downloader.parse_file_chunk((3).to_bytes(4, "little") + b"abc", 3),
            b"abc",
        )

    def test_rejects_traversal_and_overlong_metadata_path(self) -> None:
        with self.assertRaises(downloader.JsnDownloadError):
            downloader.normalize_resource_path("../secret.jsn")
        with self.assertRaisesRegex(
            downloader.JsnDownloadError,
            "协议上限",
        ):
            downloader.build_file_info_request("x" * 40)

    def test_prefixes_bare_client_resource_with_list(self) -> None:
        self.assertEqual(
            downloader.normalize_resource_path("func_ygxc101_1.jsn"),
            "list/func_ygxc101_1.jsn",
        )
        self.assertEqual(
            downloader.normalize_resource_path("ggxc/0000001.jsn"),
            "ggxc/0000001.jsn",
        )


class InventoryAndSummaryTests(unittest.TestCase):
    def test_inventory_deduplicates_reqformat11_resources(self) -> None:
        xml = """<?xml version="1.0" encoding="gbk"?>
<root>
  <datasource reqformat="11" body="list/example.jsn"/>
  <datasource reqformat="11" body="list/example.jsn"/>
  <datasource reqformat="22" body="ignored"/>
</root>
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / "T0002" / "cloud_cfg"
            directory.mkdir(parents=True)
            (directory / "sample.xml").write_bytes(xml.encode("gb18030"))
            resources = downloader.inventory_resources(root)
        self.assertEqual(len(resources), 1)
        self.assertEqual(resources[0].resource, "list/example.jsn")
        self.assertEqual(resources[0].source_files, ("sample.xml",))

    def test_inventory_recovers_and_merges_cfg_file_resources(self) -> None:
        xml = """<?xml version="1.0" encoding="gbk"?>
<root><datasource reqformat="11" body="list/example.jsn"/></root>
"""
        cfg = """<?xml version="1.0" encoding="gbk"?>
<root>
  <unit file="example.jsn"/>
  <unit file='hidden_1.jsn'/>
  <item name="not-a-resource"/>
</root>
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / "T0002" / "cloud_cfg"
            directory.mkdir(parents=True)
            (directory / "sample.xml").write_bytes(xml.encode("gb18030"))
            (directory / "hidden.cfg").write_bytes(cfg.encode("gb18030"))
            resources = downloader.merge_resource_inventories(
                downloader.inventory_resources(root),
                downloader.inventory_cfg_resources(root),
            )
        self.assertEqual([item.resource for item in resources], [
            "list/example.jsn",
            "list/hidden_1.jsn",
        ])
        self.assertEqual(
            resources[0].source_files,
            ("sample.xml", "hidden.cfg"),
        )

    def test_inventory_recovers_known_cfg_detail_template(self) -> None:
        cfg = """<?xml version="1.0" encoding="gbk"?>
<root>
  <unit file="lhbfx"/>
  <unit file="cgfxmx1"/>
  <unit file="cgfxmx2"/>
  <unit file="zcjc"/>
  <unit file="hgrztj$$UNITID$$"/>
  <unit file="func_hsgt10$$UNITID$$.jsn"/>
</root>
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / "T0002" / "cloud_cfg"
            directory.mkdir(parents=True)
            (directory / "func_lhbfx102.cfg").write_bytes(
                cfg.encode("gb18030")
            )
            resources = downloader.inventory_cfg_resources(root)
        self.assertEqual(
            [item.resource for item in resources],
            [
                "cgfxmx1/$$$SC$$$$$ZQDM$$.jsn",
                "cgfxmx2/$$$SC$$$$$ZQDM$$.jsn",
                "hgrztj21701/$$$ZQDM$$.jsn",
                "hgrztj21702/$$$ZQDM$$.jsn",
                "hgrztj21703/$$$ZQDM$$.jsn",
                "lhbfx/$$$ZQDM$$.jsn",
                "zcjc/$$$SC$$$$$ZQDM$$.jsn",
            ],
        )
        self.assertEqual(
            resources[0].placeholders,
            ("$$$SC$$", "$$$ZQDM$$"),
        )

    def test_expands_market_and_code_template(self) -> None:
        self.assertEqual(
            downloader.expand_template(
                "ggxc/$$$SC$$$$$ZQDM$$.jsn",
                market=0,
                code="000001",
            ),
            "ggxc/0000001.jsn",
        )

    def test_selects_only_static_resources(self) -> None:
        resources = (
            downloader.JsnResource("list/a.jsn", ("a.xml",), ()),
            downloader.JsnResource(
                "ggxc/$$$ZQDM$$.jsn",
                ("b.xml",),
                ("$$$ZQDM$$",),
            ),
        )
        self.assertEqual(
            downloader.select_resources(
                resources,
                (),
                (),
                False,
                True,
            ),
            ("list/a.jsn",),
        )

    def test_selects_verified_cfg_family_from_inventory(self) -> None:
        resources = (
            downloader.JsnResource(
                "list/func_qszj101_1.jsn",
                ("func_qszj101.cfg",),
                (),
            ),
            downloader.JsnResource("list/other.jsn", ("other.cfg",), ()),
        )
        self.assertEqual(
            downloader.select_resource_families(
                resources,
                ("capital-strength",),
            ),
            ("list/func_qszj101_1.jsn",),
        )

    def test_new_action_families_have_no_duplicate_names(self) -> None:
        for family in (
            "corporate-actions", "block-trading", "equity-groups",
            "margin-financing", "stock-connect", "industry-lhb",
            "market-calendar", "futures-statistics", "ipo-bond-issuance",
            "price-limit-analysis", "commodity-themes", "premium-stocks",
            "market-anomalies", "etf-fund-flow", "event-research",
            "economic-indicators", "industry-region-logic",
            "earnings-forecast", "active-fund-holdings",
            "institution-seat-activity", "stock-industry-ratings",
            "convertible-bond-terms",
        ):
            resources = downloader.VERIFIED_CFG_FAMILIES[family]
            self.assertEqual(len(resources), len(set(resources)))
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["corporate-actions"]),
            28,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["block-trading"]),
            7,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["equity-groups"]),
            4,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["margin-financing"]),
            14,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["stock-connect"]),
            18,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["industry-lhb"]),
            3,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["market-calendar"]),
            2,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["futures-statistics"]),
            3,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["ipo-bond-issuance"]),
            3,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["price-limit-analysis"]),
            5,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["commodity-themes"]),
            2,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["premium-stocks"]),
            1,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["market-anomalies"]),
            5,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["etf-fund-flow"]),
            2,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["event-research"]),
            8,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["economic-indicators"]),
            1,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["industry-region-logic"]),
            2,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["earnings-forecast"]), 1,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["active-fund-holdings"]), 1,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["institution-seat-activity"]), 4,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["stock-industry-ratings"]), 2,
        )
        self.assertEqual(
            len(downloader.VERIFIED_CFG_FAMILIES["convertible-bond-terms"]), 6,
        )

    def test_summarizes_grouped_gb18030_json(self) -> None:
        value = [
            {
                "colheader": ["代码", "行业"],
                "data": [["000001", "银行"], ["000002", "地产"]],
            }
        ]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "sample.jsn"
            path.write_bytes(
                json.dumps(value, ensure_ascii=False).encode("gb18030")
            )
            summary = downloader.summarize_jsn(path)
        self.assertEqual(summary["groups"], 1)
        self.assertEqual(summary["rows"], 2)
        self.assertEqual(summary["headers"], [["代码", "行业"]])


if __name__ == "__main__":
    unittest.main()
