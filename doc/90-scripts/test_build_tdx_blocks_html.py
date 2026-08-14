from __future__ import annotations

import base64
import gzip
import json
import unittest

import build_tdx_blocks_html as builder
import extract_tdx_blocks as extractor


class CompactPayloadTests(unittest.TestCase):
    def test_compacts_and_round_trips_relations(self) -> None:
        blocks = [
            extractor.Block(
                block_id="industry:T01",
                family="industry",
                family_name="通达信行业",
                block_code="880001",
                name="测试父行业",
                source_key="T01",
                parent_block_id="",
                level=1,
                is_leaf=False,
                declared_count=None,
                member_count=1,
                start_date="",
                update_date="",
                source_file="test",
            ),
            extractor.Block(
                block_id="industry:T0101",
                family="industry",
                family_name="通达信行业",
                block_code="880002",
                name="测试行业",
                source_key="T0101",
                parent_block_id="industry:T01",
                level=2,
                is_leaf=True,
                declared_count=None,
                member_count=1,
                start_date="",
                update_date="20260731",
                source_file="test",
            ),
        ]
        members = [
            extractor.BlockMember(
                block_id="industry:T01",
                family="industry",
                family_name="通达信行业",
                block_code="880001",
                block_name="测试父行业",
                security_id="SZ000001",
                market_id=0,
                market="SZ",
                code="000001",
                security_name="平安银行",
                membership="descendant",
                name_resolved=True,
            ),
            extractor.BlockMember(
                block_id="industry:T0101",
                family="industry",
                family_name="通达信行业",
                block_code="880002",
                block_name="测试行业",
                security_id="SZ000001",
                market_id=0,
                market="SZ",
                code="000001",
                security_name="平安银行",
                membership="direct",
                name_resolved=True,
            ),
        ]

        document = builder.compact_payload(
            blocks,
            members,
            generated_at="2026-07-31T12:00:00+08:00",
        )
        encoded, raw_size, packed_size = builder.encode_payload(document)
        decoded = json.loads(
            gzip.decompress(base64.b64decode(encoded)).decode("utf-8")
        )

        self.assertEqual(decoded["stats"], {
            "blocks": 2,
            "memberships": 2,
            "securities": 1,
        })
        self.assertEqual(decoded["blocks"][1][3], 0)
        self.assertEqual(decoded["members"], [[0], [1]])
        self.assertEqual(decoded["reverse"], [[0, 3]])
        self.assertEqual(decoded["source_date"], "20260731")
        self.assertGreater(raw_size, packed_size)

    def test_renders_one_self_contained_html(self) -> None:
        document = {
            "v": 1,
            "generated_at": "2026-07-31T12:00:00+08:00",
            "source_date": "20260731",
            "stats": {"blocks": 0, "memberships": 0, "securities": 0},
            "families": [],
            "blocks": [],
            "securities": [],
            "members": [],
            "reverse": [],
        }
        html, _, _ = builder.render_html(document)

        self.assertIn("<!doctype html>", html)
        self.assertIn('id="tdx-data"', html)
        self.assertIn("DecompressionStream", html)
        self.assertIn('id="view-tree"', html)
        self.assertIn('id="view-market"', html)
        self.assertIn("renderBlockTree", html)
        self.assertIn("blockMetric", html)
        self.assertNotIn("__PAYLOAD__", html)
        self.assertNotIn("http://", html)
        self.assertNotIn("https://", html)


if __name__ == "__main__":
    unittest.main()
