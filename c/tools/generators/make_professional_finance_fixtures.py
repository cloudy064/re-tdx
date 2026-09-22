"""Generate c/tests/professional_finance_fixtures.h.

The real package is 5.7 MB compressed and 13 MB uncompressed, so it cannot be embedded.
What the test needs instead is a ZIP built by SOMETHING ELSE: Python's zipfile writes the
archives here and the C reader is checked against them, which is a genuine cross-check
rather than one implementation agreeing with itself.

Four archives, chosen for the branches that matter:

  stored    method 0, so the copy path is exercised
  deflate   method 8, so the inflate path is exercised
  bad_crc   the deflate archive with its CRC field corrupted, so the CRC check is shown
            to actually reject rather than merely exist
  no_eocd   a valid archive with its end record removed, so the reader refuses it

The finance member inside each is a two-record table built here, with values chosen so a
wrong field offset shows up as a wrong number rather than as a plausible one.

    python c/tools/generators/make_professional_finance_fixtures.py
"""
import io
import os
import struct
import zlib
import zipfile

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
ROOT = str(OUTPUT)
TARGET = str(OUTPUT / "c/tests/professional_finance_fixtures.h")
MEMBER_NAME = 'gpcw20260630.dat'

FIELD_COUNT = 200
DATA_SIZE = FIELD_COUNT * 4
RECORDS = [('000001', [float(index) for index in range(FIELD_COUNT)]),
           ('600519', [float(index) * 2.0 for index in range(FIELD_COUNT)])]


def member_bytes():
    """The finance table: 20-byte header, 11-byte index entries, then the float blocks."""
    index_size = 11
    # H I H H H I I = 20 bytes: the reference reads through the data size at 12 and
    # treats the header as 20, so the last four bytes are unread padding here too.
    header = struct.pack('<HIHHHII', 1, 20260630, len(RECORDS), 0, index_size, DATA_SIZE,
                         0)
    assert len(header) == 20, len(header)
    index_offset = 20
    data_start = index_offset + len(RECORDS) * index_size
    index = b''
    data = b''
    for position, (code, values) in enumerate(RECORDS):
        offset = data_start + position * DATA_SIZE
        index += code.encode('ascii') + b'\x00' + struct.pack('<I', offset)
        data += struct.pack('<%df' % FIELD_COUNT, *values)
    return header + index + data, data_start


def build(method):
    payload, data_start = member_bytes()
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', compression=method) as archive:
        info = zipfile.ZipInfo(MEMBER_NAME, date_time=(2000, 1, 1, 0, 0, 0))
        info.compress_type = method
        info.create_system = 0
        archive.writestr(info, payload)
    return buffer.getvalue(), payload, data_start


def c_literals(data, per_line=16):
    lines = []
    for index in range(0, len(data), per_line):
        chunk = data[index:index + per_line]
        lines.append('    ' + ', '.join('0x%02x' % byte for byte in chunk) + ',')
    return lines


stored, payload, data_start = build(zipfile.ZIP_STORED)
deflated, _, _ = build(zipfile.ZIP_DEFLATED)

# The deflate archive with its stored CRC broken: the central directory's CRC field is
# the first of the two, and both are checked, so either would do.
bad_crc = bytearray(deflated)
position = bad_crc.find(b'PK\x01\x02')
assert position >= 0
bad_crc[position + 16] ^= 0xff

# A valid archive with its end record removed.
no_eocd = deflated[:-22]

out = [
    '/* professional_finance_fixtures.h - ZIP archives built by Python, GENERATED.',
    ' * Do not hand edit.',
    ' *',
    ' * Produced by c/tools/generators/make_professional_finance_fixtures.py.  The archives are written',
    ' * by Python\'s zipfile and read by the C code, so the two implementations are checked',
    ' * against each other rather than one against itself.',
    ' *',
    ' * Each holds one member, %s, holding a two-record finance table: header, index and' % MEMBER_NAME,
    ' * %d floats per record.  The values are index and index times two, so a wrong field' % FIELD_COUNT,
    ' * offset shows up as a wrong number rather than as a plausible one.',
    ' *',
    ' * The real package is 5.7 MB compressed and 13 MB uncompressed and cannot be embedded;',
    ' * its measured facts are in output/professional_finance_evidence.txt. */',
    '#ifndef TDX_TEST_PROFESSIONAL_FINANCE_FIXTURES_H',
    '#define TDX_TEST_PROFESSIONAL_FINANCE_FIXTURES_H',
    '',
    '#define PROFINANCE_MEMBER_NAME "%s"' % MEMBER_NAME,
    '#define PROFINANCE_FIXTURE_RECORDS %d' % len(RECORDS),
    '#define PROFINANCE_FIXTURE_FIELDS %d' % FIELD_COUNT,
    '#define PROFINANCE_FIXTURE_DATA_SIZE %d' % DATA_SIZE,
    '#define PROFINANCE_FIXTURE_DATA_START %d' % data_start,
    '',
]
for name, data in [('stored', stored), ('deflate', deflated), ('bad_crc', bytes(bad_crc)),
                   ('no_eocd', no_eocd)]:
    out.append('/* %s archive, %d bytes. */' % (name, len(data)))
    out.append('static const unsigned char profinance_%s_zip[] = {' % name)
    out.extend(c_literals(data))
    out.append('};')
    out.append('#define PROFINANCE_%s_BYTES %d' % (name.upper(), len(data)))
    out.append('')
out.append('#endif /* TDX_TEST_PROFESSIONAL_FINANCE_FIXTURES_H */')
open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')

print('stored %d bytes, deflate %d bytes, bad_crc %d, no_eocd %d' % (
    len(stored), len(deflated), len(bad_crc), len(no_eocd)))
print('member %d bytes, data starts at %d, records %s' % (len(payload), data_start,
                                                          [code for code, _ in RECORDS]))
print('member crc %08x' % (zlib.crc32(payload) & 0xffffffff))
print('wrote', os.path.normpath(TARGET))
