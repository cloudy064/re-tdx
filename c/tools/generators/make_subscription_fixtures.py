"""Widen the subscription fixture so the projection has something to match.

The fixture previously kept the first three rows, which the projection does not name, so
a reconciliation test against it would have matched nothing and passed vacuously.  It now
keeps those three AND the rows whose subscription code the captured projection carries,
which is what makes a real match testable.
"""
import io
import json
import os

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
TARGET = str(OUTPUT / "c/tests/subscription_fixtures.h")


def c_literals(text, per_line=100):
    pieces = []
    for index in range(0, len(text), per_line):
        chunk = text[index:index + per_line].replace('\\', '\\\\').replace('"', '\\"')
        pieces.append('    "%s"' % chunk)
    return pieces


def load(filename):
    document = json.loads(open(os.path.join(HERE, filename), 'rb').read().decode('gbk'))
    group = document[0]
    return group['colheader'], group['data']


# 1. the projection's subscription codes decide which subscription rows to keep.
proj_headers, proj_rows = load('newbond_projection.gbk')
proj_code_at = proj_headers.index('sgdm')
wanted = {row[proj_code_at].strip() for row in proj_rows}

sub_headers, sub_rows = load('subscription.gbk')
code_at = sub_headers.index('sgdm')
selected = list(sub_rows[:3])
for row in sub_rows:
    if row[code_at].strip() in wanted and row not in selected:
        selected.append(row)
print('keeping %d subscription rows: the first 3 plus the %d the projection names' % (
    len(selected), len(selected) - 3))

serialised = json.dumps([{'colheader': sub_headers, 'data': selected}], ensure_ascii=False,
                        separators=(',', ':'))
out = [
    '/* subscription_fixtures.h - a captured subscription list, GENERATED. Do not edit.',
    ' *',
    ' * Produced by c/tools/generators/make_subscription_fixtures.py from output/subscription.gbk,',
    ' * which output/dbg_jsn_probe.c wrote from the bytes a public node returned for',
    ' * bi/list/func_kkzss101_1.jsn (md5 2990f03628291e2135eb9b9c7bca1842, 319 rows).',
    ' *',
    ' * Kept: the VERBATIM colheader, the FIRST THREE ROWS (which carry the values the',
    ' * mapping assertions use) and the rows the captured new-bond projection NAMES by',
    ' * subscription code.  Without the second group a reconciliation test would match',
    ' * nothing and pass vacuously, which is the kind of test this project tries not to',
    ' * write.',
    ' *',
    ' * As UTF-8, which is what the parser sees after the GBK conversion. */',
    '#ifndef TDX_TEST_SUBSCRIPTION_FIXTURES_H',
    '#define TDX_TEST_SUBSCRIPTION_FIXTURES_H',
    '',
    '/* %d columns, %d of %d rows kept. */' % (len(sub_headers), len(selected), source_info('subscription.gbk')['source_rows']),
    'static const char subscription_list[] =',
]
out.extend(c_literals(serialised))
out.append(';')
out.append('')
out.append('#define SUBSCRIPTION_FIXTURE_ROWS %d' % len(selected))
out.append('#define SUBSCRIPTION_FIXTURE_MAPPING_ROWS 3')
out.append('#define SUBSCRIPTION_FIXTURE_COLUMNS %d' % len(sub_headers))
out.append('#define SUBSCRIPTION_FIXTURE_SOURCE_ROWS %d' % source_info('subscription.gbk')['source_rows'])

# 2. the projection whole: 2824 bytes over 13 rows.
proj_serialised = json.dumps([{'colheader': proj_headers, 'data': proj_rows}],
                             ensure_ascii=False, separators=(',', ':'))
out.append('')
out.append('/* newbond_projection: bi/list/gxjty_zq_xkzz102_1.jsn, %d columns, all %d rows,'
           % (len(proj_headers), len(proj_rows)))
out.append(' * md5 b74b52eac542578ad0ef74130f4a59c6, embedded whole because it is small. */')
out.append('static const char newbond_projection[] =')
out.extend(c_literals(proj_serialised))
out.append(';')
out.append('')
out.append('#define NEWBOND_FIXTURE_ROWS %d' % len(proj_rows))
out.append('#define NEWBOND_FIXTURE_COLUMNS %d' % len(proj_headers))
out.append('')
out.append('#endif /* TDX_TEST_SUBSCRIPTION_FIXTURES_H */')
open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
print('subscription fixture: %d columns, %d of %d rows, %d bytes of JSON' % (
    len(sub_headers), len(selected), source_info('subscription.gbk')['source_rows'], len(serialised)))
print('projection fixture:   %d columns, %d rows, %d bytes of JSON' % (
    len(proj_headers), len(proj_rows), len(proj_serialised)))
print('wrote', os.path.normpath(TARGET))
