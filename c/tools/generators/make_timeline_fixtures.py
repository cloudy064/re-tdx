"""Generate c/tests/timeline_fixtures.h from the captured 0x0537 reply.

The reply bytes come from output/timeline_0537_reply.bin, which was extracted
from the probe transcript (output/timeline_probe_evidence.txt, block
"0x0537 u8 market,0,code,u16 start,u16 count").  Keeping the generation
mechanical means the test reads the server's stream, not a re-derivation.

    python c/tools/generators/make_timeline_fixtures.py
"""
import os

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE = os.path.join(HERE, 'timeline_0537_reply.bin')
TARGET = str(OUTPUT / "c/tests/timeline_fixtures.h")


def main():
    data = open(SOURCE, 'rb').read()
    if len(data) < 4:
        raise SystemExit('captured reply is too short: %d bytes' % len(data))
    count = data[0] | (data[1] << 8)
    out = [
        '/* timeline_fixtures.h - a live 0x0537 reply, GENERATED. Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_timeline_fixtures.py from',
        ' * output/timeline_0537_reply.bin, the reply a public node returned for',
        ' * sz000623.  Regenerate the pair together; editing these bytes by hand',
        ' * would turn a captured fixture into an invented one. */',
        '#ifndef TDX_TEST_TIMELINE_FIXTURES_H',
        '#define TDX_TEST_TIMELINE_FIXTURES_H',
        '',
        '#include <stdint.h>',
        '',
        '/* %d bytes, %d points, header %d/%d. */' % (len(data), count, data[0] | (data[1] << 8),
                                                     data[2] | (data[3] << 8)),
        'static const uint8_t today_reply[] = {',
    ]
    for index in range(0, len(data), 14):
        row = ', '.join('0x%02x' % b for b in data[index:index + 14])
        out.append('    %s,' % row)
    out.append('};')
    out.append('')
    out.append('#endif /* TDX_TEST_TIMELINE_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('wrote %s (%d bytes, %d points)' % (os.path.normpath(TARGET), len(data), count))


if __name__ == '__main__':
    main()
