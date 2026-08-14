"""Parse native/src/registry.cpp into exact source spans for each CommandSpec entry."""
import io
import json
import re

SRC = 'native/src/registry.cpp'
lines = io.open(SRC, encoding='utf-8').read().split('\n')

# Region 1: includes (before "namespace tdx {")
ns_line = next(i for i, l in enumerate(lines) if l.startswith('namespace tdx {'))
fn_start = next(i for i, l in enumerate(lines)
                if l.startswith('const std::vector<CommandSpec>& command_registry()'))
init_start = next(i for i, l in enumerate(lines)
                  if 'static const std::vector<CommandSpec> commands{' in l)
init_end = next(i for i in range(init_start, len(lines)) if lines[i].strip() == '};')
find_start = next(i for i, l in enumerate(lines)
                  if l.startswith('const CommandSpec* find_command'))

print('namespace at   :', ns_line + 1)
print('fn_start at    :', fn_start + 1)
print('init_start at  :', init_start + 1)
print('init_end at    :', init_end + 1)
print('find_start at  :', find_start + 1)

# Entries: each starts with a line matching {"name", and ends when brace depth returns to 0
entries = []
i = init_start + 1
while i < init_end:
    if re.match(r'\s*\{"', lines[i]):
        start = i
        depth = 0
        j = i
        while j < init_end:
            depth += lines[j].count('{') - lines[j].count('}')
            if depth <= 0:
                break
            j += 1
        text = '\n'.join(lines[start:j + 1])
        name = re.match(r'\s*\{"([^"]+)"', lines[start]).group(1)
        # handler is the last identifier before the closing brace
        handler_m = re.search(r'(command_[A-Za-z0-9_]+)\s*\}\s*,?\s*$', text)
        handler = handler_m.group(1) if handler_m else None
        cat_m = re.match(r'\s*\{"[^"]+",\s*"[^"]*",\s*"([^"]*)"', text.replace('\n', ' '))
        entries.append({
            'index': len(entries),
            'name': name,
            'handler': handler,
            'category': cat_m.group(1) if cat_m else None,
            'start': start,
            'end': j,
            'lines': j - start + 1,
        })
        i = j + 1
    else:
        i += 1

print('entries parsed :', len(entries))
missing = [e['name'] for e in entries if not e['handler']]
print('missing handler:', missing)

json.dump({
    'ns_line': ns_line,
    'fn_start': fn_start,
    'init_start': init_start,
    'init_end': init_end,
    'find_start': find_start,
    'entries': entries,
}, io.open('reg_parse.json', 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
