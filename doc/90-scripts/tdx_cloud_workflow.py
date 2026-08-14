#!/usr/bin/env python3
"""Run linked TDX cloud queries described by the client's master/slave pages."""

from __future__ import annotations

import argparse
import copy
import json
import sys
import urllib.error
from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Callable, Sequence

import tdx_pbrpc as pbrpc
import tdx_tqlex as tqlex
import update_tdx_blocks as updater


class WorkflowError(RuntimeError):
    """Raised when a linked cloud workflow cannot be completed."""


@dataclass(frozen=True)
class StepSpec:
    transport: str
    request_id: str
    field_map: tuple[tuple[str, str], ...] = ()
    parameter_aliases: tuple[tuple[str, str], ...] = ()


@dataclass(frozen=True)
class WorkflowSpec:
    name: str
    description: str
    key_field: str
    master: StepSpec
    details: tuple[StepSpec, ...]


WORKFLOWS = {
    "fund-holdings": WorkflowSpec(
        name="fund-holdings",
        description="基金筛选→行业持仓、股票持仓",
        key_field="fund_code",
        master=StepSpec("json", "500050"),
        details=(
            StepSpec("json", "500051", (("fund_code", "fund_code"),)),
            StepSpec("json", "500052", (("fund_code", "fund_code"),)),
        ),
    ),
    "fund-risk": WorkflowSpec(
        name="fund-risk",
        description="基金收益风险主表→每日基金/基准收益走势",
        key_field="fund_code",
        master=StepSpec("json", "500030"),
        details=(
            StepSpec("json", "500031", (("fund_code", "fund_code"),)),
        ),
    ),
    "fund-volatility": WorkflowSpec(
        name="fund-volatility",
        description="基金月度风险主表→月度基金/基准收益走势",
        key_field="fund_code",
        master=StepSpec("json", "500032"),
        details=(
            StepSpec("json", "500033", (("fund_code", "fund_code"),)),
        ),
    ),
    "fund-interval-holdings": WorkflowSpec(
        name="fund-interval-holdings",
        description="基金区间持仓主表→行业平均持仓、逐报告期集中度",
        key_field="fund_code",
        master=StepSpec("json", "500055"),
        details=(
            StepSpec("json", "500056", (("fund_code", "fund_code"),)),
            StepSpec("json", "500057", (("fund_code", "fund_code"),)),
        ),
    ),
    "index-valuation": WorkflowSpec(
        name="index-valuation",
        description="指数估值概览→所选指数每日估值比走势",
        key_field="code",
        master=StepSpec("json", "200000"),
        details=(
            StepSpec("pbrpc", "200001", (("Code", "code"),)),
        ),
    ),
    "lhb-details": WorkflowSpec(
        name="lhb-details",
        description="PBRPC 龙虎榜列表→普通 JSON 上榜原因说明",
        key_field="Code",
        master=StepSpec("pbrpc", "500107"),
        details=(
            StepSpec(
                "json",
                "500108",
                (("Code", "Code"), ("SetCode", "SetCode")),
                (("#2111.result", "result"),),
            ),
        ),
    ),
}


def shift_year(value: date, years: int) -> date:
    try:
        return value.replace(year=value.year + years)
    except ValueError:
        return value.replace(year=value.year + years, day=28)


def shift_month(value: date, months: int) -> date:
    month_index = value.year * 12 + value.month - 1 + months
    year, zero_based_month = divmod(month_index, 12)
    month = zero_based_month + 1
    month_lengths = (
        31,
        29
        if year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)
        else 28,
        31,
        30,
        31,
        30,
        31,
        31,
        30,
        31,
        30,
        31,
    )
    return date(year, month, min(value.day, month_lengths[month - 1]))


def latest_full_fund_report(value: date) -> date:
    # Full stock/industry holdings are reliably available in annual and
    # semiannual reports.  Before 31 August use the preceding annual report.
    if (value.month, value.day) >= (8, 31):
        return date(value.year, 6, 30)
    return date(value.year - 1, 12, 31)


