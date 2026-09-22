"""Generate c/tests/daily_fixtures.h from real .day files.

Two files, chosen because they are the two price scales that matter: a stock, whose
records are hundredths, and a convertible bond, whose records are ten-thousandths.  A
reader with a fixed scale gets one of them wrong by a hundredfold, so a fixture with only
one of them would not test the thing that is easy to get wrong.

Each entry keeps a verbatim prefix plus the source file's size and md5, so the slice is
traceable.  The full files are 43 KB and 190 KB; a prefix of ten records is 320 bytes and
carries the listing-day bar, which is the interesting one.

    python c/tools/generators/make_daily_fixtures.py
"""
import hashlib
import io
import os

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
ROOT = str(OUTPUT)
TARGET = str(OUTPUT / "c/tests/daily_fixtures.h")
VIPDOC = str(INPUT)
RECORD = 32
KEEP = 10

SOURCES = [('stock', 'sh', '600519'), ('bond', 'sh', '110075')]


def c_literals(data, per_line=16):
    lines = []
    for index in range(0, len(data), per_line):
        chunk = data[index:index + per_line]
        lines.append('    ' + ', '.join('0x%02x' % byte for byte in chunk) + ',')
    return lines


out = [
    '/* daily_fixtures.h - prefixes of real .day files, GENERATED. Do not hand edit.',
    ' *',
    ' * Produced by c/tools/generators/make_daily_fixtures.py from the .day files in the terminal\'s own',
    ' * vipdoc tree, each with its source md5 and size so the prefix is traceable.',
    ' *',
    ' * A stock and a convertible bond, because the two are stored at DIFFERENT scales -',
    ' * hundredths and ten-thousandths - and a reader with one fixed scale is wrong by a',
    ' * hundredfold on the other while still printing numbers.',
    ' */',
    '#ifndef TDX_TEST_DAILY_FIXTURES_H',
    '#define TDX_TEST_DAILY_FIXTURES_H',
    '',
]
for name, market, code in SOURCES:
    path = os.path.join(VIPDOC, '%s%s.day' % (market, code))
    if not os.path.exists(path):
        raise SystemExit('missing %s' % path)
    raw = open(path, 'rb').read()
    prefix = raw[:KEEP * RECORD]
    digest = source_info(os.path.basename(path))['source_md5']
    out.append('/* %s%s.day: %d bytes, md5 %s; prefix of %d records. */' % (
        market, code, source_info(os.path.basename(path))['source_bytes'], digest, KEEP))
    out.append('static const unsigned char daily_%s_prefix[] = {' % name)
    out.extend(c_literals(prefix))
    out.append('};')
    out.append('')
    out.append('#define DAILY_%s_PREFIX_BYTES %d' % (name.upper(), len(prefix)))
    out.append('#define DAILY_%s_PREFIX_RECORDS %d' % (name.upper(), KEEP))
    out.append('#define DAILY_%s_SOURCE_BYTES %d' % (name.upper(), source_info(os.path.basename(path))['source_bytes']))
    out.append('#define DAILY_%s_SOURCE_MD5 "%s"' % (name.upper(), digest))
    out.append('#define DAILY_%s_CODE "%s"' % (name.upper(), code))
    out.append('')
    print('%-6s %s%s.day %d bytes md5 %s -> %d-byte prefix' % (name, market, code, source_info(os.path.basename(path))['source_bytes'],
                                                               digest, len(prefix)))
out.append('#endif /* TDX_TEST_DAILY_FIXTURES_H */')
open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
print('wrote', os.path.normpath(TARGET))
