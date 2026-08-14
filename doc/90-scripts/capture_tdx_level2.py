#!/usr/bin/env python3
"""Capture passive TDX Level2 observations as bounded JSON Lines.

The launcher checks the running executable's SHA-256 before it injects the
Frida observer.  Without ``--attach`` it only performs this read-only preflight.
No session is created and no request is sent by either the launcher or agent.
"""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import sys
import threading
import time
from typing import Any, Iterable


KNOWN_TDXW_SHA256 = {
    "f5f2e6025a4d80bb1afbcb2a51c753d3c1909e09aa9d1bc8f7c3b701b081f74c"
}
KNOWN_TPBUS_SHA256 = {
    "0b6576270baf5b8421df7c282820306dcaee382b90f8b04ffd7e1075ebbcb481"
}
CODE_PATTERN = re.compile(r"^[0-9A-Za-z._-]{1,22}$")
AGENT_DIAGNOSTIC_EVENTS = {
    "hook-refused",
    "probe-error",
    "eventbus-registry-refused",
    "module-version-mismatch",
}


def is_agent_diagnostic(payload: dict[str, Any]) -> bool:
    return payload.get("event") in AGENT_DIAGNOSTIC_EVENTS


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def process_image_path(process_id: int) -> Path | None:
    """Return a Windows process image path without attaching to the process."""

    if sys.platform != "win32":
        return None
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    open_process = kernel32.OpenProcess
    open_process.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    open_process.restype = wintypes.HANDLE
    query_path = kernel32.QueryFullProcessImageNameW
    query_path.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    query_path.restype = wintypes.BOOL
    close_handle = kernel32.CloseHandle
    close_handle.argtypes = [wintypes.HANDLE]
    close_handle.restype = wintypes.BOOL

    handle = open_process(0x1000, False, process_id)  # PROCESS_QUERY_LIMITED_INFORMATION
    if not handle:
        return None
    try:
        capacity = wintypes.DWORD(32768)
        buffer = ctypes.create_unicode_buffer(capacity.value)
        if not query_path(handle, 0, buffer, ctypes.byref(capacity)):
            return None
        return Path(buffer.value)
    finally:
        close_handle(handle)


def select_process(processes: Iterable[Any], process_id: int | None, name: str) -> Any:
    candidates = list(processes)
    if process_id is not None:
        for process in candidates:
            if process.pid == process_id:
                return process
        raise RuntimeError(f"没有找到 PID {process_id}")
    matches = [process for process in candidates if process.name.lower() == name.lower()]
    if not matches:
        raise RuntimeError(f"没有找到进程 {name}")
    if len(matches) > 1:
        identifiers = ", ".join(str(process.pid) for process in matches)
        raise RuntimeError(f"存在多个 {name} 进程（PID: {identifiers}），请使用 --pid")
    return matches[0]


def preflight(process: Any) -> dict[str, Any]:
    image_path = process_image_path(process.pid)
    report: dict[str, Any] = {
        "event": "preflight",
        "pid": process.pid,
        "process": process.name,
        "image_path": str(image_path) if image_path else None,
        "tdxw_sha256": None,
        "tdxw_recognized": False,
        "tpbus_path": None,
        "tpbus_sha256": None,
        "tpbus_recognized": None,
    }
    if image_path is None or not image_path.is_file():
        report["reason"] = "无法读取运行进程的可执行文件路径"
        return report
    executable_hash = sha256_file(image_path)
    report["tdxw_sha256"] = executable_hash
    report["tdxw_recognized"] = executable_hash in KNOWN_TDXW_SHA256

    tpbus_path = image_path.parent / "tpbus.dll"
    if tpbus_path.is_file():
        tpbus_hash = sha256_file(tpbus_path)
        report["tpbus_path"] = str(tpbus_path)
        report["tpbus_sha256"] = tpbus_hash
        report["tpbus_recognized"] = tpbus_hash in KNOWN_TPBUS_SHA256
    return report


def build_agent_source(agent_source: str, configuration: dict[str, Any]) -> str:
    rendered = json.dumps(configuration, ensure_ascii=True, separators=(",", ":"))
    return f"globalThis.TDX_L2_CONFIG={rendered};\n{agent_source}"


def default_output_path() -> Path:
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    return Path("output") / f"tdx-level2-{stamp}.jsonl"


def parse_arguments(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="被动捕获现有通达信 Level2 调用；默认只做版本预检，不附加进程。"
    )
    parser.add_argument("--attach", action="store_true", help="通过版本检查后附加并开始捕获")
    parser.add_argument("--process", default="TdxW.exe", help="目标进程名（默认 TdxW.exe）")
    parser.add_argument("--pid", type=int, help="目标 PID；同名进程超过一个时使用")
    parser.add_argument("--code", default="", help="只记录这一只证券代码，例如 600000")
    parser.add_argument("--label", default="", help="本次窗口标签，例如 transaction/order/depth/queue")
    parser.add_argument("--duration", type=float, default=60.0, help="捕获秒数；0 表示直到 Ctrl+C")
    parser.add_argument("--max-events", type=int, default=500, help="业务事件硬上限（默认 500）")
    parser.add_argument("--max-records", type=int, default=20, help="每个事件最多展开的记录数")
    parser.add_argument("--raw-sample-bytes", type=int, default=64, help="未知体最多保留的十六进制字节数")
    parser.add_argument("--correlation-window", type=float, default=300.0, help="LX 与推送的关联时间窗（秒）")
    parser.add_argument(
        "--eventbus-registry",
        action="store_true",
        help="显式调用只读 EventBus 单例 getter，快照现有主题订阅；不发送事件",
    )
    parser.add_argument("--output", type=Path, help="JSONL 输出路径；默认写入 output/tdx-level2-时间.jsonl")
    parser.add_argument(
        "--allow-unknown-version",
        action="store_true",
        help="显式允许未知主程序版本；仍要求每个 Hook 的机器码签名匹配",
    )
    arguments = parser.parse_args(argv)
    if arguments.code and not CODE_PATTERN.fullmatch(arguments.code):
        parser.error("--code 必须是 1 到 22 位 ASCII 证券代码")
    if len(arguments.label) > 64 or any(ord(character) < 32 for character in arguments.label):
        parser.error("--label 不能超过 64 个字符且不能包含控制字符")
    if arguments.duration < 0:
        parser.error("--duration 不能为负数")
    if not 1 <= arguments.max_events <= 100000:
        parser.error("--max-events 必须在 1..100000 之间")
    if not 0 <= arguments.max_records <= 1000:
        parser.error("--max-records 必须在 0..1000 之间")
    if not 0 <= arguments.raw_sample_bytes <= 512:
        parser.error("--raw-sample-bytes 必须在 0..512 之间")
    if not 1 <= arguments.correlation_window <= 3600:
        parser.error("--correlation-window 必须在 1..3600 秒之间")
    return arguments


