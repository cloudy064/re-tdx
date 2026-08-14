import io, json
for tag in ('reg_base', 'reg_new'):
    d = json.load(io.open(tag + '/features.json', encoding='utf-8'))
    o = json.load(io.open(tag + '/openapi.json', encoding='utf-8'))
    names = [f['command'] for f in d['features']]
    paths = len(o['paths'])
    print(f'{tag}: schema={d["schema"]} count={d["count"]} features={len(d["features"])} '
          f'openapi_paths={paths}')
    print(f'  first={names[0]!r} last={names[-1]!r} unique={len(set(names))}')
