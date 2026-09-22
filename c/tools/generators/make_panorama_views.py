"""Extract the panorama view registry from the reference and emit it as a C table.

Ten views and about a hundred field mappings.  Transcribing them by hand is exactly the work
that produced three rounds of binary-field mistakes earlier, so the mapping is read out of the
reference mechanically instead - the reference IS the authority for which column becomes which
name, and a machine cannot mistype "rzljlr20".

The extractor reports what it found so the count can be asserted in the test: if a regex
silently stopped matching, the test's expected view and field counts fail rather than the table
quietly shrinking.
"""
import io
import os
import re

from generator_support import INPUT, OUTPUT, source_info
HERE = str(INPUT)
ROOT = str(OUTPUT)
SOURCE = str(INPUT / 'panorama_registry.txt')
TARGET = str(OUTPUT / "c/src/tdx_panorama_views.c")

text = io.open(SOURCE, encoding='utf-8').read()

# The registry function's body, then each view initializer inside it.
start = text.find('const std::vector<ViewSpec>& views()')
if start < 0:
    raise SystemExit('the views() function was not found')
end = text.find('\n}\n', start)
body = text[start:end]

pattern = re.compile(
    r'\{"(?P<id>[a-z0-9-]+)",\s*"(?P<label>[^"]+)",\s*"(?P<resource>[^"]+)",\s*'
    r'"(?P<market>\$[A-Za-z0-9_]+)",\s*"(?P<code>\$[A-Za-z0-9_]+)",\s*\{(?P<fields>.*?)\}\}',
    re.S)
field_pattern = re.compile(r'\{"(?P<name>[a-z0-9_]+)",\s*"(?P<column>[^"]+)"\}')

views = []
for match in pattern.finditer(body):
    fields = field_pattern.findall(match.group('fields'))
    views.append({
        'id': match.group('id'),
        'label': match.group('label'),
        'resource': match.group('resource'),
        'market': match.group('market'),
        'code': match.group('code'),
        'fields': fields,
    })

print('extracted %d views, %d field mappings'
      % (len(views), sum(len(view['fields']) for view in views)))
for view in views:
    print('  %-20s %-24s %2d fields  %s' % (view['id'], view['resource'],
                                            len(view['fields']), view['label']))

# The labels are Chinese and the C string must hold their UTF-8 bytes; Python has them as
# characters, so each is encoded explicitly and written as escapes to keep the file ASCII.
def c_bytes(value):
    return ''.join('\\x%02x' % byte for byte in value.encode('utf-8'))


out = [
    '/* tdx_panorama_views.c - the panorama view registry. GENERATED.',
    ' *',
    ' * Do not hand edit: produced by c/tools/generators/make_panorama_views.py from the reference\'s own',
    ' * view table in native/src/market/panorama.cpp.  Ten views and %d field mappings - a' %
    sum(len(view['fields']) for view in views),
    ' * transcription job whose mistakes would look like plausible column names.',
    ' *',
    ' * The labels are UTF-8 byte escapes so this file stays ASCII. */',
    '',
    '#include "tdx_panorama.h"',
    '',
    'const tdx_panorama_view tdx_panorama_views[] = {',
]
for view in views:
    out.append('    {"%s",' % view['id'])
    out.append('     "%s",' % c_bytes(view['label']))
    out.append('     "%s", "%s", "%s",' % (view['resource'], view['market'], view['code']))
    out.append('     %d, (const tdx_panorama_field[]){' % len(view['fields']))
    for name, column in view['fields']:
        out.append('         {"%s", "%s"},' % (name, column))
    out.append('     }},')
out.append('};')
out.append('')
out.append('const size_t tdx_panorama_view_count = '
           'sizeof(tdx_panorama_views) / sizeof(tdx_panorama_views[0]);')
out.append('')
out.append('/* The counts the test asserts, so a table that quietly shrank fails loudly. */')
out.append('#define TDX_PANORAMA_VIEWS_EXPECTED %d' % len(views))
out.append('#define TDX_PANORAMA_FIELDS_EXPECTED %d'
           % sum(len(view['fields']) for view in views))

# Emitted as constants the test can include rather than as macros in a header: the test
# includes the header, so the counts go there too.
io.open(TARGET, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
print()
print('wrote %s' % os.path.normpath(TARGET))
