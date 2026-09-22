"""Validate emitted C JSON with an independent standard-library decoder."""
import json
import subprocess
import sys


def reject_constant(value):
    raise ValueError("non-JSON numeric constant: " + value)


raw = subprocess.check_output([sys.argv[1], "--emit-json-vectors"])
actual = json.loads(raw.decode("utf-8"), parse_constant=reject_constant)
expected = ['a"b\\c\n\t\0z', '\u4e2d', '']
if actual != expected:
    raise SystemExit("C JSON output did not preserve the original string values")
print("independent JSON output checks passed")
