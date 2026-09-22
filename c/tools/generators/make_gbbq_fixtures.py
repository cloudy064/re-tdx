"""Generate c/tests/gbbq_fixtures.h from the real encrypted gbbq file.

The fixture is not synthetic data: it is five real 29-byte encrypted records
sliced out of C:\\new_tdx\\T0002\\hq_cache\\gbbq, with a synthetic 4-byte header
that declares five.  That way the test exercises the actual cipher on actual
ciphertext, and the expected values it asserts are the ones the network 0x000F
command independently produced for the same events.

The record indices come from output/gbbq_probe_evidence.txt, which the probe prints
by decrypting the file and listing this security's records.

    python c/tools/generators/make_gbbq_fixtures.py
"""
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE_BIN = str(INPUT / 'gbbq_selected.bin')
SOURCE_TXT = os.path.join(HERE, 'gbbq_probe_evidence.txt')
TARGET = str(OUTPUT / "c/tests/gbbq_fixtures.h")
RECORD = 29
HEADER = 4

# The events worth pinning, as (file index, date, category, why).
CHOSEN = [
    (0, 19900301, 1, "a rights issue before the listing date"),
    (1, 19910403, 5, "the listing-date share capital"),
    (2, 19910502, 1, "10 for 3 dividend plus 10 for 4 bonus"),
    (3, 19910502, 2, "its listed shares, which chain from record 1"),
    (71, 20240614, 1, "the 10-for-7.19 dividend the C++ reference recorded"),
]


def main():
    text = open(SOURCE_TXT, encoding='utf-8').read()
    # Confirm the probe saw exactly these events, so the slice is not taken blind.
    for index, date, category, why in CHOSEN:
        pattern = r'^GBBQ_INDEX %d %d %d$' % (index, date, category)
        if not re.search(pattern, text, re.M):
            raise SystemExit('the probe did not report index %d as %d/%d (%s)'
                             % (index, date, category, why))

    data = open(SOURCE_BIN, 'rb').read()
    total = int.from_bytes(data[:4], 'little')
    if len(data) != HEADER + total * RECORD:
        raise SystemExit('the gbbq file is %d bytes for %d records' % (len(data), total))

    payload = bytearray()
    payload += len(CHOSEN).to_bytes(4, 'little')
    for saved_index, (index, date, category, why) in enumerate(CHOSEN):
        start = HEADER + saved_index * RECORD
        payload += data[start:start + RECORD]

    out = [
        '/* gbbq_fixtures.h - real encrypted GBBQ records, GENERATED. Do not hand edit.',
        ' *',
        ' * Produced by c/tools/generators/make_gbbq_fixtures.py, which slices five 29-byte records',
        ' * out of the live C:\\new_tdx\\T0002\\hq_cache\\gbbq file and writes a synthetic',
        ' * 4-byte header declaring them.  The ciphertext is real, so the test drives the',
        ' * actual cipher; the header is synthetic because the real one declares 193,370',
        ' * records and a test should not need the whole file to prove five.',
        ' *',
        ' * The five are chosen because the network 0x000F command independently returned',
        ' * the same values for the same events:',
    ]
    for index, date, category, why in CHOSEN:
        out.append(' *   file %-5d %d  category %-2d - %s' % (index, date, category, why))
    out.append(' */')
    out.append('#ifndef TDX_TEST_GBBQ_FIXTURES_H')
    out.append('#define TDX_TEST_GBBQ_FIXTURES_H')
    out.append('')
    out.append('#include <stdint.h>')
    out.append('')
    out.append('/* %d bytes: a 4-byte header + %d records of %d. */' % (
        len(payload), len(CHOSEN), RECORD))
    out.append('static const uint8_t gbbq_file[] = {')
    tokens = ['0x%02x' % byte for byte in payload]
    for index in range(0, len(tokens), 14):
        out.append('    %s,' % ', '.join(tokens[index:index + 14]))
    out.append('};')
    out.append('')
    out.append('#define GBBQ_FIXTURE_COUNT %d' % len(CHOSEN))
    out.append('')
    out.append('#endif /* TDX_TEST_GBBQ_FIXTURES_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('gbbq fixture: %d bytes, %d real encrypted records' % (len(payload), len(CHOSEN)))
    print('wrote', os.path.normpath(TARGET))


if __name__ == '__main__':
    main()
