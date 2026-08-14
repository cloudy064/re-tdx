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

bool validate_formula_sequence_annotation_contract(const std::string& contract_id,
                                                   const Json& document,
                                                   Json& result) {
    (void)contract_id;
        const bool identity =
            string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
            string_is(member(document, "execution_mode"), "native-cpp") &&
            string_is(member(document, "formula_source_mode"), "inline-post") &&
            string_is(member(document, "formula"), "CONTRACT_DRAWNUMBER_DIF") &&
            string_is(member(document, "market"), "sz") &&
            string_is(member(document, "code"), "000001");
        add_assertion(result, "native_formula_identity", identity,
                      "inline-post native-cpp sz000001/CONTRACT_DRAWNUMBER_DIF",
                      identity);

        const auto* environment = member(document, "render_environment");
        const auto* annotation = environment ? member(*environment, "annotation") : nullptr;
        const auto* conditional = annotation
            ? member(*annotation, "conditional_fonts") : nullptr;
        const auto* style2_font = conditional ? member(*conditional, "13") : nullptr;
        const bool native_style2_font =
            environment &&
            string_is(member(*environment, "schema"),
                      "tdx-formula-render-environment-v1") &&
            string_is(member(*environment, "native_source"), "TdxW.exe") &&
            bool_is(member(*environment, "pixel_font_equivalent"), false) &&
            style2_font &&
            number_is(member(*style2_font, "native_font_table_index"), 13.0) &&
            string_is(member(*style2_font, "selected_profile"),
                      "recovered-drawnumber-dif-style2-table") &&
            string_is(member(*style2_font, "native_source"),
                      "TdxW.exe!sub_690350") &&
            string_is(member(*style2_font, "face"), "Arial") &&
            number_is(member(*style2_font, "logical_height"), 15.0) &&
            number_is(member(*style2_font, "weight"), 400.0);
        add_assertion(result, "native_style2_font", native_style2_font,
                      "TdxW hard-coded font table index 13 Arial/+15/400",
                      native_style2_font);

        const auto* ir = member(document, "render_ir");
        const auto* primitives = ir ? member(*ir, "primitives") : nullptr;
        const bool ir_shape = ir &&
            string_is(member(*ir, "schema"), "tdx-formula-render-ir-v1") &&
            primitives && primitives->is_array();
        add_assertion(result, "render_ir", ir_shape,
                      "tdx-formula-render-ir-v1 with primitives", ir_shape);

        bool style2_geometry = false;
        bool style1_geometry = false;
        bool style0_geometry = false;
        bool overlap_state = false;
        std::size_t sequence_primitives = 0;
        if (primitives && primitives->is_array()) {
            for (const auto& primitive : primitives->as_array()) {
                if (!string_is(member(primitive, "function"), "DRAWNUMBER_DIF") ||
                    !string_is(member(primitive, "kind"), "sequence-number"))
                    continue;
                ++sequence_primitives;
                const auto* order = member(primitive, "sequence_argument_order");
                const auto* styles = member(primitive, "sequence_styles");
                const bool argument_order = order && order->is_array() &&
                    order->size() == 4 &&
                    order->as_array()[0].is_string() &&
                    order->as_array()[0].as_string() == "condition" &&
                    order->as_array()[1].is_string() &&
                    order->as_array()[1].as_string() == "style" &&
                    order->as_array()[2].is_string() &&
                    order->as_array()[2].as_string() == "start" &&
                    order->as_array()[3].is_string() &&
                    order->as_array()[3].as_string() == "count";
                const bool native_common =
                    argument_order &&
                    string_is(member(primitive, "sequence_semantics"),
                              "increment-by-one-on-consecutive-bars") &&
                    string_is(member(primitive, "sequence_condition_true_rule"),
                              "abs(value-1)<0.0001") &&
                    string_is(member(primitive, "sequence_active_rule"),
                              "first-trigger-wins-through-source-plus-count-minus-one") &&
                    string_is(member(primitive, "sequence_overlap_rule"),
                              "ignore-trigger-while-active") &&
                    string_is(member(primitive, "sequence_style_evaluation"),
                              "per-rendered-bar-exact-float") &&
                    number_is(member(primitive, "sequence_limit"), 250.0) &&
                    styles &&
                    string_is(member(*styles, "0"), "plain") &&
                    string_is(member(*styles, "1"), "leader") &&
                    string_is(member(*styles, "2"), "leader-box") &&
                    string_is(member(*styles, "other"),
                              "offset-without-leader") &&
                    number_is(member(primitive, "sequence_anchor_gap_pixels"), 2.0) &&
                    number_is(member(primitive, "sequence_nonzero_offset_pixels"), 10.0) &&
                    number_is(member(primitive, "sequence_leader_length_pixels"), 10.0) &&
                    string_is(member(primitive, "sequence_leader_style"), "dotted") &&
                    number_is(member(primitive, "sequence_leader_dot_step_pixels"), 4.0) &&
                    number_is(member(primitive,
                                     "sequence_leader_accelerated_step_pixels"), 5.0) &&
                    number_is(member(primitive, "sequence_box_width_numeric_pixels"), 8.0) &&
                    number_is(member(primitive, "sequence_box_width_alpha_pixels"), 14.0) &&
                    number_is(member(primitive, "sequence_box_height_pixels"), 14.0) &&
                    number_is(member(primitive, "sequence_box_fill_alpha_byte"), 80.0) &&
                    string_is(member(primitive, "sequence_box_fill_compositing"),
                              "gdiplus-argb-solid-fill") &&
                    number_is(member(primitive, "sequence_box_border_alpha_byte"), 255.0) &&
                    number_is(member(primitive, "sequence_box_border_width_pixels"), 1.0) &&
                    string_is(member(primitive, "sequence_box_border_path"),
                              "closed-gdi-polyline") &&
                    string_is(member(primitive, "sequence_default_flip_rule"),
                              "box-bottom>=pane-bottom-40?above-high:below-low") &&
                    string_is(member(primitive, "sequence_drawabove_flip_rule"),
                              "box-top<=pane-top+25?below-low:above-high") &&
                    number_is(member(primitive, "annotation_native_render_type"), 23.0) &&
                    string_is(member(primitive, "annotation_native_renderer"),
                              "tdxw-sub_9626E0") &&
                    number_is(member(primitive,
                                     "annotation_conditional_font_table_index"), 13.0) &&
                    string_is(member(primitive, "annotation_conditional_font_rule"),
                              "first-rendered-bar-arg1==2") &&
                    string_is(member(primitive, "annotation_conditional_font_scope"),
                              "renderer-wide-before-event-loop") &&
                    string_is(member(primitive, "sequence_style_series_alignment"),
                              "document-points") &&
                    string_is(member(primitive, "annotation_effective_font_basis"),
                              "evaluation-window-first-bar-fallback;frontend-recomputes-visible-first-bar") &&
                    member(primitive, "sequence_style_series") &&
                    member(primitive, "sequence_style_series")->is_array() &&
                    member(primitive, "sequence_style_series")->size() == 120;
                const auto* events = member(primitive, "events");
                if (!native_common || !events || !events->is_array() || !events->size())
                    continue;
                const auto& values = events->as_array();
                const auto* first_style = member(values.front(), "sequence_style");
                if (!first_style || !first_style->is_number()) continue;
                const double style = first_style->as_number();
                if (style == 2.0) {
                    bool event_shape = values.size() == 10;
                    std::set<int> indexes;
                    std::set<int> sources;
                    for (const auto& event : values) {
                        const auto* index = member(event, "index");
                        const auto* source = member(event, "source_index");
                        const auto* offset = member(event, "sequence_offset");
                        if (!index || !source || !offset || !index->is_number() ||
                            !source->is_number() || !offset->is_number()) {
                            event_shape = false;
                            continue;
                        }
                        const int index_value = static_cast<int>(index->as_number());
                        const int source_value = static_cast<int>(source->as_number());
                        const int offset_value = static_cast<int>(offset->as_number());
                        indexes.insert(index_value);
                        sources.insert(source_value);
                        const std::string expected_label(
                            1, static_cast<char>('A' + offset_value));
                        event_shape = event_shape &&
                            index_value - source_value == offset_value &&
                            offset_value >= 0 && offset_value < 4 &&
                            number_is(member(event, "sequence_source_start"), 11.0) &&
                            number_is(member(event, "sequence_source_count"), 4.0) &&
                            string_is(member(event, "sequence_style_mode"), "leader-box") &&
                            string_is(member(event, "sequence_label"), expected_label) &&
                            string_is(member(event, "sequence_label_class"), "alpha") &&
                            bool_is(member(event, "sequence_leader"), true) &&
                            bool_is(member(event, "sequence_boxed"), true) &&
                            number_is(member(event, "sequence_offset_pixels"), 10.0) &&
                            number_is(member(event, "sequence_box_width_pixels"), 14.0) &&
                            number_is(member(event, "sequence_box_height_pixels"), 14.0) &&
                            string_is(member(event, "sequence_text_alignment"), "center") &&
                            number_is(member(event, "sequence_drawtext_flags"), 0x825) &&
                            string_is(member(event, "anchor"), "bar-high");
                    }
                    style2_geometry = event_shape &&
                        number_is(member(primitive,
                                         "annotation_effective_font_table_index"), 13.0);
                    overlap_state = event_shape && indexes.size() == 10 &&
                                    sources.size() == 3;
                } else if (style == 1.0) {
                    const auto& event = values.front();
                    style1_geometry = values.size() == 3 &&
                        number_is(member(primitive,
                                         "annotation_effective_font_table_index"), 1.0) &&
                        string_is(member(event, "sequence_style_mode"), "leader") &&
                        string_is(member(event, "sequence_label_class"), "numeric") &&
                        bool_is(member(event, "sequence_leader"), true) &&
                        bool_is(member(event, "sequence_boxed"), false) &&
                        number_is(member(event, "sequence_offset_pixels"), 10.0) &&
                        number_is(member(event, "sequence_box_width_pixels"), 8.0) &&
                        string_is(member(event, "sequence_text_alignment"), "right") &&
                        number_is(member(event, "sequence_drawtext_flags"), 0x826) &&
                        string_is(member(event, "anchor"), "bar-low");
                } else if (style == 0.0) {
                    const auto& event = values.front();
                    style0_geometry = values.size() == 2 &&
                        string_is(member(event, "sequence_style_mode"), "plain") &&
                        bool_is(member(event, "sequence_leader"), false) &&
                        bool_is(member(event, "sequence_boxed"), false) &&
                        number_is(member(event, "sequence_offset_pixels"), 0.0);
                }
            }
        }
        const bool native_styles = sequence_primitives == 3 &&
            style2_geometry && style1_geometry && style0_geometry;
        add_assertion(result, "native_sequence_styles", native_styles,
                      "STYLE 2/1/0 exact box, leader, alignment, font and pixel geometry",
                      native_styles);
        add_assertion(result, "native_active_sequence", overlap_state,
                      "10 unique events from 3 non-overlapping first-trigger-wins runs",
                      overlap_state);
    return true;
}

}  // namespace tdx::recon_contract_detail
