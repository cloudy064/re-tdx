"""Generate c/tests/industry_fixtures.h from the captured resource.

The resource is 9 MB over 5,567 rows, so the fixture is a slice - but a slice chosen so the
property the module is built on survives it: ALL rows of the industries kept, not the first
N rows overall.  Keeping a prefix of the file would cut industries in half and make the
declared-versus-measured comparison fail in the fixture while passing on the real file,
which would be a fixture lying about the format.
"""
import hashlib
import io
import json
import os

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
ROOT = str(OUTPUT)
TARGET = str(OUTPUT / "c/tests/industry_fixtures.h")
KEEP_INDUSTRIES = 3


def c_literals(text, per_line=100):
    pieces = []
    for index in range(0, len(text), per_line):
        chunk = text[index:index + per_line].replace('\\', '\\\\').replace('"', '\\"')
        pieces.append('    "%s"' % chunk)
    return pieces


raw = open(os.path.join(HERE, 'hyzt.gbk'), 'rb').read()
document = json.loads(raw.decode('gbk'))
group = document[0]
headers = group['colheader']
index = {name: position for position, name in enumerate(headers)}
rows = group['data']

# The first few industries in the file's own order, with every one of their rows.
wanted = []
for row in rows:
    code = row[index['$ZQDM1']].strip()
    if code not in wanted:
        if len(wanted) >= KEEP_INDUSTRIES:
            break
        wanted.append(code)
kept = [row for row in rows if row[index['$ZQDM1']].strip() in wanted]
print('keeping industries %s: %d of %d rows' % (wanted, len(kept), source_info('hyzt.gbk')['source_rows']))

serialised = json.dumps([{'colheader': headers, 'data': kept}], ensure_ascii=False,
                        separators=(',', ':'))
out = [
    '/* industry_fixtures.h - the industry valuation resource, reduced. GENERATED.',
    ' * Do not hand edit.',
    ' *',
    ' * Produced by c/tools/generators/make_industry_fixtures.py from output/hyzt.gbk, which',
    ' * output/dbg_jsn_probe.c wrote from the bytes a public node returned for',
    ' * bi/list/func_gx_hyzt101_1.jsn (md5 %s, %d rows, 9 columns).' % (
        source_info('hyzt.gbk')['source_md5'], source_info('hyzt.gbk')['source_rows']),
    ' *',
    ' * The slice keeps EVERY ROW of %d industries rather than a prefix of the file.' % KEEP_INDUSTRIES,
    ' * A prefix would cut industries in half, and then the property this module rests on -',
    ' * the declared member list agreeing with the rows themselves - would fail in the',
    ' * fixture while holding on the real file.  A fixture that lies about the format is',
    ' * worse than a smaller one.',
    ' *',
    ' * As UTF-8, which is what the parser sees after the GBK conversion. */',
    '#ifndef TDX_TEST_INDUSTRY_FIXTURES_H',
    '#define TDX_TEST_INDUSTRY_FIXTURES_H',
    '',
    '/* %d columns, %d of %d rows kept. */' % (len(headers), len(kept), source_info('hyzt.gbk')['source_rows']),
    'static const char industry_resource[] =',
]
out.extend(c_literals(serialised))
out.append(';')
out.append('')
out.append('#define INDUSTRY_FIXTURE_ROWS %d' % len(kept))
out.append('#define INDUSTRY_FIXTURE_COLUMNS %d' % len(headers))
out.append('#define INDUSTRY_FIXTURE_SOURCE_ROWS %d' % source_info('hyzt.gbk')['source_rows'])
out.append('#define INDUSTRY_FIXTURE_INDUSTRIES %d' % len(wanted))
for position, code in enumerate(wanted):
    count = len([row for row in kept if row[index['$ZQDM1']].strip() == code])
    out.append('#define INDUSTRY_FIXTURE_CODE_%d "%s" /* %d rows */' % (position, code, count))
out.append('')
out.append('#endif /* TDX_TEST_INDUSTRY_FIXTURES_H */')
open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
print('wrote %s (%d bytes of JSON)' % (os.path.normpath(TARGET), len(serialised)))

print()
print('the rows the test will assert:')
for position, code in enumerate(wanted):
    subset = [row for row in kept if row[index['$ZQDM1']].strip() == code]
    declared = len(subset[0][index['$S_ZQDM']].split(','))
    print('  %s: %d rows, declared %d, name %r, pe %r, pb %r' % (
        code, len(subset), declared, subset[0][index['TDXHY']], subset[0][index['hyPE']],
        subset[0][index['hyPB']]
        if False else subset[0][index['hyPB']]))
    print('     first stock %s/%s, themes %d pieces' % (
        subset[0][index['$SC']], subset[0][index['$ZQDM']],
        len([p for p in subset[0][index['sszt']].split(',') if p.strip()])))
