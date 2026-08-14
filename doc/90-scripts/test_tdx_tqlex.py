from __future__ import annotations

import json
import unittest

import tdx_tqlex as tqlex


def response(
    rows: list[list[str]],
    *,
    total: int | None = None,
) -> bytes:
    result_sets = [
        {
            "ColDes": [{"Name": "code"}],
            "Content": rows,
            "RowNum": len(rows),
        }
    ]
    if total is not None:
        result_sets.append(
            {
                "ColDes": [{"Name": "total"}],
                "Content": [[str(total)]],
                "RowNum": 1,
            }
        )
    return json.dumps(
        {
            "ErrorCode": 0,
            "ResultSetNum": len(result_sets),
            "ResultSets": result_sets,
        },
        separators=(",", ":"),
    ).encode()


class TQLEXTests(unittest.TestCase):
    def test_parses_single_quoted_config_and_paging_macros(self) -> None:
        parsed = tqlex.parse_config_body(
            "[{'ReqId':'500050','Type':'$$Type$$',"
            "'Page':'$$$STARTPOS$ $$','PageSize':'$$$PAGEROWS$ $$'}]",
            replacements={"Type": "1"},
            page=2,
            page_size=50,
        )
        self.assertEqual(
            parsed,
            [
                {
                    "ReqId": "500050",
                    "Type": "1",
                    "Page": "2",
                    "PageSize": "50",
                }
            ],
        )

    def test_queries_json_and_rejects_server_error(self) -> None:
        def transport(_url: str, payload: bytes, _timeout: float) -> bytes:
            self.assertEqual(json.loads(payload), [{"ReqId": "1"}])
            return response([["000001"]])

        result = tqlex.query_tqlex(
            "HQServ.test",
            [{"ReqId": "1"}],
            transport=transport,
        )
        self.assertEqual(result["ResultSets"][0]["RowNum"], 1)

        with self.assertRaisesRegex(tqlex.TQLEXError, "ErrorCode 5"):
            tqlex.query_tqlex(
                "HQServ.test",
                [{"ReqId": "1"}],
                transport=lambda *_args: b'{"ErrorCode":5,"ErrorInfo":"bad"}',
            )
        with self.assertRaisesRegex(tqlex.TQLEXError, "Invalid Page Size"):
            tqlex.query_tqlex(
                "HQServ.test",
                [{"ReqId": "1"}],
                transport=lambda *_args: b'{"error":"Invalid Page Size"}',
            )

    def test_pages_and_merges_result_sets(self) -> None:
        replies = iter(
            [
                response([["000001"], ["000002"]], total=3),
                response([["000003"]], total=3),
            ]
        )
        page_numbers: list[str] = []

        def transport(_url: str, payload: bytes, _timeout: float) -> bytes:
            item = json.loads(payload)[0]
            page_numbers.append(item["page"])
            self.assertNotIn("Page", item)
            self.assertEqual(item["pageSize"], "2")
            return next(replies)

        result = tqlex.query_all_pages(
            "HQServ.test",
            [{"ReqId": "1", "page": "0", "pageSize": "2"}],
            page_size=2,
            transport=transport,
        )
        result_set = result["ResultSets"][0]
        self.assertEqual(page_numbers, ["0", "1"])
        self.assertEqual(
            result_set["Content"],
            [["000001"], ["000002"], ["000003"]],
        )
        self.assertEqual(result_set["RowNum"], 3)
        self.assertEqual(
            result["ResultSets"][1]["Content"],
            [["3"]],
        )


if __name__ == "__main__":
    unittest.main()