def workflow_defaults(name: str, *, today: date | None = None) -> dict[str, str]:
    today = today or date.today()
    end_date = today.strftime("%Y%m%d")
    common_fund = {
        "style_details": "005001",
        "fund_size": "0",
        "fund_setup_time": "0",
    }
    if name == "fund-holdings":
        report = latest_full_fund_report(today)
        return {
            **common_fund,
            "report_date": report.strftime("%Y%m%d"),
        }
    if name == "fund-risk":
        return {
            **common_fund,
            "basic_code": "0",
            "start_date": shift_month(today, -3).strftime("%Y%m%d"),
            "end_date": end_date,
            "rate_freerisk": "3",
        }
    if name == "fund-volatility":
        return {
            **common_fund,
            "basic_code": "0",
            "start_date": shift_year(today, -3).strftime("%Y%m%d"),
            "end_date": end_date,
        }
    if name == "fund-interval-holdings":
        report = latest_full_fund_report(today)
        return {
            **common_fund,
            "start_date": shift_year(report, -2).strftime("%Y%m%d"),
            "end_date": report.strftime("%Y%m%d"),
        }
    if name == "index-valuation":
        return {
            "StartDate": shift_year(today, -2).strftime("%Y%m%d"),
            "EndDate": end_date,
            "IndexType": "1",
            "Basics": "000001",
            "Methods": "PETTM",
        }
    if name == "lhb-details":
        return {"result": "0"}
    raise WorkflowError(f"unknown workflow {name!r}")


def _column_names(result_set: dict[str, object]) -> list[str]:
    descriptions = result_set.get("ColDes", [])
    if not isinstance(descriptions, list):
        raise WorkflowError("ColDes must be an array")
    names: list[str] = []
    for description in descriptions:
        if not isinstance(description, dict):
            raise WorkflowError("ColDes items must be objects")
        name = description.get("Name")
        if not isinstance(name, str) or not name:
            raise WorkflowError("result column has no Name")
        names.append(name)
    return names


def result_rows(
    response: dict[str, object],
    *,
    result_set_index: int = 0,
) -> list[dict[str, object]]:
    result_sets = response.get("ResultSets", [])
    if not isinstance(result_sets, list):
        raise WorkflowError("ResultSets must be an array")
    if result_set_index >= len(result_sets):
        raise WorkflowError(f"result set {result_set_index} does not exist")
    result_set = result_sets[result_set_index]
    if not isinstance(result_set, dict):
        raise WorkflowError("ResultSets items must be objects")
    columns = _column_names(result_set)
    content = result_set.get("Content", [])
    if not isinstance(content, list):
        raise WorkflowError("ResultSet Content must be an array")

    rows: list[dict[str, object]] = []
    for raw_row in content:
        if isinstance(raw_row, dict):
            rows.append(dict(raw_row))
            continue
        if isinstance(raw_row, list):
            values = raw_row
        elif isinstance(raw_row, str) and len(columns) > 1:
            values = raw_row.split()
        else:
            values = [raw_row]
        if len(values) != len(columns):
            raise WorkflowError(
                f"row has {len(values)} values for {len(columns)} columns"
            )
        rows.append(dict(zip(columns, values)))
    return rows


def _casefold_value(row: dict[str, object], name: str) -> object:
    folded = name.casefold()
    for key, value in row.items():
        if key.casefold() == folded:
            return value
    raise WorkflowError(f"master result has no {name!r} column")


def select_rows(
    rows: Sequence[dict[str, object]],
    *,
    key_field: str,
    selected: Sequence[str] = (),
    limit: int = 1,
) -> list[dict[str, object]]:
    if limit <= 0:
        raise WorkflowError("limit must be positive")
    if selected:
        wanted = {value.casefold() for value in selected}
        matched = [
            row
            for row in rows
            if str(_casefold_value(row, key_field)).casefold() in wanted
        ]
        found = {
            str(_casefold_value(row, key_field)).casefold()
            for row in matched
        }
        missing = [
            value for value in selected if value.casefold() not in found
        ]
        if missing:
            raise WorkflowError(
                "selected master keys were not returned: " + ", ".join(missing)
            )
        return matched[:limit]
    return list(rows[:limit])


