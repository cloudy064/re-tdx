"""Generate c/tests/jsn_fixtures.h from the captured JSN resource.

The whole raw GBK payload becomes the fixture, not a slice: it is only a few
kilobytes, and keeping it whole means the test drives the GBK conversion, the JSON
parse and the table layer on exactly the bytes the node sent.

    python c/tools/generators/make_jsn_fixtures.py
"""
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE = os.path.join(HERE, 'jsn_probe_evidence.txt')
TARGET = str(OUTPUT / "c/tests/jsn_fixtures.h")


def main():
    lines = open(SOURCE, encoding='utf-8').read().splitlines()
    declared = None
    tokens = []
    for index, line in enumerate(lines):
        match = re.match(r'\s*C fixture \((\d+) bytes, raw GBK as sent\):', line)
        if not match:
            continue
        declared = int(match.group(1))
        cursor = index + 1
        while cursor < len(lines) and not lines[cursor].strip():
            cursor += 1
        while cursor < len(lines) and re.match(r'^(\s*0x[0-9a-f]{2},\s*)+$', lines[cursor]):
            tokens.extend(re.findall(r'0x([0-9a-f]{2}),', lines[cursor]))
            cursor += 1
        break
    if declared is None:
        raise SystemExit('no captured fixture in %s' % SOURCE)
    if len(tokens) != declared:
        raise SystemExit('%d tokens for a declared %d bytes' % (len(tokens), declared))
    md5 = None
    group = None
    rows = None
    columns = None
    utf8 = None
    for line in lines:
        match = re.search(r'bytes \d+, declared \d+, md5 ([0-9a-f]{32})', line)
        if match:
            md5 = match.group(1)
        match = re.search(r'^groups (\d+), rows (\d+)$', line.strip())
        if match:
            group = int(match.group(1))
            rows = int(match.group(2))
        match = re.search(r'group 0: (\d+) rows, (\d+) columns', line)
        if match:
            columns = int(match.group(2))
        match = re.search(r'^utf8 (\d+) bytes$', line.strip())
        if match:
            utf8 = int(match.group(1))
    if md5 is None or rows is None or columns is None or utf8 is None:
        raise SystemExit('the transcript does not report the shape of the resource')

    out = [
        '/* jsn_fixtures.h - a captured JSN resource, GENERATED. Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_jsn_fixtures.py from output/jsn_probe_evidence.txt,',
        ' * which output/dbg_jsn_probe.c wrote from the bytes a public node returned for',
        ' * bi/list/zq_tx201.jsn (the discount-bond list).  The payload is GBK and is kept',
        ' * exactly as sent, md5 %s,' % md5,
        ' * so the test drives the whole chain - transfer digest, GBK conversion, JSON,',
        ' * table - on real bytes.  Regenerate the pair together. */',
        '#ifndef TDX_TEST_JSN_FIXTURES_H',
        '#define TDX_TEST_JSN_FIXTURES_H',
        '',
        '#include <stdint.h>',
        '',
        '/* %d bytes of GBK, %d bytes once converted to UTF-8. */' % (declared, utf8),
        'static const uint8_t jsn_gbk[] = {',
    ]
    for index in range(0, len(tokens), 14):
        out.append('    %s,' % ', '.join('0x' + t for t in tokens[index:index + 14]))
    out.append('};')
    out.append('')
    out.append('#define JSN_FIXTURE_MD5 "%s"' % md5)
    out.append('#define JSN_FIXTURE_UTF8_BYTES %d' % utf8)
    out.append('#define JSN_FIXTURE_GROUPS %d' % group)
    out.append('#define JSN_FIXTURE_ROWS %d' % rows)
    out.append('#define JSN_FIXTURE_COLUMNS %d' % columns)
    out.append('')
    out.append('#endif /* TDX_TEST_JSN_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('jsn fixture: %d GBK bytes -> %d UTF-8, %d group(s), %d rows, %d columns' % (
        declared, utf8, group, rows, columns))
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
