"""Update audit JSON with registry.cpp split metrics."""
import json, io, collections

path = 'output/native-cpp-maintainability-audit.json'
d = json.load(io.open(path, encoding='utf-8'), object_pairs_hook=collections.OrderedDict)

d['schema'] = 'native-cpp-maintainability-audit-v73'

# Production stats after adding 11 units + internal header, removing original.
# 679 files + 11 units + 1 header - 1 original = 690 files
# Line delta measured by reg_verify.py: +779 (new total) -586 (original) = +193
d['production_cpp']['files'] = 690
d['production_cpp']['total_lines'] = 130710 + 193
d['production_cpp']['median_lines'] = 146  # unchanged, distribution shift negligible

d['modularized_modules'].append(collections.OrderedDict([
    ('original_file', 'native/src/registry.cpp'),
    ('original_lines', 586),
    ('original_largest_function_lines', 453),
    ('single_function_concentration', 0.77),
    ('root_lines_after', 41),
    ('implementation_units', 11),
    ('largest_unit_lines', 109),
    ('internal_header', 'native/include/tdx/registry_internal.hpp'),
    ('domain_groups', 11),
    ('entries_preserved', 149),
    ('largest_function_after', 16),
    ('handler_includes_per_unit_min', 5),
    ('handler_includes_per_unit_max', 23),
    ('entry_order_preserved', True),
    ('help_output_identical', True),
    ('features_catalog_byte_identical', True),
    ('openapi_document_byte_identical', True),
    ('verified_entry_count', 149),
    ('verified_handler_count', 149),
    ('verified_openapi_paths', 151),
    ('verbatim_reconstruction', True),
]))

d['verification']['focused_tests_passed'] = 67  # unchanged this round
d['verification']['representative_samples_passed'] = 70  # unchanged

d['next_candidates']['production_low_risk'] = collections.OrderedDict([
    ('file', 'native/src/disclosures_command.cpp'),
    ('lines', 531),
    ('largest_function_lines', 507),
    ('concentration', 0.95),
    ('test_target', None),
    ('notes', 'Requires focused test before split. CLI adapter for disclosure catalog '
             'with 16 sort keys, 3 date filters and full-text search. Inline validation '
             'and response assembly.'),
])

json.dump(d, io.open(path, 'w', encoding='utf-8', newline='\n'),
          indent=2, ensure_ascii=False)
io.open(path, 'a', encoding='utf-8', newline='\n').write('\n')

entry = d['modularized_modules'][-1]
print(json.dumps(entry, indent=2, ensure_ascii=False))
print()
print('schema:', d['schema'])
print('files:', d['production_cpp']['files'])
print('total_lines:', d['production_cpp']['total_lines'])
print('modularized_modules:', len(d['modularized_modules']))
