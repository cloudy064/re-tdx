"""Generate c/tests/convertible_fixtures.h from the six captured documents.

Each capture is tens to hundreds of kilobytes, far too much to embed.  The generator
keeps the rows for the FIRST THREE BONDS OF THE OVERVIEW from every document, so the
fixture contains bonds that all six documents describe and a join test can show all
six groups being filled.  The selection is by key, not by position, and no value is
invented or edited; the fixture header states the rule.

All six are reduced from one capture run, so cross-document facts are consistent.

    python c/tools/generators/make_convertible_fixtures.py
"""
import io
import json
import os

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
TARGET = str(OUTPUT / "c/tests/convertible_fixtures.h")
KEEP_BONDS = 3

DOCUMENTS = [
    ('overview', 'kzz_overview.gbk', 'bi/list/kzz_kzzsy201_1.jsn'),
    ('progress', 'kzz_progress.gbk', 'bi/list/func_kzz_tkjd201.jsn'),
    ('coupons', 'kzz_coupons.gbk', 'bi/list/func_kzz_lltk201.jsn'),
    ('sellback', 'kzz_sellback.gbk', 'bi/list/func_kzz_hstk201.jsn'),
    ('redemption', 'kzz_redemption.gbk', 'bi/list/func_kzz_shtk201.jsn'),
    ('revision', 'kzz_revision.gbk', 'bi/list/func_kzz_xztk201.jsn'),
]


def load(filename):
    document = json.loads(open(os.path.join(HERE, filename), 'rb').read().decode('gbk'))
    group = document[0]
    headers = group['colheader']
    return (headers, group['data'], headers.index('$ZQDM'), headers.index('$SC'))


def c_literals(text, per_line=100):
    pieces = []
    for index in range(0, len(text), per_line):
        chunk = text[index:index + per_line]
        chunk = chunk.replace('\\', '\\\\').replace('"', '\\"')
        pieces.append('    "%s"' % chunk)
    return pieces


def main():
    headers, data, code_index, market_index = load(DOCUMENTS[0][1])
    keys = [(row[market_index], row[code_index]) for row in data[:KEEP_BONDS]]
    print('keeping the overview first %d bonds: %s' % (KEEP_BONDS, keys))

    out = [
        '/* convertible_fixtures.h - six captured convertible-bond documents, GENERATED.',
        ' * Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_convertible_fixtures.py from the .gbk files that',
        ' * output/dbg_jsn_probe.c wrote from the bytes a public node returned.  Each',
        ' * capture is tens to hundreds of kilobytes, so each is reduced to its VERBATIM',
        ' * colheader plus the rows for the FIRST %d BONDS OF THE OVERVIEW - chosen by' % KEEP_BONDS,
        ' * key, so the same %d bonds appear in all six documents and a join test can' % KEEP_BONDS,
        ' * show every group filled.  No value is invented or edited; only rows are',
        ' * dropped.  All six come from one capture run, so cross-document assertions are',
        ' * consistent.',
        ' *',
        ' * As UTF-8, which is what the parser sees after the GBK conversion. */',
        '#ifndef TDX_TEST_CONVERTIBLE_FIXTURES_H',
        '#define TDX_TEST_CONVERTIBLE_FIXTURES_H',
        '',
    ]
    defines = []
    for name, filename, remote in DOCUMENTS:
        document_headers, document_data, code_at, market_at = load(filename)
        kept = [row for row in document_data
                if (row[market_at], row[code_at]) in keys]
        if len(kept) != KEEP_BONDS:
            raise SystemExit('%s carries %d of the %d chosen bonds' % (
                filename, len(kept), KEEP_BONDS))
        reduced = [{'colheader': document_headers, 'data': kept}]
        serialised = json.dumps(reduced, ensure_ascii=False, separators=(',', ':'))
        out.append('/* %s: %s, %d columns, %d of %d rows kept. */' % (
            name, remote, len(document_headers), len(kept), source_info(filename)['source_rows']))
        out.append('static const char convertible_%s[] =' % name)
        out.extend(c_literals(serialised))
        out.append(';')
        out.append('')
        defines.append((name, len(document_headers), source_info(filename)['source_rows'], remote))
        print('%-11s %3d columns, %d of %d rows' % (name, len(document_headers), len(kept),
                                                    source_info(filename)['source_rows']))
    out.append('#define CONVERTIBLE_FIXTURE_ROWS %d' % KEEP_BONDS)
    for name, columns, rows, remote in defines:
        out.append('#define CONVERTIBLE_%s_COLUMNS %d' % (name.upper(), columns))
        out.append('#define CONVERTIBLE_%s_SOURCE_ROWS %d' % (name.upper(), rows))
    out.append('')
    out.append('/* The three bonds the fixture carries, as "market/code". */')
    for market, code in keys:
        out.append('#define CONVERTIBLE_FIXTURE_BOND_%s_%s "%s/%s"' % (
            market, code, market, code))
    out.append('')
    out.append('#endif /* TDX_TEST_CONVERTIBLE_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('wrote', os.path.normpath(TARGET))



# Retain the two additional captured documents from the original extension.
EXTRA = [
    ('exchangeable', 'kzz_exchangeable.gbk', 'bi/list/kjhz_kjhzsy201_1.jsn'),
    ('projection', 'kzz_exchangeable_projection.gbk', 'bi/list/func_kzz103_1.jsn'),
]


def append_extra():
    text = io.open(TARGET, encoding='utf-8').read()
    anchor = '#define CONVERTIBLE_FIXTURE_ROWS'
    addition = []
    for name, filename, remote in EXTRA:
        path = os.path.join(HERE, filename)
        if not os.path.exists(path):
            raise SystemExit('%s is missing; capture it first' % path)
        raw = open(path, 'rb').read()
        document = json.loads(raw.decode('gbk'))
        group = document[0]
        serialised = json.dumps([{'colheader': group['colheader'], 'data': group['data']}],
                                ensure_ascii=False, separators=(',', ':'))
        addition.append('/* %s: %s, %d columns, all %d rows. */' % (
            name, remote, len(group['colheader']), len(group['data'])))
        addition.append('static const char convertible_%s[] =' % name)
        addition.extend(c_literals(serialised))
        addition.append(';')
        addition.append('')
        addition.append('#define CONVERTIBLE_%s_COLUMNS %d' % (name.upper(), len(group['colheader'])))
        addition.append('#define CONVERTIBLE_%s_SOURCE_ROWS %d' % (name.upper(), len(group['data'])))
        addition.append('')
        print('%-12s %d bytes, %d columns, %d rows' % (name, len(raw), len(group['colheader']),
                                                       len(group['data'])))

    if 'convertible_exchangeable' not in text:
        text = text.replace(anchor, '\n'.join(addition) + anchor, 1)
        io.open(TARGET, 'w', encoding='utf-8', newline='\n').write(text)
        print('fixture extended')
    else:
        print('already extended')

if __name__ == '__main__':
    main()
    append_extra()
