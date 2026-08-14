from __future__ import annotations

import unittest

import update_tdx_auction as auction


def response(columns: list[str], rows: list[list[object]]) -> dict[str, object]:
    return {
        "ErrorCode": 0,
        "ResultSets": [{
            "ColDes": [{"Name": name} for name in columns],
            "Content": rows,
        }],
    }


class AuctionSignalTests(unittest.TestCase):
    def test_normalizes_result_sets(self) -> None:
        rows = auction.response_rows(response(
            ["code", "market", "bidAmount"],
            [["000001", "0", "123"]],
        ))
        self.assertEqual(
            rows,
            [{"code": "000001", "market": "0", "bidAmount": "123"}],
        )

    def test_builds_security_reverse_index(self) -> None:
        model = auction.build_model({
            "auction-volume": response(
                ["code", "market", "bidAmount"],
                [["000001", "0", "123"], ["600000", "1", "456"]],
            ),
            "bad-board-strength": response(
                ["code", "market", "reason"],
                [["000001", "0", "测试"]],
            ),
        }, market_filter="0")
        self.assertEqual(model["counts"], {
            "signals": 2,
            "records": 3,
            "securities": 2,
            "multi_signal_securities": 1,
        })
        self.assertEqual(model["securities"][0]["code"], "000001")
        self.assertEqual(len(model["securities"][0]["signals"]), 2)

    def test_rejects_mismatched_row_width(self) -> None:
        with self.assertRaises(auction.AuctionUpdateError):
            auction.response_rows(response(["code", "market"], [["000001"]]))


if __name__ == "__main__":
    unittest.main()