def run(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        import frida  # type: ignore
    except ImportError:
        print("缺少 frida Python 包：python -m pip install frida-tools", file=sys.stderr)
        return 2

    try:
        device = frida.get_local_device()
        process = select_process(device.enumerate_processes(), arguments.pid, arguments.process)
        report = preflight(process)
    except Exception as error:
        print(f"预检失败：{error}", file=sys.stderr)
        return 2

    print(json.dumps(report, ensure_ascii=False))
    if not arguments.attach:
        print("仅完成预检；添加 --attach 才会加载只读探针。", file=sys.stderr)
        return 0 if report["tdxw_recognized"] else 1
    if not report["tdxw_recognized"] and not arguments.allow_unknown_version:
        print(
            "运行中的 TdxW.exe 不是已分析版本；拒绝附加。确认版本后可显式使用 --allow-unknown-version。",
            file=sys.stderr,
        )
        return 2

    agent_path = Path(__file__).with_name("frida_tdx_level2_probe.js")
    if not agent_path.is_file():
        print(f"找不到探针脚本：{agent_path}", file=sys.stderr)
        return 2
    configuration = {
        "allowUnknownVersion": bool(arguments.allow_unknown_version),
        "label": arguments.label,
        "targetCode": arguments.code,
        "maxEvents": arguments.max_events,
        "maxRecordsPerEvent": arguments.max_records,
        "rawSampleBytes": arguments.raw_sample_bytes,
        "correlationWindowMs": int(arguments.correlation_window * 1000),
        "snapshotEventBusRegistry": bool(arguments.eventbus_registry),
    }
    source = build_agent_source(agent_path.read_text(encoding="utf-8"), configuration)
    output_path = arguments.output or default_output_path()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    stop_event = threading.Event()
    counters = {"messages": 0, "errors": 0}
    session = None
    script = None
    try:
        with output_path.open("x", encoding="utf-8", newline="\n") as sink:
            def write_event(payload: dict[str, Any]) -> None:
                payload = dict(payload)
                payload["captured_at"] = datetime.now().astimezone().isoformat(timespec="milliseconds")
                payload["pid"] = process.pid
                sink.write(json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n")
                sink.flush()
                counters["messages"] += 1

            def on_message(message: dict[str, Any], data: bytes | None) -> None:
                if message.get("type") == "send":
                    payload = message.get("payload")
                    if not isinstance(payload, dict):
                        payload = {"event": "agent-message", "payload": payload}
                    if is_agent_diagnostic(payload):
                        counters["errors"] += 1
                    write_event(payload)
                    if payload.get("event") == "capture-limit":
                        stop_event.set()
                    return
                counters["errors"] += 1
                write_event({
                    "schema": "tdx-level2-probe/v1",
                    "event": "agent-error",
                    "message": message,
                    "data_size": len(data) if data else 0,
                })

            def on_detached(reason: str, crash: Any = None) -> None:
                counters["errors"] += int(reason != "application-requested")
                try:
                    if not sink.closed:
                        write_event({
                            "schema": "tdx-level2-probe/v1",
                            "event": "detached",
                            "reason": reason,
                            "crash": str(crash) if crash else None,
                        })
                finally:
                    stop_event.set()

            session = device.attach(process.pid)
            session.on("detached", on_detached)
            script = session.create_script(source)
            script.on("message", on_message)
            script.load()
            print(f"被动探针已附加，输出：{output_path.resolve()}", file=sys.stderr)

            if arguments.duration == 0:
                while not stop_event.wait(0.25):
                    pass
            else:
                deadline = time.monotonic() + arguments.duration
                while not stop_event.is_set() and time.monotonic() < deadline:
                    stop_event.wait(min(0.25, max(0.0, deadline - time.monotonic())))
    except FileExistsError:
        print(f"输出文件已存在，拒绝覆盖：{output_path}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("收到 Ctrl+C，停止捕获。", file=sys.stderr)
    except Exception as error:
        print(f"捕获失败：{error}", file=sys.stderr)
        return 2
    finally:
        if script is not None:
            try:
                script.unload()
            except Exception:
                pass
        if session is not None:
            try:
                session.detach()
            except Exception:
                pass

    print(
        json.dumps(
            {
                "event": "capture-summary",
                "output": str(output_path.resolve()),
                **counters,
            },
            ensure_ascii=False,
        )
    )
    return 0 if counters["errors"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(run())
