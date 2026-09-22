"""Generate c/tests/capital_fixtures.h from the captured 0x000F reply.

    python c/tools/generators/make_capital_fixtures.py
"""
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE = os.path.join(HERE, 'capital_probe_evidence.txt')
TARGET = str(OUTPUT / "c/tests/capital_fixtures.h")
RECORD = 29
HEADER = 11


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
    count = int(tokens[9], 16) | (int(tokens[10], 16) << 8)
    if count != (declared - HEADER) // RECORD:
        raise SystemExit('count %d disagrees with %d bytes' % (count, declared))
    out = [
        '/* capital_fixtures.h - a live 0x000F reply, GENERATED. Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_capital_fixtures.py from',
        ' * output/capital_probe_evidence.txt, which output/dbg_capital_probe.c wrote',
        ' * from the bytes a public node returned for sz000001.  The reply is Ping An',
        ' * Bank\'s whole share-capital history, and it is the fixture that carries the',
        ' * 2024 "10 for 7.19" dividend the C++ reference recorded.  Regenerate the pair',
        ' * together; editing these bytes by hand would turn a captured fixture into an',
        ' * invented one. */',
        '#ifndef TDX_TEST_CAPITAL_FIXTURES_H',
        '#define TDX_TEST_CAPITAL_FIXTURES_H',
        '',
        '#include <stdint.h>',
        '',
        '/* %d bytes: %d-byte header + %d records of %d. */' % (declared, HEADER, count, RECORD),
        'static const uint8_t capital_reply[] = {',
    ]
    for index in range(0, len(tokens), 14):
        out.append('    %s,' % ', '.join('0x' + t for t in tokens[index:index + 14]))
    out.append('};')
    out.append('')
    out.append('/* The record count the header declares, so a test can assert it without')
    out.append(' * re-deriving it. */')
    out.append('#define CAPITAL_FIXTURE_COUNT %d' % count)
    out.append('')
    out.append('#endif /* TDX_TEST_CAPITAL_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('capital_reply  %d bytes, %d records' % (declared, count))
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
