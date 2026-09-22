"""Generate c/tests/finance_fixtures.h from the captured 0x0010 reply.

    python c/tools/generators/make_finance_fixtures.py
"""
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE = os.path.join(HERE, 'finance_probe_evidence.txt')
TARGET = str(OUTPUT / "c/tests/finance_fixtures.h")
RECORD = 143


def main():
    lines = open(SOURCE, encoding='utf-8').read().splitlines()
    declared = None
    tokens = []
    for index, line in enumerate(lines):
        match = re.match(r'\s*C fixture \((\d+) bytes\):', line)
        if not match:
            continue
        declared = int(match.group(1))
        cursor = index + 1
        while cursor < len(lines) and not lines[cursor].strip():
            cursor += 1
        while cursor < len(lines) and re.match(r'^(\s*0x[0-9a-f]{2},\s*)+$', lines[cursor]):
            tokens.extend(re.findall(r'0x([0-9a-f]{2}),', lines[cursor]))
            cursor += 1
        break
    if declared is None:
        raise SystemExit('no captured fixture in %s' % SOURCE)
    if len(tokens) != declared:
        raise SystemExit('%d tokens for a declared %d bytes' % (len(tokens), declared))
    count = int(tokens[0], 16) | (int(tokens[1], 16) << 8)
    if count != (declared - 2) // RECORD:
        raise SystemExit('count %d disagrees with %d bytes' % (count, declared))
    out = [
        '/* finance_fixtures.h - a live 0x0010 reply, GENERATED. Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_finance_fixtures.py from',
        ' * output/finance_probe_evidence.txt, which output/dbg_finance_probe.c wrote',
        ' * from the bytes a public node returned for sh600000, sz000001, sh601398,',
        ' * sz000002, sh600519 and sz300750.  Regenerate the pair together; editing',
        ' * these bytes by hand would turn a captured fixture into an invented one. */',
        '#ifndef TDX_TEST_FINANCE_FIXTURES_H',
        '#define TDX_TEST_FINANCE_FIXTURES_H',
        '',
        '#include <stdint.h>',
        '',
        '/* %d bytes, %d records of %d. */' % (declared, count, RECORD),
        'static const uint8_t finance_reply[] = {',
    ]
    for index in range(0, len(tokens), 14):
        out.append('    %s,' % ', '.join('0x' + t for t in tokens[index:index + 14]))
    out.append('};')
    out.append('')
    out.append('#endif /* TDX_TEST_FINANCE_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('finance_reply  %d bytes, %d records' % (declared, count))
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
