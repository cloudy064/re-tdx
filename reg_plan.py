"""Propose contiguous registration groups for registry.cpp and report sizes."""
import io
import json

data = json.load(io.open('reg_parse.json', encoding='utf-8'))
headers = json.load(io.open('reg_headers.json', encoding='utf-8'))
entries = data['entries']

by_name = {e['name']: e['index'] for e in entries}

# Seams are given by the FIRST command of each group; groups are contiguous.
seams = [
    ('cloud_gateway',      'cloud workflow'),
    ('formula',            'formulas extract'),
    ('static_resource',    'image-data decode'),
    ('market_reference',   'market depth'),
    ('market_corporate',   'market calendar'),
    ('market_quote',       'level2 build'),
    ('market_disclosure',  'market futures-issuance'),
    ('market_anomaly',     'market lhb'),
    ('market_session',     'market consensus'),
    ('market_valuation',   'market technical-signals'),
    ('system',             'minute download'),
]

bounds = []
for pos, (unit, first) in enumerate(seams):
    start = by_name[first]
    end = by_name[seams[pos + 1][1]] - 1 if pos + 1 < len(seams) else len(entries) - 1
    bounds.append((unit, start, end))

total_entries = 0
total_lines = 0
print(f'{"unit":22s} {"entries":>7s} {"body":>5s} {"hdrs":>5s}  {"first":24s} {"last"}')
for unit, start, end in bounds:
    group = entries[start:end + 1]
    body = sum(e['lines'] for e in group)
    hdrs = len({headers[e['handler']] for e in group})
    total_entries += len(group)
    total_lines += body
    # +includes +namespace +fn signature/closing ~ 12 lines of scaffolding
    print(f'{unit:22s} {len(group):7d} {body:5d} {hdrs:5d}  '
          f'{group[0]["name"]:24s} {group[-1]["name"]}')

print()
print('entries covered :', total_entries, 'of', len(entries))
print('body lines      :', total_lines)
assert total_entries == len(entries)

# Verify contiguity + order reconstruction
rebuilt = []
for unit, start, end in bounds:
    rebuilt.extend(e['name'] for e in entries[start:end + 1])
assert rebuilt == [e['name'] for e in entries], 'ORDER MISMATCH'
print('order reconstruction: OK (exact)')

json.dump([{'unit': u, 'start': s, 'end': e} for u, s, e in bounds],
          io.open('reg_plan.json', 'w', encoding='utf-8'), indent=1)
