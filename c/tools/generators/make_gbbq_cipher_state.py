"""Emit the GBBQ cipher state from the C++ reference into a C header.

The state is a 4168-byte blob the reference embeds as base64.  Transcribing 5558
characters by hand would be an obvious place to introduce a silent corruption, so
the blob is extracted mechanically from native/src/corporate/corporate_gbbq.cpp
and only the decoding moves into C, where a test can check it round-trips.

    python c/tools/generators/make_gbbq_cipher_state.py
"""
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
SOURCE = str(INPUT / 'gbbq_cipher_state.txt')
TARGET = str(OUTPUT / "c/src/gbbq_cipher_state.h")
EXPECTED_BYTES = 4168


def extract_base64():
    text = open(SOURCE, encoding='utf-8').read()
    match = re.search(
        r'gbbq_cipher_state_base64\[\]\s*=\s*(.*?);', text, re.S)
    if not match:
        raise SystemExit('no cipher state in %s' % SOURCE)
    pieces = re.findall(r'"([^"]*)"', match.group(1))
    if not pieces:
        raise SystemExit('the cipher state literal has no string pieces')
    return ''.join(pieces)


def decode(blob):
    import base64
    return base64.b64decode(blob, validate=True)


def main():
    blob = extract_base64()
    raw = decode(blob)
    if len(raw) != EXPECTED_BYTES:
        raise SystemExit('decoded %d bytes, the reference expects %d' % (len(raw), EXPECTED_BYTES))

    lines = [
        '/* gbbq_cipher_state.h - the GBBQ cipher state, GENERATED. Do not hand edit.',
        ' *',
        ' * Extracted from native/src/corporate/corporate_gbbq.cpp by',
        ' * c/tools/generators/make_gbbq_cipher_state.py, which pulls the base64 literal out of the',
        ' * reference and decodes it here.  The blob is %d bytes and the decoding is' % len(raw),
        ' * checked for length, because a mistyped character in a table like this would',
        ' * corrupt every record with no other symptom.',
        ' *',
        ' * The base64 is split into array entries rather than concatenated into one',
        ' * literal: ISO C only guarantees 4095 characters, and -Wpedantic says so. */',
        '#ifndef TDX_GBBQ_CIPHER_STATE_H',
        '#define TDX_GBBQ_CIPHER_STATE_H',
        '',
        '#include <stddef.h>',
        '',
        '/* %d bytes of base64 in %d pieces. */' % (len(blob), (len(blob) + 511) // 512),
        'static const char *const tdx_gbbq_cipher_state_pieces[] = {',
    ]
    for index in range(0, len(blob), 512):
        lines.append('    "%s",' % blob[index:index + 512])
    lines.append('};')
    lines.append('')
    lines.append('#define TDX_GBBQ_CIPHER_STATE_PIECES %d' % len(
        [1 for _ in range(0, len(blob), 512)]))
    lines.append('#define TDX_GBBQ_CIPHER_STATE_SIZE %d' % len(raw))
    lines.append('#define TDX_GBBQ_CIPHER_STATE_BASE64_LENGTH %d' % len(blob))
    lines.append('')
    lines.append('#endif /* TDX_GBBQ_CIPHER_STATE_H */')
    open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    print('cipher state: %d base64 chars -> %d bytes' % (len(blob), len(raw)))
    print('wrote', os.path.normpath(TARGET))

    # A spot check so the extraction is not taken on faith: the reference indexes
    # tables at fixed offsets inside this blob.
    for offset in (0x00, 0x44, 0x448, 0x1044):
        word = int.from_bytes(raw[offset:offset + 4], 'little')
        print('  state[0x%04X] = 0x%08X' % (offset, word))


if __name__ == '__main__':
    main()
