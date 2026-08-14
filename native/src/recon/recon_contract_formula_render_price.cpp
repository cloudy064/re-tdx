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

bool validate_formula_price_annotations_contract(const std::string& contract_id,
                                                 const Json& document,
                                                 Json& result) {
    (void)contract_id;
        const bool identity =
            string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
            string_is(member(document, "execution_mode"), "native-cpp") &&
            string_is(member(document, "formula_source_mode"), "inline-post") &&
            string_is(member(document, "formula"), "CONTRACT_PRICE_ANNOTATIONS") &&
            string_is(member(document, "market"), "sz") &&
            string_is(member(document, "code"), "000001");
        add_assertion(result, "native_formula_identity", identity,
                      "inline-post native-cpp sz000001/CONTRACT_PRICE_ANNOTATIONS",
                      identity);

        const auto* environment = member(document, "render_environment");
        const auto* annotation = environment ? member(*environment, "annotation") : nullptr;
        const auto* font = annotation ? member(*annotation, "font") : nullptr;
        const bool exact_environment = environment && annotation && font &&
            string_is(member(*environment, "schema"),
                      "tdx-formula-render-environment-v1") &&
            number_is(member(*annotation, "native_chart_row_height_pixels"), 16.0) &&
            string_is(member(*annotation, "native_chart_row_height_source"),
                      "TdxW.exe!sub_92A830+0x1EA") &&
            number_is(member(*font, "native_textout_aux_mode"), 0.0) &&
            number_is(member(*font, "native_textout_y_adjustment_pixels"), 0.0);
        add_assertion(result, "native_annotation_environment", exact_environment,
                      "row-height=16 and TextOutA aux-mode=0", exact_environment);

        bool text_origin = false, text_above = false;
        bool number_origin = false, number_above = false;
        std::size_t filtered = 0;
        const auto* ir = member(document, "render_ir");
        const auto* primitives = ir ? member(*ir, "primitives") : nullptr;
        if (ir && string_is(member(*ir, "schema"), "tdx-formula-render-ir-v1") &&
            primitives && primitives->is_array()) {
            for (const auto& primitive : primitives->as_array()) {
                const bool price_annotation =
                    string_is(member(primitive, "function"), "DRAWTEXT") ||
                    string_is(member(primitive, "function"), "DRAWNUMBER");
                if (!price_annotation) continue;
                const bool common =
                    string_is(member(primitive, "annotation_condition_true_rule"),
                              "abs(value-1)<0.0001") &&
                    string_is(member(primitive, "annotation_missing_price_rule"),
                              "skip-event") &&
                    string_is(member(primitive, "annotation_edge_behavior"),
                              "no-clamp-no-flip") &&
                    string_is(member(primitive, "annotation_collision_behavior"),
                              "none-source-order-overpaint") &&
                    string_is(member(primitive, "annotation_textout_y_adjustment_rule"),
                              "font-aux-mode==0?0:font-aux-mode==1?-1:-3");
                const auto* events = member(primitive, "events");
                if (common && events && events->is_array() && events->size() == 0) {
                    ++filtered;
                    continue;
                }
                if (!common || !events || !events->is_array() || events->size() != 1)
                    continue;
                const auto& event = events->as_array().front();
                const auto* text = member(event, "annotation_text");
                if (!text || !text->is_string()) continue;
                if (string_is(member(primitive, "function"), "DRAWTEXT")) {
                    const auto* lines = member(event, "annotation_lines");
                    const bool text_common =
                        number_is(member(primitive, "annotation_max_lines"), 10.0) &&
                        string_is(member(primitive, "annotation_unframed_horizontal_rule"),
                                  "bar-x") &&
                        string_is(member(primitive, "annotation_unframed_base_y_rule"),
                                  "price-y-minus-8") &&
                        string_is(member(primitive, "annotation_unframed_line_step_rule"),
                                  "measured-text-height;empty-line-measured-A-height") &&
                        number_is(member(event, "annotation_x_offset_pixels"), 0.0) &&
                        number_is(member(event, "annotation_y_offset_pixels"), -8.0);
                    if (text_common && text->as_string() == "A&&B" && lines &&
                        lines->is_array() && lines->size() == 3 &&
                        lines->as_array()[1].is_string() &&
                        lines->as_array()[1].as_string().empty() &&
                        number_is(member(event, "annotation_line_count"), 3.0) &&
                        bool_is(member(event, "annotation_drawabove"), false) &&
                        string_is(member(event, "annotation_vertical_align"),
                                  "price-origin"))
                        text_origin = true;
                    if (text_common && text->as_string() == "UP&NEXT" &&
                        number_is(member(event, "annotation_line_count"), 2.0) &&
                        bool_is(member(event, "annotation_drawabove"), true) &&
                        string_is(member(event, "annotation_vertical_align"),
                                  "above-price") &&
                        string_is(member(primitive, "annotation_unframed_drawabove_rule"),
                                  "price-y-line-count*(native-chart-row-height-2)-8"))
                        text_above = true;
                } else {
                    const bool number_common =
                        string_is(member(primitive, "annotation_unframed_horizontal_rule"),
                                  "bar-x-minus-3") &&
                        string_is(member(primitive, "annotation_unframed_base_y_rule"),
                                  "price-y") &&
                        string_is(member(primitive, "annotation_number_format_rule"),
                                  "sub_591950-integer-or-2/3-decimals") &&
                        number_is(member(primitive, "annotation_number_chart_precision"), 2.0) &&
                        (string_is(member(primitive,
                                          "annotation_number_chart_precision_source"),
                                   "native-constructor-default") ||
                         string_is(member(primitive,
                                          "annotation_number_chart_precision_source"),
                                   "kline.price_precision")) &&
                        number_is(member(event, "annotation_x_offset_pixels"), -3.0) &&
                        number_is(member(event, "annotation_y_offset_pixels"), 0.0);
                    if (number_common && text->as_string() == "12.340" &&
                        bool_is(member(event, "annotation_drawabove"), false) &&
                        string_is(member(event, "annotation_vertical_align"),
                                  "price-origin"))
                        number_origin = true;
                    if (number_common && text->as_string() == "56.780" &&
                        bool_is(member(event, "annotation_drawabove"), true) &&
                        string_is(member(event, "annotation_vertical_align"),
                                  "above-price") &&
                        string_is(member(primitive, "annotation_unframed_drawabove_rule"),
                                  "price-y-(native-chart-row-height-2)"))
                        number_above = true;
                }
            }
        }
        add_assertion(result, "unframed_text_geometry",
                      text_origin && text_above, true, text_origin && text_above);
        add_assertion(result, "unframed_number_geometry_and_format",
                      number_origin && number_above, true,
                      number_origin && number_above);
        add_assertion(result, "native_condition_and_price_filtering",
                      filtered == 2, 2.0, static_cast<double>(filtered));
    return true;
}

}  // namespace tdx::recon_contract_detail
