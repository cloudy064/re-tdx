from __future__ import annotations

import unittest
from datetime import date
from pathlib import Path

import tdx_cloud_workflow as workflow


def response(columns: list[str], rows: list[object]) -> dict[str, object]:
    return {
        "ResultSets": [
            {
                "ColDes": [{"Name": name} for name in columns],
                "Content": rows,
                "RowNum": len(rows),
            }
        ],
        "ErrorCode": 0,
    }


class WorkflowTest(unittest.TestCase):
    def test_fund_report_defaults_observe_disclosure_cutoff(self) -> None:
        before = workflow.workflow_defaults(
            "fund-holdings",
            today=date(2026, 7, 31),
        )
        after = workflow.workflow_defaults(
            "fund-holdings",
            today=date(2026, 9, 1),
        )
        self.assertEqual(before["report_date"], "20251231")
        self.assertEqual(after["report_date"], "20260630")

    def test_result_rows_supports_json_and_pbrpc_shapes(self) -> None:
        json_rows = workflow.result_rows(
            response(["Code", "Name"], [["000001", "平安银行"]])
        )
        pbrpc_rows = workflow.result_rows(
            response(["SetCode", "Code", "Name"], ["0 000001 平安银行"])
        )
        scalar_rows = workflow.result_rows(
            response(["Detail"], ["上榜原因"])
        )
        self.assertEqual(
            json_rows,
            [{"Code": "000001", "Name": "平安银行"}],
        )
        self.assertEqual(
            pbrpc_rows,
            [{"SetCode": "0", "Code": "000001", "Name": "平安银行"}],
        )
        self.assertEqual(scalar_rows, [{"Detail": "上榜原因"}])

    def test_run_fund_workflow_maps_master_fields_to_children(self) -> None:
        calls: list[tuple[str, dict[str, str], bool]] = []

        def executor(
            _root: Path,
            step: workflow.StepSpec,
            parameters: dict[str, str],
            *,
            all_pages: bool,
            **_kwargs: object,
        ) -> dict[str, object]:
            calls.append((step.request_id, dict(parameters), all_pages))
            if step.request_id == "500050":
                result = response(
                    ["fund_code", "fund_name"],
                    [["000326", "基金甲"], ["000711", "基金乙"]],
                )
            else:
                result = response(["value"], [[step.request_id]])
            return {
                "req_id": step.request_id,
                "response": result,
            }

        result = workflow.run_workflow(
            Path("C:/new_tdx"),
            workflow.WORKFLOWS["fund-holdings"],
            {
                "style_details": "005001",
                "fund_size": "0",
                "fund_setup_time": "0",
                "report_date": "20250630",
            },
            selected=["000711"],
            executor=executor,
        )
        self.assertEqual(result["selection"]["selected_rows"], 1)
        self.assertTrue(calls[0][2])
        self.assertEqual([item[0] for item in calls], ["500050", "500051", "500052"])
        self.assertEqual(calls[1][1]["fund_code"], "000711")
        self.assertEqual(calls[2][1]["report_date"], "20250630")

    def test_lhb_aliases_condition_parameter(self) -> None:
        step = workflow.WORKFLOWS["lhb-details"].details[0]
        parameters = workflow.child_parameters(
            step,
            {"SetCode": "0", "Code": "000009"},
            {"result": "0"},
        )
        self.assertEqual(parameters["#2111.result"], "0")
        self.assertEqual(parameters["Code"], "000009")
        self.assertEqual(parameters["SetCode"], "0")


if __name__ == "__main__":
    unittest.main()
