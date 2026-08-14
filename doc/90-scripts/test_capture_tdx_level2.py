from __future__ import annotations

import hashlib
import importlib.util
import io
from pathlib import Path
import tempfile
import types
import unittest
from contextlib import redirect_stderr


MODULE_PATH = Path(__file__).with_name("capture_tdx_level2.py")
SPEC = importlib.util.spec_from_file_location("capture_tdx_level2", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CaptureTdxLevel2Tests(unittest.TestCase):
    def test_sha256_file(self) -> None:
        content = b"tdx-level2-probe\n"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.bin"
            path.write_bytes(content)
            self.assertEqual(MODULE.sha256_file(path), hashlib.sha256(content).hexdigest())

    def test_build_agent_source_injects_configuration(self) -> None:
        rendered = MODULE.build_agent_source(
            '"use strict";\n',
            {"targetCode": "600000", "maxEvents": 7},
        )
        self.assertTrue(rendered.startswith("globalThis.TDX_L2_CONFIG="))
        self.assertIn('"targetCode":"600000"', rendered)
        self.assertTrue(rendered.endswith('"use strict";\n'))

    def test_select_process_by_name_and_pid(self) -> None:
        first = types.SimpleNamespace(pid=11, name="Other.exe")
        target = types.SimpleNamespace(pid=22, name="tdxw.exe")
        self.assertIs(MODULE.select_process([first, target], None, "TdxW.exe"), target)
        self.assertIs(MODULE.select_process([first, target], 11, "TdxW.exe"), first)

    def test_select_process_rejects_ambiguous_name(self) -> None:
        processes = [
            types.SimpleNamespace(pid=21, name="TdxW.exe"),
            types.SimpleNamespace(pid=22, name="tdxw.exe"),
        ]
        with self.assertRaisesRegex(RuntimeError, "--pid"):
            MODULE.select_process(processes, None, "TdxW.exe")

    def test_argument_bounds(self) -> None:
        arguments = MODULE.parse_arguments(
            [
                "--code", "600000", "--label", "order_queue",
                "--max-events", "10", "--raw-sample-bytes", "0",
            ]
        )
        self.assertEqual(arguments.code, "600000")
        self.assertEqual(arguments.label, "order_queue")
        self.assertEqual(arguments.max_events, 10)
        self.assertEqual(arguments.raw_sample_bytes, 0)
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            MODULE.parse_arguments(["--raw-sample-bytes", "513"])

    def test_eventbus_registry_requires_explicit_flag(self) -> None:
        self.assertFalse(MODULE.parse_arguments([]).eventbus_registry)
        self.assertTrue(
            MODULE.parse_arguments(["--eventbus-registry"]).eventbus_registry
        )

    def test_agent_diagnostics_are_not_reported_as_success(self) -> None:
        self.assertTrue(MODULE.is_agent_diagnostic({"event": "probe-error"}))
        self.assertTrue(MODULE.is_agent_diagnostic({"event": "hook-refused"}))
        self.assertFalse(MODULE.is_agent_diagnostic({"event": "ready"}))


if __name__ == "__main__":
    unittest.main()
