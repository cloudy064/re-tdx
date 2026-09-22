"""Locate the OHLC violations and build the fixture around them.

A fixture that merely omits the awkward records would not test the decision that matters:
real files DO break the range, so the reader counts rather than refuses.  So the fixture
is a window AROUND a real violation, and the test asserts the count is one rather than
zero.
"""
import hashlib
import io
import os
import struct

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
ROOT = str(OUTPUT)
TARGET = str(OUTPUT / "c/tests/minute_fixtures.h")
VIPDOC = str(INPUT)
RECORD = 32
WINDOW_BEFORE = 2
WINDOW = 10


def records(path):
    raw = open(path, 'rb').read()
    return [raw[offset:offset + RECORD] for offset in range(0, len(raw), RECORD)]


def violates(record):
    _, _, o, h, low, c = struct.unpack('<HHffff', record[:20])
    return h + 1e-5 < max(o, low, c) or low - 1e-5 > min(o, h, c)


# The two indices whose real records break the range, and a stock for the ordinary case.
finding = os.path.join(VIPDOC, 'sh000043.lc1')
ordinary = os.path.join(VIPDOC, 'sh600519.lc1')

violation_rows = records(finding)
violation_at = next(index for index, row in enumerate(violation_rows) if violates(row))
start = max(0, violation_at - WINDOW_BEFORE)
window = violation_rows[start:start + WINDOW]
print('sh000043: violation at record %d, window %d..%d' % (violation_at, start,
                                                           start + len(window) - 1))

ordinary_rows = records(ordinary)
plain = ordinary_rows[:WINDOW]
print('sh600519: first %d records, violations %d' % (len(plain),
                                                    sum(1 for row in plain if violates(row))))


def c_literals(data, per_line=16):
    lines = []
    for index in range(0, len(data), per_line):
        chunk = data[index:index + per_line]
        lines.append('    ' + ', '.join('0x%02x' % byte for byte in chunk) + ',')
    return lines


def emit(out, name, path, payload, total_records, source_bytes, source_md5):
    out.append('/* %s: %d records in the file, md5 %s; %d records kept. */' % (
        os.path.basename(path), total_records, source_md5, len(payload) // RECORD))
    out.append('static const unsigned char minute_%s[] = {' % name)
    out.extend(c_literals(b''.join(payload)))
    out.append('};')
    out.append('')
    out.append('#define MINUTE_%s_RECORDS %d' % (name.upper(), len(payload)))
    out.append('#define MINUTE_%s_BYTES %d' % (name.upper(), len(payload) * RECORD))
    out.append('#define MINUTE_%s_SOURCE_RECORDS %d' % (name.upper(), total_records))
    out.append('#define MINUTE_%s_SOURCE_MD5 "%s"' % (name.upper(), source_md5))
    out.append('')


out = [
    '/* minute_fixtures.h - windows of real .lc1 files, GENERATED. Do not hand edit.',
    ' *',
    ' * Produced by c/tools/generators/make_minute_fixtures.py from the terminal\'s own minline tree.',
    ' *',
    ' * Two windows, chosen for the decision they test rather than for being tidy:',
    ' *',
    ' *   violation  a window AROUND a record whose close is above its high - which the',
    ' *              reference refuses and real files contain.  A fixture without such a',
    ' *              record could not test the choice to count rather than refuse.',
    ' *   plain      the first records of a stock, whose trailing words are all zero.',
    ' */',
    '#ifndef TDX_TEST_MINUTE_FIXTURES_H',
    '#define TDX_TEST_MINUTE_FIXTURES_H',
    '',
]
emit(out, 'plain', ordinary, plain, source_info('sh600519.lc1')['source_records'],
     os.path.getsize(ordinary), source_info('sh600519.lc1')['source_md5'])
emit(out, 'violation', finding, window, source_info('sh000043.lc1')['source_records'],
     os.path.getsize(finding), source_info('sh000043.lc1')['source_md5'])
out.append('#define MINUTE_VIOLATION_AT %d' % (violation_at - start))
out.append('')
out.append('#endif /* TDX_TEST_MINUTE_FIXTURES_H */')
open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
print('wrote', os.path.normpath(TARGET))

# The values the test asserts, printed so they come from the bytes rather than from a guess.
print()
for label, payload in (('plain', plain), ('violation', window)):
    print('%s:' % label)
    for index, row in enumerate(payload):
        word, minute, o, h, low, c, amount, volume, e1, e2 = struct.unpack('<HHfffffIHH', row)
        year = word // 2048 + 2004
        rest = word % 2048
        print('  [%d] %04d%02d%02d %02d:%02d O %.3f H %.3f L %.3f C %.3f vol %d extra %d/%d%s'
              % (index, year, rest // 100, rest % 100, minute // 60, minute % 60, o, h, low, c,
                 volume, e1, e2, '  <-- violates' if violates(row) else ''))