def child_parameters(
    step: StepSpec,
    master_row: dict[str, object],
    parameters: dict[str, str],
) -> dict[str, str]:
    result = dict(parameters)
    for placeholder, field in step.field_map:
        result[placeholder] = str(_casefold_value(master_row, field))
    for alias, parameter in step.parameter_aliases:
        if parameter not in parameters:
            raise WorkflowError(f"workflow parameter {parameter!r} is missing")
        result[alias] = parameters[parameter]
    return result


def _check_response(response: object, request_id: str) -> dict[str, object]:
    if not isinstance(response, dict):
        raise WorkflowError(
            f"ReqId {request_id} returned {type(response).__name__}, not an object"
        )
    error_code = response.get("ErrorCode", 0)
    try:
        numeric_error = int(error_code)
    except (TypeError, ValueError) as error:
        raise WorkflowError(
            f"ReqId {request_id} returned invalid ErrorCode {error_code!r}"
        ) from error
    if numeric_error:
        raise WorkflowError(
            f"ReqId {request_id} returned ErrorCode {numeric_error}: "
            f"{response.get('ErrorInfo', '')}"
        )
    return response


def execute_step(
    root: Path,
    step: StepSpec,
    parameters: dict[str, str],
    *,
    all_pages: bool = False,
    page_size: int = 20,
    max_pages: int = 100,
    base_url: str = tqlex.DEFAULT_BASE_URL,
    timeout: float = 15.0,
    verbose: bool = False,
) -> dict[str, object]:
    if step.transport == "json":
        spec = tqlex.find_config_spec(
            root,
            step.request_id,
            replacements=parameters,
            page=0,
            page_size=page_size,
        )
        request = copy.deepcopy(spec.request)
        mapping = tqlex.request_mapping(request)
        tqlex.set_request_value(mapping, "ReqId", step.request_id)
        response = (
            tqlex.query_all_pages(
                spec.entry,
                request,
                page_size=page_size,
                max_pages=max_pages,
                base_url=base_url,
                timeout=timeout,
                verbose=verbose,
            )
            if all_pages
            else tqlex.query_tqlex(
                spec.entry,
                request,
                base_url=base_url,
                timeout=timeout,
            )
        )
        return {
            "transport": "json",
            "req_id": step.request_id,
            "entry": spec.entry,
            "source_file": spec.source_file,
            "request": request,
            "response": _check_response(response, step.request_id),
        }

    if step.transport == "pbrpc":
        spec = pbrpc.find_config_spec(
            root,
            step.request_id,
            replacements=parameters,
        )
        request = dict(spec.request)
        request["ReqId"] = step.request_id
        raw = pbrpc.query_pbrpc(
            spec.entry,
            spec.module,
            request,
            base_url=base_url,
            timeout=timeout,
            verbose=verbose,
        )
        response = _check_response(
            pbrpc.decode_result(raw),
            step.request_id,
        )
        return {
            "transport": "pbrpc",
            "req_id": step.request_id,
            "entry": spec.entry,
            "module": spec.module,
            "source_file": spec.source_file,
            "request": request,
            "response": response,
        }
    raise WorkflowError(f"unsupported transport {step.transport!r}")


StepExecutor = Callable[..., dict[str, object]]


