"""Prove the generated units reproduce the archived initializer byte-for-byte."""
import io
import json
import re

data = json.load(io.open('reg_parse.json', encoding='utf-8'))
plan = json.load(io.open('reg_plan.json', encoding='utf-8'))
entries = data['entries']
orig = io.open('native/src/registry.cpp.removed', encoding='utf-8').read().split('\n')

# ---- real line counts ----
print(f'{"file":42s} {"lines":>6s}')
total = 0


def count(path):
    global total
    n = len(io.open(path, encoding='utf-8').read().split('\n'))
    total += n
    print(f'{path.split("/")[-1]:42s} {n:6d}')
    return n


count('native/src/registry.cpp')
count('native/include/tdx/registry_internal.hpp')
unit_max = 0
for group in plan:
    unit_max = max(unit_max, count(f'native/src/registry_{group["unit"]}.cpp'))
print(f'{"TOTAL":42s} {total:6d}')
print(f'original                                   {len(orig):6d}')
print(f'largest unit                               {unit_max:6d}')
print()

# ---- verbatim reconstruction of the initializer region ----
region = '\n'.join(orig[data['init_start'] + 1:data['init_end']])


def entry_text(e):
    return re.sub(r',\s*$', '', '\n'.join(orig[e['start']:e['end'] + 1]))


order = []
for group in plan:
    order.extend(entries[group['start']:group['end'] + 1])
rebuilt = ',\n'.join(entry_text(e) for e in order) + ','

print('entries in plan order :', len(order))
print('region lines          :', len(region.split('\n')))
print('rebuilt lines         :', len(rebuilt.split('\n')))
print('VERBATIM IDENTICAL    :', region == rebuilt)
if region != rebuilt:
    a, b = region.split('\n'), rebuilt.split('\n')
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            print(f'  first diff at region line {data["init_start"] + 2 + i}')
            print(f'    orig: {a[i]!r}')
            print(f'    new : {b[i]!r}')
            break

# ---- entries actually present in the generated units ----
found = []
for group in plan:
    text = io.open(f'native/src/registry_{group["unit"]}.cpp', encoding='utf-8').read()
    found.extend(re.findall(r'\{"([^"]+)",\s*"[^"]*",\s*"[^"]*",', text))
expect = [e['name'] for e in order]
print()
print('names emitted         :', len(found))
print('NAME ORDER IDENTICAL  :', found == expect)

# ---- handler coverage ----
handlers_orig = set(re.findall(r'(command_[A-Za-z0-9_]+)\}', region))
handlers_new = set()
for group in plan:
    text = io.open(f'native/src/registry_{group["unit"]}.cpp', encoding='utf-8').read()
    handlers_new |= set(re.findall(r'(command_[A-Za-z0-9_]+)\}', text))
print('handlers orig / new   :', len(handlers_orig), '/', len(handlers_new))
print('HANDLER SET IDENTICAL :', handlers_orig == handlers_new)
