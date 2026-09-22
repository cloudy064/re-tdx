"""Independent field contracts for CLI renderers; reject duplicate JSON keys."""
import json
import subprocess
import sys

def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate rendered key: ' + repr(key))
        result[key] = value
    return result

def reject_constant(value):
    raise ValueError('non-JSON constant: ' + value)

raw = subprocess.check_output([sys.argv[1], '--emit-json-vectors'])
rows = [json.loads(line, object_pairs_hook=unique_pairs, parse_constant=reject_constant)
        for line in raw.decode('utf-8').splitlines()]
assert len(rows) == 9
assert rows[0]['source'] == 'C:\\quoted"dir\\rules\n.dat'
assert rows[1]['previous_close'] == 10 and rows[1]['upper'] == 11
assert rows[2]['type'] == 'jsn_row'
assert rows[2]['type_cell'] == 'B' and rows[2]['type_cell_2'] == 'C'
assert rows[2]['type_cell_3'] == 'A' and rows[2]['type_cell_4'] == 'D'
assert rows[2]['quote"\\'] == 'F' and rows[2]['type\0tail'] == 'G'
assert rows[2]['dup'] == 'H' and rows[2]['dup_cell_2'] == 'I'
assert rows[3]['columns_renamed'] == 4 and rows[3]['md5'] == 'md5"\\'
assert rows[4]['documents'] == 8 and rows[4]['projection_fields_used'] == 4
assert rows[5]['name'] == 'Bond\0name'
assert rows[5]['coupon_schedule'] == [
    {'date': 20270101, 'rate_pct': 2}, {'date': 'odd"\\date\0x', 'rate_pct': 3}]
assert rows[5]['remaining_coupon_schedule'] == [{'date': 20280101, 'rate_pct': 4}]
assert rows[5]['numbers']['face_value_yuan'] is None
assert rows[6] == ['line\n\0x', '1\0tail', 'inf', 'nan', '1e999', 12.5]
assert rows[7]['scale'] == 'scale"\\\n'
assert rows[8]['overview']['numbers']['face_value'] is None
print('independent domain renderer contracts passed')