def run_workflow(
    root: Path,
    workflow: WorkflowSpec,
    parameters: dict[str, str],
    *,
    selected: Sequence[str] = (),
    limit: int = 1,
    master_all_pages: bool = False,
    page_size: int = 20,
    max_pages: int = 100,
    base_url: str = tqlex.DEFAULT_BASE_URL,
    timeout: float = 15.0,
    verbose: bool = False,
    executor: StepExecutor = execute_step,
) -> dict[str, object]:
    fetch_all = master_all_pages or bool(selected)
    master = executor(
        root,
        workflow.master,
        parameters,
        all_pages=fetch_all,
        page_size=page_size,
        max_pages=max_pages,
        base_url=base_url,
        timeout=timeout,
        verbose=verbose,
    )
    response = master.get("response")
    if not isinstance(response, dict):
        raise WorkflowError("master step has no response object")
    rows = result_rows(response)
    chosen = select_rows(
        rows,
        key_field=workflow.key_field,
        selected=selected,
        limit=limit,
    )

    details: list[dict[str, object]] = []
    for row in chosen:
        children: list[dict[str, object]] = []
        for step in workflow.details:
            replacements = child_parameters(step, row, parameters)
            children.append(
                executor(
                    root,
                    step,
                    replacements,
                    all_pages=False,
                    page_size=page_size,
                    max_pages=max_pages,
                    base_url=base_url,
                    timeout=timeout,
                    verbose=verbose,
                )
            )
        details.append(
            {
                "key": str(_casefold_value(row, workflow.key_field)),
                "master_row": row,
                "children": children,
            }
        )

    return {
        "workflow": workflow.name,
        "description": workflow.description,
        "parameters": parameters,
        "selection": {
            "key_field": workflow.key_field,
            "master_rows": len(rows),
            "selected_rows": len(chosen),
        },
        "master": master,
        "details": details,
    }


def list_workflows() -> str:
    lines = ["Workflow\tMaster\tDetails\tDescription"]
    for name, workflow in WORKFLOWS.items():
        master = (
            f"{workflow.master.transport}:{workflow.master.request_id}"
        )
        details = ",".join(
            f"{step.transport}:{step.request_id}"
            for step in workflow.details
        )
        lines.append(f"{name}\t{master}\t{details}\t{workflow.description}")
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "按通达信 cloud_cfg 的 masterid 关系执行普通 JSON/PBRPC "
            "主表→明细工作流。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录")
    parser.add_argument("--list", action="store_true", help="列出工作流")
    parser.add_argument("--workflow", choices=tuple(WORKFLOWS))
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="覆盖工作流默认参数；可重复",
    )
    parser.add_argument(
        "--select",
        action="append",
        default=[],
        metavar="CODE",
        help="只展开指定主键；指定后自动分页搜索主表，可重复",
    )
    parser.add_argument("--limit", type=int, default=1, help="最多展开的主表行")
    parser.add_argument(
        "--master-all-pages",
        action="store_true",
        help="完整分页下载主表；不影响每个明细请求",
    )
    parser.add_argument("--page-size", type=int, default=20)
    parser.add_argument("--max-pages", type=int, default=100)
    parser.add_argument("--base-url", default=tqlex.DEFAULT_BASE_URL)
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--output", type=Path, help="保存 UTF-8 JSON")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.list:
            print(list_workflows())
            return 0
        if not args.workflow:
            raise WorkflowError("provide --workflow or --list")
        root = updater.find_tdx_root(args.root)
        parameters = workflow_defaults(args.workflow)
        parameters.update(pbrpc.parse_assignments(args.set))
        result = run_workflow(
            root,
            WORKFLOWS[args.workflow],
            parameters,
            selected=args.select,
            limit=args.limit,
            master_all_pages=args.master_all_pages,
            page_size=args.page_size,
            max_pages=args.max_pages,
            base_url=args.base_url,
            timeout=args.timeout,
            verbose=args.verbose,
        )
        rendered = json.dumps(result, ensure_ascii=False, indent=2)
        if args.output:
            output = args.output.expanduser().resolve()
            updater.atomic_write_text(output, rendered + "\n", "utf-8")
            print(f"已保存：{output}", file=sys.stderr)
        else:
            print(rendered)
    except (
        OSError,
        UnicodeError,
        ValueError,
        json.JSONDecodeError,
        urllib.error.URLError,
        updater.UpdateError,
        pbrpc.PBRPCError,
        tqlex.TQLEXError,
        WorkflowError,
    ) as error:
        print(f"通达信工作流失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
