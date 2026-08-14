"""Map each command handler to the tdx/*.hpp that declares it."""
import glob
import io
import json
import os
import re

data = json.load(io.open('reg_parse.json', encoding='utf-8'))
entries = data['entries']
handlers = sorted({e['handler'] for e in entries})

decl = {}
for path in glob.glob('native/include/tdx/*.hpp'):
    text = io.open(path, encoding='utf-8', errors='replace').read()
    rel = 'tdx/' + os.path.basename(path)
    for h in re.findall(r'\bint\s+(command_[A-Za-z0-9_]+)\s*\(', text):
        decl.setdefault(h, []).append(rel)

unresolved = [h for h in handlers if h not in decl]
multi = {h: v for h, v in decl.items() if h in handlers and len(v) > 1}

print('handlers          :', len(handlers))
print('unresolved        :', unresolved)
print('multi-declared    :', multi)

# Current includes in registry.cpp, in order
src = io.open('native/src/registry.cpp', encoding='utf-8').read().split('\n')
cur = [re.match(r'#include "(tdx/[^"]+)"', l).group(1)
       for l in src[:data['ns_line']] if re.match(r'#include "tdx/', l)]
needed = {decl[h][0] for h in handlers if h in decl}
print('includes present  :', len(cur))
print('includes needed   :', len(needed))
print('unused includes   :', sorted(set(cur) - needed - {'tdx/registry.hpp'}))

json.dump({h: decl[h][0] for h in handlers if h in decl},
          io.open('reg_headers.json', 'w', encoding='utf-8'), indent=1)
