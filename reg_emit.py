"""Emit registry_*.cpp registration units + registry_internal.hpp, moving entry text verbatim."""
import io
import json
import os
import re

data = json.load(io.open('reg_parse.json', encoding='utf-8'))
headers = json.load(io.open('reg_headers.json', encoding='utf-8'))
plan = json.load(io.open('reg_plan.json', encoding='utf-8'))
entries = data['entries']
lines = io.open('native/src/registry.cpp', encoding='utf-8').read().split('\n')

TITLE = {
    'cloud_gateway': 'Cloud gateways, block export/query and the environment doctor.',
    'formula': 'TCalc formula extraction, calculation, strategy and pool commands.',
    'static_resource': 'Local image data, HYZT and the JSN static-resource family.',
    'market_reference': 'Market depth, security directories and thematic catalogs.',
    'market_corporate': 'Corporate calendars, disclosures, financials and performance.',
    'market_quote': 'Level2 sessions, K-line, instruments, options and expansion quotes.',
    'market_disclosure': 'Issuance, forecasts, institutional research and industry profiles.',
    'market_anomaly': 'LHB, auctions, abnormal moves and risk/volatility gaps.',
    'market_session': 'Consensus, capital, limit sessions, supervision and connect flows.',
    'market_valuation': 'Technical signals, factors, ownership actions and valuation models.',
    'system': 'Minute K-line, install/session recon and the local HTTP server.',
}


def entry_text(e):
    """Original source lines for one entry, trailing comma stripped."""
    text = '\n'.join(lines[e['start']:e['end'] + 1])
    return re.sub(r',\s*$', '', text)


os.makedirs('native/src', exist_ok=True)
generated = []

for group in plan:
    unit, start, end = group['unit'], group['start'], group['end']
    members = entries[start:end + 1]
    incs = sorted({headers[e['handler']] for e in members})
    body = ',\n'.join(entry_text(e) for e in members)

    out = ['#include "tdx/registry_internal.hpp"', '']
    out += [f'#include "{inc}"' for inc in incs]
    out += ['', 'namespace tdx::registry_detail {', '']
    out += [f'// {TITLE[unit]}']
    out += [f'void append_{unit}_commands(std::vector<CommandSpec>& commands) {{']
    out += ['    commands.insert(commands.end(), {']
    out += [body]
    out += ['    });', '}', '', '}  // namespace tdx::registry_detail', '']

    path = f'native/src/registry_{unit}.cpp'
    io.open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    generated.append((path, unit, len(members), len(incs), len(out)))

# ---- internal header ----
hdr = [
    '#pragma once',
    '',
    '#include "tdx/registry.hpp"',
    '',
    '#include <vector>',
    '',
    '// Each appender contributes one contiguous slice of the command table. The',
    '// order of the calls in command_registry() is the public order of the table:',
    '// `tdx-tool --help`, /api/v1/features and the OpenAPI path seeding all iterate',
    '// the vector as-is, so appenders must never be reordered or sorted.',
    'namespace tdx::registry_detail {',
    '',
]
for group in plan:
    hdr.append(f'void append_{group["unit"]}_commands(std::vector<CommandSpec>& commands);')
hdr += ['', '}  // namespace tdx::registry_detail', '']
io.open('native/include/tdx/registry_internal.hpp', 'w', encoding='utf-8',
        newline='\n').write('\n'.join(hdr))

# ---- new registry.cpp ----
total = len(entries)
reg = [
    '#include "tdx/registry.hpp"',
    '',
    '#include "tdx/registry_internal.hpp"',
    '',
    '#include <algorithm>',
    '',
    'namespace tdx {',
    '',
    '// The table is assembled from per-domain appenders (registry_*.cpp) instead of',
    '// one 450-line literal, so a domain header change only recompiles its own unit.',
    '// Call order defines the public order of `--help`, /api/v1/features and the',
    '// OpenAPI path list.',
    'const std::vector<CommandSpec>& command_registry() {',
    '    static const std::vector<CommandSpec> commands = [] {',
    '        std::vector<CommandSpec> table;',
    f'        table.reserve({total});',
]
for group in plan:
    reg.append(f'        registry_detail::append_{group["unit"]}_commands(table);')
reg += [
    '        return table;',
    '    }();',
    '    return commands;',
    '}',
    '',
]
reg += lines[data['find_start']:]
io.open('native/src/registry.cpp', 'w', encoding='utf-8', newline='\n').write('\n'.join(reg))

print(f'{"unit":22s} {"entries":>7s} {"hdrs":>5s} {"lines":>6s}')
for path, unit, n, h, ln in generated:
    print(f'{unit:22s} {n:7d} {h:5d} {ln:6d}')
print()
print('registry.cpp lines      :', len(reg))
print('registry_internal lines :', len(hdr))
print('units emitted           :', len(generated))
