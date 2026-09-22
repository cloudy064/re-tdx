#!/usr/bin/env python3
"""Regenerate C tables/fixtures offline, or verify their checked-in forms (--check)."""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / 'c/tools'

def portable_strings(text):
    """Long C string literals exceed MSVC and ISO C implementation limits.

    Preserve UTF-8 bytes and their terminating NUL in a char array; original
    capture parsing, row selection and field rendering remain in each generator.
    Short literals are left intact for readability.
    """
    pattern = re.compile(r'(static const char\s+\w+\[\]\s*=)\s*((?:"(?:\\.|[^"\\])*"\s*)+);')
    def replace(match):
        value = ''.join(ast.literal_eval(token) for token in re.findall(r'"(?:\\.|[^"\\])*"', match[2]))
        raw = value.encode('utf-8')
        if len(raw) <= 4095:
            return match[0]
        raw += b'\0'
        lines = []
        for offset in range(0, len(raw), 20):
            # Explicit casts preserve byte patterns when plain char is signed.
            lines.append('    ' + ', '.join('(char)0x%02x' % byte for byte in raw[offset:offset+20]) + ',')
        return match[1] + ' {\n' + '\n'.join(lines) + '\n};'
    return pattern.sub(replace, text)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='fail on drift without changing sources')
    args = parser.parse_args()
    entries = json.loads((TOOLS / 'generated.json').read_text(encoding='utf-8'))
    sources = json.loads((ROOT / 'c/tests/fixtures/sources.json').read_text(encoding='utf-8'))
    for name, info in sources.items():
        raw = (ROOT / 'c/tests/fixtures/inputs' / name).read_bytes()
        if len(raw) != info['bytes'] or hashlib.sha256(raw).hexdigest() != info['sha256']:
            raise SystemExit('capture input changed: ' + name + '; review its provenance and manifest')
    with tempfile.TemporaryDirectory(prefix='tdx-generated-') as temporary:
        staging = Path(temporary)
        (staging / 'c/src').mkdir(parents=True)
        (staging / 'c/tests').mkdir(parents=True)
        env = dict(os.environ, TDX_GENERATED_ROOT=str(staging), PYTHONIOENCODING='utf-8')
        for entry in entries:
            result = subprocess.run([sys.executable, str(TOOLS / 'generators' / entry['script'])],
                                    env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if result.returncode:
                sys.stderr.write(result.stdout.decode('utf-8', errors='replace'))
                sys.stderr.write(result.stderr.decode('utf-8', errors='replace'))
                raise SystemExit('generator failed: ' + entry['script'])
        changed = []
        for entry in entries:
            target = ROOT / entry['output']
            text = portable_strings((staging / entry['output']).read_text(encoding='utf-8'))
            # Compare logical text so Git's configured checkout line endings do not
            # create drift on Windows; generated writes always use LF.
            if not target.exists() or target.read_text(encoding='utf-8') != text:
                changed.append(entry['output'])
                if not args.check:
                    with target.open('w', encoding='utf-8', newline='\n') as stream:
                        stream.write(text)
        if args.check and changed:
            raise SystemExit('generated files differ:\n' + '\n'.join(changed))
        print('%s %d generated files; %d inputs verified; %d files changed' %
              ('Checked' if args.check else 'Regenerated', len(entries), len(sources), len(changed)))

if __name__ == '__main__':
    main()
