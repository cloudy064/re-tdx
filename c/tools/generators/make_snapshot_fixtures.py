"""Generate c/tests/snapshot_fixtures.h from the captured 0x054C reply.

    python c/tools/generators/make_snapshot_fixtures.py
"""
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE = os.path.join(HERE, 'snapshot_probe_evidence.txt')
TARGET = str(OUTPUT / "c/tests/snapshot_fixtures.h")


def main():
    text = open(SOURCE, encoding='utf-8').read()
    lines = text.splitlines()
    declared = None
    tokens = []
    for index, line in enumerate(lines):
        match = re.match(r'\s*C fixture \((\d+) bytes\):', line)
        if match:
            declared = int(match.group(1))
            # Skip the blank line the probe leaves after the header, then consume
            # only lines that are purely hex tokens, so a following line like
            # "0x054C parsed 3 records" cannot leak a token in.
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
    count = int(tokens[2], 16) | (int(tokens[3], 16) << 8)
    out = [
        '/* snapshot_fixtures.h - a live 0x054C reply, GENERATED. Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_snapshot_fixtures.py from',
        ' * output/snapshot_probe_evidence.txt, which output/dbg_snapshot_probe.c wrote',
        ' * from the bytes a public node returned for sz000623, sz000001 and sh600000.',
        ' * Regenerate the pair together; editing these bytes by hand would turn a',
        ' * captured fixture into an invented one. */',
        '#ifndef TDX_TEST_SNAPSHOT_FIXTURES_H',
        '#define TDX_TEST_SNAPSHOT_FIXTURES_H',
        '',
        '#include <stdint.h>',
        '',
        '/* %d bytes, %d records. */' % (declared, count),
        'static const uint8_t snapshot_reply[] = {',
    ]
    for index in range(0, len(tokens), 14):
        out.append('    %s,' % ', '.join('0x' + t for t in tokens[index:index + 14]))
    out.append('};')
    out.append('')
    out.append('#endif /* TDX_TEST_SNAPSHOT_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('snapshot_reply  %d bytes, %d records' % (declared, count))
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
