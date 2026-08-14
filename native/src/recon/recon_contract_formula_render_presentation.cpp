#include "recon_contract_internal.hpp"
#include "recon_contract_formula_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {

bool validate_formula_colorref_contract(const std::string& contract_id,
                                        const Json& document,
                                        Json& result) {
    (void)contract_id;
        const bool identity =
            string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
            string_is(member(document, "execution_mode"), "native-cpp") &&
            string_is(member(document, "formula_source_mode"), "inline-post") &&
            string_is(member(document, "formula"), "CONTRACT_COLORREF") &&
            string_is(member(document, "market"), "sz") &&
            string_is(member(document, "code"), "000001");
        add_assertion(result, "native_formula_identity", identity,
                      "inline-post native-cpp sz000001/CONTRACT_COLORREF", identity);
        const auto* ir = member(document, "render_ir");
        const auto* primitives = ir ? member(*ir, "primitives") : nullptr;
        constexpr double expected_refs[]{255.0, 65280.0, 49344.0, 5418522.0};
        constexpr const char* expected_sources[]{
            "tcalc-named-colorref", "tcalc-named-colorref",
            "tcalc-literal-colorref", "tcalc-rgbx-rgb",
        };
        bool exact_colors = primitives && primitives->is_array() &&
                            primitives->size() == 4;
        if (exact_colors) {
            for (std::size_t index = 0; index < 4; ++index) {
                const auto* style = member(primitives->as_array()[index], "style");
                if (!style || !bool_is(member(*style, "color_ref_available"), true) ||
                    !number_is(member(*style, "color_ref"), expected_refs[index]) ||
                    !string_is(member(*style, "color_source"), expected_sources[index]) ||
                    !string_is(member(*style, "color_encoding"),
                               "Windows COLORREF: red | green<<8 | blue<<16")) {
                    exact_colors = false;
                    break;
                }
            }
        }
        add_assertion(result, "tcalc_colorref_styles", exact_colors,
                      "RED=255,GREEN=65280,COLOR00C0C0=49344,RGBX1AAE52=5418522",
                      exact_colors);
    return true;
}

bool validate_formula_render_order_contract(const std::string& contract_id,
                                            const Json& document,
                                            Json& result) {
    (void)contract_id;
        const bool identity =
            string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
            string_is(member(document, "execution_mode"), "native-cpp") &&
            string_is(member(document, "formula"), "CONTRACT_MIXED_ORDER") &&
            string_is(member(document, "market"), "sz") &&
            string_is(member(document, "code"), "000001");
        add_assertion(result, "native_formula_identity", identity,
                      "native-cpp sz000001/CONTRACT_MIXED_ORDER", identity);
        const auto* ir = member(document, "render_ir");
        const auto* primitives = ir ? member(*ir, "primitives") : nullptr;
        const bool ir_order = ir &&
            string_is(member(*ir, "schema"), "tdx-formula-render-ir-v1") &&
            string_is(member(*ir, "primitive_order"), "source-statement-order") &&
            bool_is(member(*ir, "primitive_order_contiguous"), true) &&
            number_is(member(*ir, "source_statement_count"), 4.0) &&
            primitives && primitives->is_array() && primitives->size() == 3;
        add_assertion(result, "render_ir_source_order", ir_order,
                      "4 source statements and 3 ordered primitives", ir_order);
        bool primitive_order = ir_order;
        if (primitive_order) {
            const auto& values = primitives->as_array();
            const std::array<std::string_view, 3> functions{
                "STICKLINE", "SERIES", "DRAWICON"};
            for (std::size_t index = 0; index < values.size(); ++index) {
                primitive_order = primitive_order &&
                    number_is(member(values[index], "statement_index"),
                              static_cast<double>(index + 1)) &&
                    number_is(member(values[index], "render_order"),
                              static_cast<double>(index)) &&
                    string_is(member(values[index], "render_order_semantics"),
                              "source-statement-order") &&
                    string_is(member(values[index], "function"), functions[index]);
            }
        }
        add_assertion(result, "primitive_order", primitive_order,
                      "STICKLINE@1/0, SERIES@2/1, DRAWICON@3/2",
                      primitive_order);
    return true;
}

}  // namespace tdx::recon_contract_detail
