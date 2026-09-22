"""Generate c/tests/valuation_fixtures.h from the captured resources.

The master is 2,407 bytes over 11 rows, so it goes in WHOLE - real data, all of it.  The two
histories are 100 KB each, so they go in as prefixes; the merge test uses those prefixes for
the aligned-date case and synthetic rows for the cases a capture does not promise to
contain (a date only one side carries, and out-of-order input).
"""
import hashlib
import io
import json
import os

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
ROOT = str(OUTPUT)
TARGET = str(OUTPUT / "c/tests/valuation_fixtures.h")
KEEP = 20


def c_literals(text, per_line=100):
    pieces = []
    for index in range(0, len(text), per_line):
        chunk = text[index:index + per_line].replace('\\', '\\\\').replace('"', '\\"')
        pieces.append('    "%s"' % chunk)
    return pieces


def fetch(resource, name):
    filename = 'valuation_' + ('funds' if name == 'fund' else name) + '.json'
    raw = (INPUT / filename).read_bytes()
    group = json.loads(raw.decode('utf-8'))[0]
    return raw, [dict(zip(group['colheader'], row)) for row in group['data']]


def wrap(rows):
    """A JSN document holding these rows, keeping each row's own columns."""
    columns = [key for key in rows[0].keys()
               if key not in ('type', 'resource', 'group', 'row')]
    return json.dumps([{'colheader': columns,
                        'data': [[row.get(key, '') for key in columns] for row in rows]}],
                      ensure_ascii=False, separators=(',', ':'))


master_raw, master_rows = fetch('list/func_zsgz101_1.jsn', 'master')
pe_raw, pe_rows = fetch('zsgz3/000001.jsn', 'pe')
pb_raw, pb_rows = fetch('zsgz4/000001.jsn', 'pb')
fund_raw, fund_rows = fetch('zsgz1/000001.jsn', 'fund')

master_json = wrap(master_rows)
pe_json = wrap(pe_rows[:KEEP])
pb_json = wrap(pb_rows[:KEEP])
fund_json = wrap(fund_rows)

out = [
    '/* valuation_fixtures.h - the index valuation family, captured. GENERATED.',
    ' * Do not hand edit.',
    ' *',
    ' * Produced by c/tools/generators/make_valuation_fixtures.py from the bytes a public node returned',
    ' * for four resources:',
    ' *',
    ' *   list/func_zsgz101_1.jsn  %d bytes, %d rows  ALL of them - it is small' % (
        source_info('valuation_master.json')['source_bytes'], source_info('valuation_master.json')['source_rows']),
    ' *   zsgz3/000001.jsn        %d bytes, %d rows  first %d' % (source_info('valuation_pe.json')['source_bytes'], source_info('valuation_pe.json')['source_rows'],
                                                                 KEEP),
    ' *   zsgz4/000001.jsn        %d bytes, %d rows  first %d' % (source_info('valuation_pb.json')['source_bytes'], source_info('valuation_pb.json')['source_rows'],
                                                                 KEEP),
    ' *   zsgz1/000001.jsn        %d bytes, %d rows  all of them' % (source_info('valuation_funds.json')['source_bytes'],
                                                                    source_info('valuation_funds.json')['source_rows']),
    ' *',
    ' * The two histories have the SAME row count live - 3,093 each - which is why the merge',
    ' * is checkable; the prefixes keep the same 20 dates on both sides so the aligned case',
    ' * survives the reduction.  The unaligned cases are built in the test instead, since a',
    ' * capture does not promise to contain a date that only one side carries.',
    ' *',
    ' * As UTF-8, which is what the parser sees after the GBK conversion. */',
    '#ifndef TDX_TEST_VALUATION_FIXTURES_H',
    '#define TDX_TEST_VALUATION_FIXTURES_H',
    '',
]
for name, text, source_rows, kept in (('master', master_json, source_info('valuation_master.json')['source_rows'],
                                        source_info('valuation_master.json')['source_rows']),
                                       ('pe', pe_json, source_info('valuation_pe.json')['source_rows'], KEEP),
                                       ('pb', pb_json, source_info('valuation_pb.json')['source_rows'], KEEP),
                                       ('funds', fund_json, source_info('valuation_funds.json')['source_rows'], source_info('valuation_funds.json')['source_rows'])):
    out.append('/* %s: %d of %d rows. */' % (name, kept, source_rows))
    out.append('static const char valuation_%s[] =' % name)
    out.extend(c_literals(text))
    out.append(';')
    out.append('')
    out.append('#define VALUATION_%s_ROWS %d' % (name.upper(), kept))
    out.append('#define VALUATION_%s_SOURCE_ROWS %d' % (name.upper(), source_rows))
    out.append('')
out.append('#define VALUATION_HISTORY_PREFIX_ROWS %d' % KEEP)
out.append('')
out.append('#endif /* TDX_TEST_VALUATION_FIXTURES_H */')
open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
print('master %d rows, pe %d of %d, pb %d of %d, funds %d'
      % (source_info('valuation_master.json')['source_rows'], KEEP, source_info('valuation_pe.json')['source_rows'], KEEP, source_info('valuation_pb.json')['source_rows'], source_info('valuation_funds.json')['source_rows']))

print()
print('the master rows the test asserts:')
for record in master_rows[:3]:
    print('  %s/%s detail %s pe %s pefws %s pb %s pbfws %s label %s'
          % (record['$SC1'], record['$ZQDM1'], record['$ZQDM'], record['pe'],
             record['pefws'], record['pb'], record['pbfws'], record['gzsp']))
print()
print('the history prefixes:')
for label, rows in (('pe', pe_rows[:KEEP]), ('pb', pb_rows[:KEEP])):
    print('  %s: %s .. %s (%d rows)' % (label, rows[0]['date'], rows[-1]['date'], len(rows)))
    print('     first %s' % json.dumps(rows[0], ensure_ascii=False))
print()
print('and the last live date, which the master should agree with:')
print('  master date %s pe %s pb %s' % (master_rows[0]['date'], master_rows[0]['pe'],
                                        master_rows[0]['pb']))
print('  pe history last %s pe %s' % (pe_rows[-1]['date'], pe_rows[-1]['pe']))
print('  pb history last %s pb %s' % (pb_rows[-1]['date'], pb_rows[-1]['pb']))
