from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import inventory_tdx_cloud_features as inventory


class CloudFeatureInventoryTests(unittest.TestCase):
    def test_extracts_entry_request_module_parameters_and_fields(self) -> None:
        xml = """<?xml version="1.0" encoding="gbk"?>
<root>
  <component caption="龙虎榜">
    <gridcol name="Code" caption="代码"/>
    <gridcol name="Type" caption="上榜类型"/>
    <datasource reqformat="2" cache="0"
      name="HQServ.hq_nlp_mdsi"
      body="[{&quot;ReqId&quot;:&quot;500109&quot;,
      &quot;Type&quot;:&quot;$$result$$&quot;,
      &quot;modname&quot;:&quot;mod_mdsi.dll&quot;}]">
    </datasource>
  </component>
</root>
"""
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "hq_lhb.xml"
            path.write_bytes(xml.encode("gbk"))
            records = inventory.parse_cloud_feature_file(path)

        self.assertEqual(len(records), 1)
        record = records[0]
        self.assertEqual(record.datasource_name, "HQServ.hq_nlp_mdsi")
        self.assertEqual(record.request_format, "2")
        self.assertEqual(record.request_id, "500109")
        self.assertEqual(record.module, "mod_mdsi.dll")
        self.assertEqual(record.placeholders, "result")
        self.assertIn("龙虎榜", record.titles)
        self.assertIn("Code:代码", record.output_fields)
        self.assertIn("Type:上榜类型", record.output_fields)

    def test_ignores_commented_out_datasources(self) -> None:
        xml = """<?xml version="1.0" encoding="gbk"?>
<root>
  <!--
  <datasource reqformat="22" name="HQServ.PBRPC"
    body="pb_rpc_req:ReqByte={&quot;ReqId&quot;:&quot;500007&quot;}"/>
  -->
  <datasource reqformat="2" name="HQServ.active"
    body="[{&quot;ReqId&quot;:&quot;500050&quot;}]"/>
</root>
"""
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "active.xml"
            path.write_bytes(xml.encode("gbk"))
            records = inventory.parse_cloud_feature_file(path)

        self.assertEqual([record.request_id for record in records], ["500050"])
        self.assertEqual(records[0].datasource_name, "HQServ.active")


if __name__ == "__main__":
    unittest.main()
