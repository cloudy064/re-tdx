from __future__ import annotations

import unittest
from pathlib import Path

import extract_tdx_blocks as extractor


def synthetic_tnf() -> bytes:
    data = bytearray(50 + 360)
    record = memoryview(data)[50:]
    record[:6] = b"000001"
    name = "平安银行".encode("gb18030")
    record[31 : 31 + len(name)] = name
    return bytes(data)


class SecurityMasterTests(unittest.TestCase):
    def test_parses_current_360_byte_tnf_layout(self) -> None:
        securities = extractor.parse_tnf(synthetic_tnf(), 0)
        self.assertEqual(len(securities), 1)
        self.assertEqual(securities[0].security_id, "SZ000001")
        self.assertEqual(securities[0].name, "平安银行")


class IndustryTests(unittest.TestCase):
    CATALOG = "\n".join(
        (
            "煤炭|880301|2|1|0|T0101",
            "煤炭开采|880302|2|1|1|T010101",
            "煤炭|881001|12|1|0|X10",
            "煤炭开采|881002|12|1|0|X1001",
        )
    )
    ASSIGNMENTS = "1|600000|T010101|||X1001"

    def test_parent_industry_includes_descendant_assignment(self) -> None:
        blocks = extractor.parse_industry_catalog(self.CATALOG)
        assignments = extractor.parse_industry_assignments(self.ASSIGNMENTS)
        security = extractor.Security(1, "SH", "上海", "600000", "浦发银行")
        members = extractor.build_industry_members(
            blocks,
            assignments,
            {(1, "600000"): security},
        )
        by_id = {item.block_id: item for item in members}

        self.assertEqual(by_id["industry:T0101"].membership, "descendant")
        self.assertEqual(by_id["industry:T010101"].membership, "direct")
        self.assertEqual(
            by_id["research-industry:X10"].membership, "descendant"
        )
        self.assertEqual(
            by_id["research-industry:X1001"].membership, "direct"
        )


class InfoharborTests(unittest.TestCase):
    def test_parses_concept_and_index_members(self) -> None:
        securities = {
            (0, "000001"): extractor.Security(
                0, "SZ", "深圳", "000001", "平安银行"
            ),
            (1, "600000"): extractor.Security(
                1, "SH", "上海", "600000", "浦发银行"
            ),
        }
        text = "\n".join(
            (
                "#GN_测试概念,2,880001,20200101,20260101,,",
                "0#000001,1#600000,",
                "#ZS_测试指数,1,,20200101,,,",
                "1#600000,",
            )
        )
        blocks, members = extractor.parse_infoharbor(text, securities)

        self.assertEqual(len(blocks), 2)
        self.assertEqual(blocks[0].member_count, 2)
        self.assertEqual(blocks[1].block_id, "index:测试指数")
        self.assertEqual(len(members), 3)
        self.assertTrue(all(item.name_resolved for item in members))


class CurrentInstallIntegrationTests(unittest.TestCase):
    ROOT = Path(r"C:\new_tdx")

    @unittest.skipUnless(ROOT.is_dir(), "local TDX installation is unavailable")
    def test_current_block_counts(self) -> None:
        securities, blocks, members = extractor.extract(
            self.ROOT, set(extractor.FAMILY_CHOICES)
        )
        block_counts = {
            family: sum(block.family == family for block in blocks)
            for family in extractor.FAMILY_CHOICES
        }

        self.assertGreater(len(securities), 50_000)
        self.assertGreater(block_counts["industry"], 100)
        self.assertGreater(block_counts["research-industry"], 400)
        self.assertGreater(block_counts["concept"], 200)
        self.assertGreater(block_counts["style"], 100)
        self.assertGreater(block_counts["index"], 100)
        self.assertEqual(
            sum(
                member.family in {"concept", "style", "index"}
                for member in members
            ),
            sum(
                block.declared_count or 0
                for block in blocks
                if block.family in {"concept", "style", "index"}
            ),
        )
        self.assertEqual(
            sum(
                not member.name_resolved
                for member in members
                if member.family in {"concept", "style", "index"}
            ),
            0,
        )
        self.assertEqual(
            len(members),
            len({(member.block_id, member.security_id) for member in members}),
        )


if __name__ == "__main__":
    unittest.main()
