"""Generate c/tests/pricing_fixtures.h from the captured pricing resource.

The capture is 1.27 MB over 312 rows, far too much to embed.  The fixture keeps the
verbatim colheader and the rows for the FIRST BOND OF EACH SHAPE the mapping has to
handle, chosen by inspecting the capture rather than by taking the first three: a
plain convertible bond, one with a single remaining coupon, and an exchangeable bond
whose quoted price is out of scale.  Selecting by property is what makes the fixture
able to show those branches.

    python c/tools/generators/make_pricing_fixtures.py
"""
import json
import os

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
TARGET = str(OUTPUT / "c/tests/pricing_fixtures.h")


def c_literals(text, per_line=100):
    pieces = []
    for index in range(0, len(text), per_line):
        chunk = text[index:index + per_line].replace('\\', '\\\\').replace('"', '\\"')
        pieces.append('    "%s"' % chunk)
    return pieces


def main():
    raw = open(os.path.join(HERE, 'pricing_primary.gbk'), 'rb').read()
    document = json.loads(raw.decode('gbk'))
    group = document[0]
    headers = group['colheader']
    index = {name: position for position, name in enumerate(headers)}

    chosen = []
    for row in group['data']:
        remaining = row[index['SYFXCS']]
        code = row[index['$ZQDM']]
        if not chosen:
            chosen.append(('the first row', row))
            continue
        if len(chosen) == 1 and remaining.strip() == '1':
            chosen.append(('one with a single remaining coupon', row))
            continue
        if len(chosen) == 2 and code.startswith('132'):
            chosen.append(('an exchangeable bond whose quoted price is out of scale', row))
            continue
    if len(chosen) < 3:
        raise SystemExit('the capture did not offer all three shapes: %d' % len(chosen))

    rows = [row for _, row in chosen]
    reduced = [{'colheader': headers, 'data': rows}]
    serialised = json.dumps(reduced, ensure_ascii=False, separators=(',', ':'))

    out = [
        '/* pricing_fixtures.h - a captured convertible-bond pricing document, GENERATED.',
        ' * Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_pricing_fixtures.py from output/pricing_primary.gbk,',
        ' * which output/dbg_jsn_probe.c wrote from the bytes a public node returned for',
        ' * bi/list/gxjty_zq_kzzsy101_1.jsn',
        ' * (md5 e3bbf32e7afb2a14e09fc84802c9d5cc, 312 rows, 43 columns).',
        ' *',
        ' * The capture is reduced to its VERBATIM colheader plus the rows for the first',
        ' * bond of each shape the mapping has to handle:',
    ]
    for label, row in chosen:
        out.append(' *   %s (%s)' % (label, row[index['$ZQDM']]))
    out.extend([
        ' *',
        ' * The QUOTE side is not fixture data: the test builds the quotes it needs, so it',
        ' * can exercise the previous-close fallback, an absent quote and an out-of-scale',
        ' * price, none of which a single capture can be relied on to contain.',
        ' *',
        ' * As UTF-8, which is what the parser sees after the GBK conversion. */',
        '#ifndef TDX_TEST_PRICING_FIXTURES_H',
        '#define TDX_TEST_PRICING_FIXTURES_H',
        '',
        '/* %d columns, %d of %d rows kept. */' % (len(headers), len(rows), source_info('pricing_primary.gbk')['source_rows']),
        'static const char pricing_document[] =',
    ])
    out.extend(c_literals(serialised))
    out.append(';')
    out.append('')
    out.append('#define PRICING_FIXTURE_ROWS %d' % len(rows))
    out.append('#define PRICING_FIXTURE_COLUMNS %d' % len(headers))
    out.append('#define PRICING_FIXTURE_SOURCE_ROWS %d' % source_info('pricing_primary.gbk')['source_rows'])
    out.append('')
    for position, (label, row) in enumerate(chosen):
        out.append('#define PRICING_FIXTURE_CODE_%d "%s" /* %s */' % (
            position, row[index['$ZQDM']], label))
    out.append('')
    out.append('#endif /* TDX_TEST_PRICING_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('pricing fixture: %d columns, %d of %d rows, %d bytes of JSON' % (
        len(headers), len(rows), source_info('pricing_primary.gbk')['source_rows'], len(serialised)))
    for label, row in chosen:
        print('  %s %s : face=%s remaining=%s as_of_coupon=%s' % (
            row[index['$ZQDM']], label, row[index['MZ']], row[index['SYFXCS']],
            row[index['SYFXRQXL']]))
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
