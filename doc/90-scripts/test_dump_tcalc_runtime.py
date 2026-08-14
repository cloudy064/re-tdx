from __future__ import annotations

import unittest

import dump_tcalc_runtime as runtime


class RuntimeNormalizationTests(unittest.TestCase):
    def test_normalizes_gb18030_metadata_and_category(self) -> None:
        raw = {
            "counts": {"0": 1},
            "categories": [
                {
                    "kind": 0,
                    "id": 1,
                    "parent": 0,
                    "name_hex": "超买超卖型".encode("gb18030").hex(),
                    "flags": 1,
                }
            ],
            "formulas": [
                {
                    "kind": 0,
                    "index": 0,
                    "code_hex": b"KDJ".hex(),
                    "name_hex": "随机指标".encode("gb18030").hex(),
                    "category_id": 1,
                    "display_flags": 2,
                    "attribute_flags": 3,
                }
            ],
        }

        categories, formulas = runtime.normalize_dump(raw)

        self.assertEqual(categories[0][0]["name"], "超买超卖型")
        self.assertEqual(formulas[0].code, "KDJ")
        self.assertEqual(formulas[0].name, "随机指标")
        self.assertEqual(formulas[0].category_name, "超买超卖型")
        self.assertEqual(formulas[0].source, "system")
        self.assertTrue(formulas[0].is_custom)


if __name__ == "__main__":
    unittest.main()
