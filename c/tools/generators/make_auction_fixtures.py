"""Generate c/tests/auction_fixtures.h from the captured 0x056A replies.

Both replies come from output/auction_probe_evidence.txt, which
output/dbg_auction_probe.c wrote from the bytes a public node returned.

    python c/tools/generators/make_auction_fixtures.py
"""
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE = os.path.join(HERE, 'auction_probe_evidence.txt')
TARGET = str(OUTPUT / "c/tests/auction_fixtures.h")

WANTED = [
    ('opening_reply', 'opening only (selector 0)'),
    ('combined_reply', 'opening and closing (selector 1)'),
]


def main():
    text = open(SOURCE, encoding='utf-8').read()
    blocks = re.split(r'=== ', text)[1:]
    out = [
        '/* auction_fixtures.h - live 0x056A replies, GENERATED. Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_auction_fixtures.py from',
        ' * output/auction_probe_evidence.txt, which output/dbg_auction_probe.c wrote',
        ' * from the bytes a public node returned for sz000623.  Regenerate the pair',
        ' * together; editing these bytes by hand would turn a captured fixture into',
        ' * an invented one. */',
        '#ifndef TDX_TEST_AUCTION_FIXTURES_H',
        '#define TDX_TEST_AUCTION_FIXTURES_H',
        '',
        '#include <stdint.h>',
        '',
    ]
    for name, needle in WANTED:
        block = next((b for b in blocks if needle in b.splitlines()[0]), None)
        if block is None:
            raise SystemExit('no captured block matching %r' % needle)
        match = re.search(r'C fixture \((\d+) bytes\):\s*\n((?:\s*0x[0-9a-f]{2},?)+)', block, re.S)
        if not match:
            raise SystemExit('no C fixture in the block for %r' % needle)
        declared = int(match.group(1))
        tokens = re.findall(r'0x([0-9a-f]{2})', match.group(2))
        if len(tokens) != declared:
            raise SystemExit('%s: %d tokens for a declared %d bytes' % (name, len(tokens), declared))
        points = (declared - 2) // 16
        out.append('/* %s: %d bytes, %d points. */' % (needle, declared, points))
        out.append('static const uint8_t %s[] = {' % name)
        for index in range(0, len(tokens), 14):
            out.append('    %s,' % ', '.join('0x' + t for t in tokens[index:index + 14]))
        out.append('};')
        out.append('')
        print('%-16s %d bytes, %d points' % (name, declared, points))
    out.append('#endif /* TDX_TEST_AUCTION_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
