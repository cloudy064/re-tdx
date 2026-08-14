from __future__ import annotations

import json
import unittest
from pathlib import Path
from unittest import mock

import tdx_pbrpc as pbrpc


def response(
    *,
    rpc_id: int,
    start_pos: int = 0,
    total_len: int = 0,
    data: bytes = b"",
) -> bytes:
    result = pbrpc.encode_varint_field(2, rpc_id)
    if start_pos:
        result += pbrpc.encode_varint_field(3, start_pos)
    if total_len:
        result += pbrpc.encode_varint_field(4, total_len)
    if data:
        result += pbrpc.encode_varint_field(5, len(data))
        result += pbrpc.encode_bytes_field(6, data)
    return result


class PBRPCCodecTests(unittest.TestCase):
    def test_builds_known_200404_request(self) -> None:
        request_json = (
            '{"ReqId":"200404","market":"0","Page":"-1",'
            '"PageSize":"100","modname":"mod_peg.dll"}'
        )
        payload = pbrpc.build_request("mod_peg.dll", request_json)
        self.assertEqual(
            payload.hex(),
            "0a030a0131220b6d6f645f7065672e646c6c2a54"
            "7b225265714964223a22323030343034222c226d61726b6574223a2230222c"
            "2250616765223a222d31222c225061676553697a65223a22313030222c226d"
            "6f646e616d65223a226d6f645f7065672e646c6c227d",
        )

    def test_parses_rpc_id_only_response(self) -> None:
        parsed = pbrpc.parse_response(bytes.fromhex("109d8601"))
        self.assertEqual(parsed.rpc_id, 17181)
        self.assertEqual(parsed.ret_byte, b"")

    def test_parses_negative_int32_rpc_id(self) -> None:
        parsed = pbrpc.parse_response(
            bytes.fromhex("10ffffffffffffffffff01")
        )
        self.assertEqual(parsed.rpc_id, -1)

    def test_completes_two_round_rpc(self) -> None:
        business = json.dumps(
            {"ErrorCode": 0, "ResultSets": []},
            separators=(",", ":"),
        ).encode()
        replies = iter(
            [
                response(rpc_id=123),
                response(rpc_id=123, total_len=len(business), data=business),
            ]
        )
        requests: list[bytes] = []

        def transport(_url: str, payload: bytes, _timeout: float) -> bytes:
            requests.append(payload)
            return next(replies)

        result = pbrpc.query_pbrpc(
            "HQServ.PBRPC_PEG",
            "mod_peg.dll",
            {"ReqId": "200404"},
            retry_delay=0,
            transport=transport,
        )
        self.assertEqual(result, business)
        second_fields = {
            number: value
            for number, _wire, value in pbrpc.decode_fields(requests[1])
        }
        self.assertEqual(second_fields[2], 123)

    def test_parses_xml_descriptor(self) -> None:
        module, request_json = pbrpc.parse_descriptor(
            "pb_rpc_req:Head.CharSet=1;Head.Target=0;RpcID=0;"
            "StartPos=0;Moduledll=mod_peg.dll;"
            'ReqByte={"ReqId":"200404","Page":"-1"}'
        )
        self.assertEqual(module, "mod_peg.dll")
        self.assertEqual(json.loads(request_json)["ReqId"], "200404")

    def test_decodes_nul_terminated_json(self) -> None:
        payload = b'{"ErrorCode":0,"ResultSets":[]}\x00'
        self.assertEqual(
            pbrpc.decode_result(payload),
            {"ErrorCode": 0, "ResultSets": []},
        )

    def test_selects_duplicate_reqid_by_body_text(self) -> None:
        def feature(body: str, placeholders: str) -> object:
            return pbrpc.cloud_inventory.CloudFeature(
                source_file="boards.xml",
                datasource_name="HQServ.PBRPC_GPSJ",
                request_format="22",
                request_id="200199",
                module="mod_gpsj.dll",
                cache="",
                titles="",
                output_fields="",
                placeholders=placeholders,
                body=body,
            )

        records = [
            feature(
                "pb_rpc_req:Moduledll=mod_gpsj.dll;"
                'ReqByte={"ReqId":"200199","CodeList":"12:0"}',
                "",
            ),
            feature(
                "pb_rpc_req:Moduledll=mod_gpsj.dll;"
                'ReqByte={"ReqId":"200199","CodeList":"2:$$code$$|1"}',
                "code",
            ),
        ]
        with mock.patch.object(
            pbrpc.cloud_inventory,
            "inventory",
            return_value=records,
        ):
            spec = pbrpc.find_config_spec(
                Path("."),
                "200199",
                body_contains=("2:$$code$$|1",),
                replacements={"code": "881001"},
            )
        self.assertEqual(spec.request["CodeList"], "2:881001|1")


if __name__ == "__main__":
    unittest.main()
