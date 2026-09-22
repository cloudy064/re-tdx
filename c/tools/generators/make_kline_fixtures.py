"""Generate c/tests/kline_fixtures.h from the captured 0x052D replies.

The probe prints each reply as a C initializer; this extracts those bytes
verbatim so the test fixture is the server's actual stream rather than anything
re-derived from the decoded values.  Run it after re-running the probe.

    python c/tools/generators/make_kline_fixtures.py
"""
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE = os.path.join(HERE, 'kline_probe_evidence.txt')
TARGET = str(OUTPUT / "c/tests/kline_fixtures.h")

WANTED = [
    ('daily_reply', 'sz000623 day', 'day'),
    ('minute_reply', 'sz000623 1m', '1m'),
    ('index_reply', 'sh000001 day', 'index'),
]


def main():
    text = open(SOURCE, encoding='utf-8').read()
    blocks = re.split(r'--- \d\) ', text)[1:]
    out = [
        '/* kline_fixtures.h - live 0x052D replies, GENERATED. Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_kline_fixtures.py from output/kline_probe_evidence.txt,',
        ' * which output/dbg_kline_probe.c wrote from the bytes a public node returned.',
        ' * Regenerate both together; editing these bytes by hand would turn a captured',
        ' * fixture into an invented one. */',
        '#ifndef TDX_TEST_KLINE_FIXTURES_H',
        '#define TDX_TEST_KLINE_FIXTURES_H',
        '',
        '#include <stdint.h>',
        '',
    ]
    produced = []
    for name, needle, label in WANTED:
        block = next((b for b in blocks if needle in b.splitlines()[0]), None)
        if block is None:
            raise SystemExit('no captured block matching %r' % needle)
        match = re.search(r'C fixture \((\d+) bytes\):\s*\n(.*?)\n\n', block, re.S)
        if not match:
            raise SystemExit('no C fixture in the block for %r' % needle)
        declared = int(match.group(1))
        tokens = re.findall(r'0x([0-9a-f]{2})', match.group(2))
        if len(tokens) != declared:
            raise SystemExit('%s: %d tokens for a declared %d bytes' % (name, len(tokens), declared))
        out.append('/* %s, start=0, %s: %d bytes. */' % (label, needle, declared))
        out.append('static const uint8_t %s[] = {' % name)
        for index in range(0, len(tokens), 14):
            row = ', '.join('0x' + t for t in tokens[index:index + 14])
            out.append('    %s,' % row)
        out.append('};')
        out.append('')
        produced.append((name, declared))
    out.append('#endif /* TDX_TEST_KLINE_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    for name, count in produced:
        print('%-14s %d bytes' % (name, count))
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
