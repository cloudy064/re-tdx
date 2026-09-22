"""Generate c/tests/pending_fixtures.h from the captured pending resources.

The primary list is reduced to its first rows - the row MAPPING is what a fixture of
that size can verify.  The set reconciliation is pure logic over row arrays, so the
test builds its arrays directly and covers the cases a real capture cannot be relied
on to contain: duplicates, empty identities and an exactly equal set.

    python c/tools/generators/make_pending_fixtures.py
"""
import json
import os

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
TARGET = str(OUTPUT / "c/tests/pending_fixtures.h")
KEEP_ROWS = 3


def c_literals(text, per_line=100):
    pieces = []
    for index in range(0, len(text), per_line):
        chunk = text[index:index + per_line]
        chunk = chunk.replace('\\', '\\\\').replace('"', '\\"')
        pieces.append('    "%s"' % chunk)
    return pieces


def main():
    raw = open(os.path.join(HERE, 'pending_primary.gbk'), 'rb').read()
    document = json.loads(raw.decode('gbk'))
    group = document[0]
    headers = group['colheader']
    data = group['data']
    if source_info('pending_primary.gbk')['source_rows'] < KEEP_ROWS:
        raise SystemExit('the capture holds only %d rows' % source_info('pending_primary.gbk')['source_rows'])
    reduced = [{'colheader': headers, 'data': data[:KEEP_ROWS]}]
    serialised = json.dumps(reduced, ensure_ascii=False, separators=(',', ':'))

    out = [
        '/* pending_fixtures.h - a captured pending convertible-bond list, GENERATED.',
        ' * Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_pending_fixtures.py from output/pending_primary.gbk,',
        ' * which output/dbg_jsn_probe.c wrote from the bytes a public node returned for',
        ' * bi/list/dfkzz201_1.jsn (md5 c2df9c63712e1e6331d8f3b0ed647440, 153 rows).',
        ' *',
        ' * The capture is reduced to its VERBATIM colheader plus the FIRST %d ROWS: a' % KEEP_ROWS,
        ' * fixture of that size can verify the row mapping, and the set reconciliation',
        ' * is tested against arrays built in the test rather than against a capture,',
        ' * because the interesting cases there are duplicates, empty identities and an',
        ' * exactly equal set, which a live document does not promise to contain.',
        ' *',
        ' * As UTF-8, which is what the parser sees after the GBK conversion. */',
        '#ifndef TDX_TEST_PENDING_FIXTURES_H',
        '#define TDX_TEST_PENDING_FIXTURES_H',
        '',
        '/* %d columns, %d of %d rows kept. */' % (len(headers), KEEP_ROWS, source_info('pending_primary.gbk')['source_rows']),
        'static const char pending_primary[] =',
    ]
    out.extend(c_literals(serialised))
    out.append(';')
    out.append('')
    out.append('#define PENDING_FIXTURE_ROWS %d' % KEEP_ROWS)
    out.append('#define PENDING_FIXTURE_COLUMNS %d' % len(headers))
    out.append('#define PENDING_FIXTURE_SOURCE_ROWS %d' % source_info('pending_primary.gbk')['source_rows'])
    out.append('')
    out.append('#endif /* TDX_TEST_PENDING_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('pending fixture: %d columns, %d of %d rows, %d bytes of JSON' % (
        len(headers), KEEP_ROWS, source_info('pending_primary.gbk')['source_rows'], len(serialised)))
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
