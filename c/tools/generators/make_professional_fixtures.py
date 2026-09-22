"""Generate c/tests/professional_fixtures.h from the real .dat samples.

The samples are 380 KB to 1.1 MB each, far too much to embed, so the fixture keeps a
verbatim PREFIX of two of them - the stock file and the board file, which between them
cover a named id, an unnamed id and a file whose every id is named - together with the
source file's size and md5 so the slice is traceable to what it came from.

The full-file facts (record counts, id sets, date ranges) are measured by
output/verify_professional_samples.py and live in the evidence file rather than in the
fixture, because a prefix cannot carry them.

    python c/tools/generators/make_professional_fixtures.py
"""
import hashlib
import io
import json
import os

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
ROOT = str(OUTPUT)
TARGET = str(OUTPUT / "c/tests/professional_fixtures.h")
CACHE = str(INPUT)
RECORD = 13
KEEP = 24  # records


def c_literals(data, per_line=16):
    pieces = []
    for index in range(0, len(data), per_line):
        chunk = data[index:index + per_line]
        pieces.append('    ' + ', '.join('0x%02x' % byte for byte in chunk) + ',')
    return pieces


SOURCES = [('stock', 'gpsz000001.dat'), ('board', 'gpsh880471.dat')]

out = [
    '/* professional_fixtures.h - prefixes of real professional-data files, GENERATED.',
    ' * Do not hand edit.',
    ' *',
    ' * Produced by c/tools/generators/make_professional_fixtures.py from the .dat files the',
    ' * reference implementation left in its own cache; each entry records the source',
    ' * md5 and size so the prefix is traceable to the whole file.  The full file cannot',
    ' * be embedded - 380 KB to 1.1 MB - and the full-file facts (record counts, id sets,',
    ' * date ranges) are in output/professional_verification_evidence.txt instead.',
    ' *',
    ' * The two files are chosen for what they show: the stock prefix carries both ids the',
    ' * 44-field table names and ids it does not, and every id in the board file is named.',
    ' */',
    '#ifndef TDX_TEST_PROFESSIONAL_FIXTURES_H',
    '#define TDX_TEST_PROFESSIONAL_FIXTURES_H',
    '',
]
for name, filename in SOURCES:
    path = os.path.join(CACHE, filename)
    if not os.path.exists(path):
        raise SystemExit('missing sample %s' % path)
    raw = open(path, 'rb').read()
    prefix = raw[:KEEP * RECORD]
    digest = source_info(os.path.basename(path))['source_md5']
    out.append('/* %s: %d bytes, md5 %s; prefix of %d records (%d bytes). */' % (
        filename, source_info(os.path.basename(path))['source_bytes'], digest, KEEP, len(prefix)))
    out.append('static const unsigned char professional_%s_prefix[] = {' % name)
    out.extend(c_literals(prefix))
    out.append('};')
    out.append('')
    out.append('#define PROFESSIONAL_%s_PREFIX_BYTES %d' % (name.upper(), len(prefix)))
    out.append('#define PROFESSIONAL_%s_PREFIX_RECORDS %d' % (name.upper(), KEEP))
    out.append('#define PROFESSIONAL_%s_SOURCE_BYTES %d' % (name.upper(), source_info(os.path.basename(path))['source_bytes']))
    out.append('#define PROFESSIONAL_%s_SOURCE_MD5 "%s"' % (name.upper(), digest))
    out.append('')
    print('%-6s %-16s %d bytes md5 %s -> %d-byte prefix' % (name, filename, source_info(os.path.basename(path))['source_bytes'], digest,
                                                            len(prefix)))
out.append('#endif /* TDX_TEST_PROFESSIONAL_FIXTURES_H */')
open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
print('wrote', os.path.normpath(TARGET))
