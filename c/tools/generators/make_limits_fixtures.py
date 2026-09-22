"""Generate c/tests/limits_fixtures.h from the captured 0x0452 first page.

The first page is what the server really returns: ONE record, 15 bytes.  That is
the shape the port has to handle, so it is the shape the fixture captures.  A
multi-record page is also built synthetically inside the test, clearly labelled,
because the parser is written to accept `count` records even though this server
answers one at a time.

    python c/tools/generators/make_limits_fixtures.py
"""
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE = os.path.join(HERE, 'limits_probe_evidence.txt')
TARGET = str(OUTPUT / "c/tests/limits_fixtures.h")
RECORD = 13


def main():
    lines = open(SOURCE, encoding='utf-8').read().splitlines()
    declared = None
    tokens = []
    for index, line in enumerate(lines):
        match = re.match(r'\s*C fixture \(first page, (\d+) bytes\):', line)
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
    if declared != 2 + count * RECORD:
        raise SystemExit('the captured page does not satisfy 2 + count * %d' % RECORD)

    out = [
        '/* limits_fixtures.h - a live 0x0452 page, GENERATED. Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_limits_fixtures.py from',
        ' * output/limits_probe_evidence.txt, which output/dbg_limits_probe.c wrote from',
        ' * the bytes a public node returned for a request from index 0.',
        ' *',
        ' * The page holds ONE record, because that is what this command answers: the',
        ' * server returns a single row per request and the client advances by explicit',
        ' * index.  A fixture with several records would not be a capture of anything.',
        ' *',
        ' * The record is SZ000010: up 1.74, down 1.42.  Its midpoint, 1.58, is the',
        ' * previous close the 0x054C command reported for the same security at the same',
        ' * time, which is what makes it a readable sample rather than a plausible one. */',
        '#ifndef TDX_TEST_LIMITS_FIXTURES_H',
        '#define TDX_TEST_LIMITS_FIXTURES_H',
        '',
        '#include <stdint.h>',
        '',
        '/* %d bytes: a 2-byte count + %d record of %d. */' % (declared, count, RECORD),
        'static const uint8_t limits_page[] = {',
    ]
    for index in range(0, len(tokens), 14):
        out.append('    %s,' % ', '.join('0x' + t for t in tokens[index:index + 14]))
    out.append('};')
    out.append('')
    out.append('#define LIMITS_PAGE_COUNT %d' % count)
    out.append('')
    out.append('#endif /* TDX_TEST_LIMITS_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('limits_page  %d bytes, %d record' % (declared, count))
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
