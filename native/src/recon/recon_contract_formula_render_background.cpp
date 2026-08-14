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

bool validate_formula_background_contract(const std::string& contract_id,
                                          const Json& document,
                                          Json& result) {
    (void)contract_id;
        const bool identity =
            string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
            string_is(member(document, "execution_mode"), "native-cpp") &&
            string_is(member(document, "formula_source_mode"), "inline-post") &&
            string_is(member(document, "formula"), "CONTRACT_BACKGROUND") &&
            string_is(member(document, "market"), "sz") &&
            string_is(member(document, "code"), "000001");
        add_assertion(result, "native_formula_identity", identity,
                      "inline-post native-cpp sz000001/CONTRACT_BACKGROUND", identity);
        bool opaque_background_semantics = false;
        bool alpha_background_semantics = false;
        const auto* ir = member(document, "render_ir");
        const auto* primitives = ir ? member(*ir, "primitives") : nullptr;
        if (ir && string_is(member(*ir, "schema"), "tdx-formula-render-ir-v1") &&
            primitives && primitives->is_array()) {
            for (const auto& primitive : primitives->as_array()) {
                if (!string_is(member(primitive, "function"), "DRAWGBK_DIV") ||
                    !string_is(member(primitive, "kind"), "background") ||
                    !string_is(member(primitive, "background_condition_scope"),
                               "per-bar-contiguous-regions") ||
                    !number_is(member(primitive, "background_native_render_type"), 21.0) ||
                    !string_is(member(primitive, "background_native_renderer"),
                               "tdxw-sub_95A6C0-sub_95A330") ||
                    !string_is(member(primitive, "background_condition_true_rule"),
                               "abs(value-1)<0.0001") ||
                    !string_is(member(primitive, "background_region_rule"),
                               "maximal-contiguous-native-true-bars") ||
                    !number_is(member(primitive, "background_alpha_mode_min"), 10.0) ||
                    !number_is(member(primitive, "background_alpha_mode_max"), 20.0) ||
                    !string_is(member(primitive, "background_alpha_rule"),
                               "255*(mode-10)/10") ||
                    !number_is(member(primitive, "background_fill_mode_argument"), 3.0) ||
                    !number_is(member(primitive, "background_range_argument"), 4.0))
                    continue;
                const auto* events = member(primitive, "events");
                if (!events || !events->is_array() || events->size() <= 1) continue;
                for (const auto& event : events->as_array()) {
                    const bool common =
                        bool_is(member(event, "background_color1_available"), true) &&
                        bool_is(member(event, "background_color2_available"), true) &&
                        number_is(member(event, "background_color1_ref"), 197121.0) &&
                        number_is(member(event, "background_color2_ref"), 394500.0) &&
                        bool_is(member(event, "background_region_leader"), true) &&
                        member(event, "background_region_start_index") &&
                        member(event, "background_region_start_index")->is_number() &&
                        member(event, "background_region_end_index") &&
                        member(event, "background_region_end_index")->is_number() &&
                        number_is(member(event, "background_range"), 0.0) &&
                        string_is(member(event, "background_range_name"), "pane") &&
                        string_is(member(event, "background_price_aggregation"),
                                  "whole-pane") &&
                        number_is(member(event, "background_fill_alpha_denominator"),
                                  255.0);
                    if (common &&
                        number_is(member(event, "background_fill_mode"), 0.0) &&
                        string_is(member(event, "background_fill_mode_name"),
                                  "vertical-gradient") &&
                        string_is(member(event, "background_fill_compositing"),
                                  "opaque-gdi-gradient-or-solid") &&
                        number_is(member(event, "background_fill_alpha_byte"), 255.0))
                        opaque_background_semantics = true;
                    if (common &&
                        number_is(member(event, "background_fill_mode"), 17.0) &&
                        string_is(member(event, "background_fill_mode_name"),
                                  "alpha-solid") &&
                        string_is(member(event, "background_fill_compositing"),
                                  "gdiplus-argb-solid-color1") &&
                        number_is(member(event, "background_fill_alpha_byte"), 178.0))
                        alpha_background_semantics = true;
                }
            }
        }
        const bool background_semantics =
            opaque_background_semantics && alpha_background_semantics;
        add_assertion(result, "native_background_regions", background_semantics,
                      "opaque mode 0 and GDI+ mode 17 alpha=178",
                      background_semantics);
    return true;
}

}  // namespace tdx::recon_contract_detail
