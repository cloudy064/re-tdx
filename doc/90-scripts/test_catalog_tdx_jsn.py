from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import catalog_tdx_jsn as catalog
import download_tdx_jsn as downloader


class CatalogTests(unittest.TestCase):
    def test_matches_dynamic_resource_template(self) -> None:
        resources = (
            downloader.JsnResource(
                "ggxc/$$$SC$$$$$ZQDM$$.jsn",
                ("cfg.xml",),
                ("$$$SC$$", "$$$ZQDM$$"),
            ),
        )
        matched = catalog.identify_resource("ggxc/0000001.jsn", resources)
        self.assertIsNotNone(matched)
        self.assertEqual(
            catalog.template_parameters(
                "ggxc/$$$SC$$$$$ZQDM$$.jsn",
                "ggxc/0000001.jsn",
            ),
            {"market": "0", "code": "000001"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "zttzty/$$$ZQDM$$.jsn",
                "zttzty/657.jsn",
            ),
            {"theme_id": "657"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "lhbfx/$$$ZQDM$$.jsn",
                "lhbfx/3725663.jsn",
            ),
            {"event_id": "3725663"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "cgfxmx2/$$$SC$$$$$ZQDM$$.jsn",
                "cgfxmx2/1603221.jsn",
            ),
            {"market": "1", "code": "603221"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "dzjy1/$$$ZQDM$$.jsn",
                "dzjy1/2026-07.jsn",
            ),
            {"month": "2026-07"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "yybph22401/$$$ZQDM$$.jsn",
                "yybph22401/1339358b.jsn",
            ),
            {"branch_id": "1339358b"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "gqgg/$$$ZQDM$$.jsn",
                "gqgg/gl197.jsn",
            ),
            {"group_id": "gl197"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "gghg/$$$SC$$$$$ZQDM$$.jsn",
                "gghg/3102378.jsn",
            ),
            {"market": "31", "code": "02378"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "hsgtcg1/$$$ZQDM$$.jsn",
                "hsgtcg1/jd601369.jsn",
            ),
            {"holding_key": "jd601369"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "ggthy/$$$SC$$$$$ZQDM$$.jsn",
                "ggthy/70HK0201.jsn",
            ),
            {"group_market": "70", "group_code": "HK0201"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "ipotj103/$$$ZQDM$$.jsn",
                "ipotj103/2026881015.jsn",
            ),
            {"ipo_industry_key": "2026881015"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "zdtfx2/$$$ZQDM$$.jsn",
                "zdtfx2/20260731.jsn",
            ),
            {"date": "20260731"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "zjtc3/$$$ZQDM$$.jsn",
                "zjtc3/19948.jsn",
            ),
            {"price_event_id": "19948"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "zjtc4/$$$ZQDM$$.jsn",
                "zjtc4/X100102003.jsn",
            ),
            {"commodity_id": "X100102003"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "jjzb2/$$$ZQDM$$.jsn",
                "jjzb2/M2400000007.jsn",
            ),
            {"economic_indicator_id": "M2400000007"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "ydyl1/$$$ZQDM$$.jsn",
                "ydyl1/hy57.jsn",
            ),
            {"opportunity_group_id": "hy57"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "sjqd/$$$ZQDM$$.jsn",
                "sjqd/20115.jsn",
            ),
            {"news_event_id": "20115"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "yjyg/$$$ZQDM$$.jsn",
                "yjyg/88147720260630.jsn",
            ),
            {"forecast_group_key": "88147720260630"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "ggpj/$$$SC$$$$$ZQDM$$.jsn",
                "ggpj/3100666.jsn",
            ),
            {"market": "31", "code": "00666"},
        )
        self.assertEqual(
            catalog.template_parameters(
                "kzz_xztk/$$$SC$$$$$ZQDM$$.jsn",
                "kzz_xztk/1110076.jsn",
            ),
            {"market": "1", "code": "110076"},
        )

    def test_maps_live_redemption_quantity_header_to_cfg_caption(self) -> None:
        cfg = """<?xml version="1.0" encoding="gbk"?>
<root><unit><item code="WHSSL" name="未赎回数量" datatype="F"/></unit></root>
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / "T0002" / "cloud_cfg"
            directory.mkdir(parents=True)
            (directory / "func_kzz_shtk201_1.cfg").write_bytes(
                cfg.encode("gb18030")
            )
            metadata = catalog.column_metadata(
                root,
                ("func_kzz_shtk201_1.cfg",),
                "kzz_shtk/$$$SC$$$$$ZQDM$$.jsn",
            )

        self.assertEqual(metadata["WSHSL"]["caption"], "未赎回数量")

    def test_builds_field_and_security_catalog(self) -> None:
        xml = """<?xml version="1.0" encoding="gbk"?>
<root>
  <gridcol name="$ZQDM" caption="代码" datatype="S"/>
  <gridcol name="$SC" caption="市场" datatype="I"/>
  <gridcol name="score" caption="分数" datatype="F"/>
  <datasource reqformat="11" body="list/sample.jsn"/>
</root>
"""
        value = [
            {
                "colheader": ["$ZQDM", "$SC", "score", "raw"],
                "data": [
                    ["000001", "0", "90", "x"],
                    ["000001", "0", "80", "y"],
                    ["600000", "1", "70", "z"],
                ],
            }
        ]
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "tdx"
            config = root / "T0002" / "cloud_cfg"
            config.mkdir(parents=True)
            (config / "sample.xml").write_bytes(xml.encode("gb18030"))
            input_dir = base / "downloads"
            resource = input_dir / "list" / "sample.jsn"
            resource.parent.mkdir(parents=True)
            resource.write_bytes(
                json.dumps(value, ensure_ascii=False).encode("gb18030")
            )
            result = catalog.build_catalog(root, input_dir)

        item = result["resources"][0]
        self.assertEqual(item["rows"], 3)
        self.assertEqual(item["unique_securities"], 2)
        self.assertEqual(item["duplicate_security_rows"], 1)
        self.assertEqual(item["markets"], {"0": 1, "1": 1})
        self.assertEqual(item["columns"][2]["caption"], "分数")
        self.assertEqual(item["unmapped_fields"], ["raw"])

    def test_counts_member_list_security_associations(self) -> None:
        self.assertEqual(
            catalog.member_security_keys("0|000001, 1|600000,invalid"),
            (("0", "000001"), ("1", "600000")),
        )

    def test_recognizes_lowercase_industry_market_field(self) -> None:
        self.assertEqual(
            catalog.security_key_fields(["$ZQDM", "sc", "value"]),
            ("sc", "$ZQDM"),
        )


if __name__ == "__main__":
    unittest.main()
