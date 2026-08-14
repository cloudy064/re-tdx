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

bool validate_formula_render_primitives_contract(const std::string& contract_id,
                                                 const Json& document,
                                                 Json& result) {
        const bool sequence = contract_id == "formula-sqjz-sequence-live";
        const bool part_line = contract_id == "formula-wavekx-partline-live";
        const bool ichimoku = contract_id == "formula-ichimoku-stickline-live";
        const bool tjcjl = contract_id == "formula-tjcjl-stickline-live";
        const bool stickline = tjcjl || ichimoku;
        const bool cyx = contract_id == "formula-cyx-drawline-live";
        const bool fkx = contract_id == "formula-fkx-candles-live";
        const bool linestick = contract_id == "formula-slzt-linestick-live";
        const bool cpbs = contract_id == "formula-cpbs-text-live";
        const bool fscage = contract_id == "formula-fscage-number-live";
        const bool drawcframe = contract_id == "formula-drawcframe-inline-post";
        const bool drawicon = contract_id == "formula-drawicon-inline-post";
        const std::string expected_formula = sequence ? "SQJZ" : part_line ? "WAVEKX" :
                                             ichimoku ? "ICHIMOKU" : tjcjl ? "TJCJL" :
                                             cyx ? "CYX" : fkx ? "FKX" :
                                             linestick ? "SLZT" :
                                             cpbs ? "CPBS" : fscage ? "FSCAGE" :
                                             drawcframe ? "CONTRACT_DRAWCFRAME" :
                                             drawicon ? "CONTRACT_DRAWICON" : "RGBAND";
        const bool identity =
            string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
            string_is(member(document, "execution_mode"), "native-cpp") &&
            string_is(member(document, "formula"), expected_formula) &&
            string_is(member(document, "market"), "sz") &&
            string_is(member(document, "code"), "000001");
        add_assertion(result, "native_formula_identity", identity,
                      "native-cpp sz000001/" + expected_formula, identity);
        const auto* ir = member(document, "render_ir");
        const auto* primitives = ir ? member(*ir, "primitives") : nullptr;
        const bool ir_shape = ir &&
            string_is(member(*ir, "schema"), "tdx-formula-render-ir-v1") &&
            primitives && primitives->is_array() && primitives->size() > 0;
        add_assertion(result, "render_ir", ir_shape,
                      "tdx-formula-render-ir-v1 with primitives", ir_shape);

        if (cpbs || fscage || tjcjl || drawcframe) {
            const auto* environment = member(document, "render_environment");
            const auto* annotation = environment ? member(*environment, "annotation") : nullptr;
            const auto* font = annotation ? member(*annotation, "font") : nullptr;
            const auto* face = font ? member(*font, "face") : nullptr;
            const bool native_font_environment =
                environment &&
                string_is(member(*environment, "schema"),
                          "tdx-formula-render-environment-v1") &&
                string_is(member(*environment, "native_source"), "TdxW.exe") &&
                bool_is(member(*environment, "pixel_font_equivalent"), false) &&
                annotation &&
                string_is(member(*annotation, "background_mode"), "transparent") &&
                number_is(member(*annotation, "background_mode_value"), 1.0) &&
                string_is(member(*annotation, "text_encoding"), "Win32-ANSI") &&
                font &&
                number_is(member(*font, "native_font_table_index"), 1.0) &&
                number_is(member(*font, "user_ini_font_ordinal"), 2.0) &&
                face && face->is_string() && !face->as_string().empty() &&
                member(*font, "logical_height") &&
                member(*font, "logical_height")->is_number() &&
                member(*font, "weight") && member(*font, "weight")->is_number();
            add_assertion(result, "annotation_render_environment",
                          native_font_environment,
                          "TdxW font index 1 / user.ini ordinal 2 environment",
                          native_font_environment);
        }

        bool semantics = false;
        bool fixed_annotation_semantics = false;
        bool polyline_semantics = false;
        bool icon49 = false;
        bool icon51 = false;
        bool fixed_frame_ignored = false;
        bool number_frame_not_forwarded = false;
        std::size_t matching_primitives = 0;
        std::size_t matching_events = 0;
        if (primitives && primitives->is_array()) {
            for (const auto& primitive : primitives->as_array()) {
                const auto* events = member(primitive, "events");
                if (sequence &&
                    string_is(member(primitive, "function"), "DRAWNUMBER_DIF") &&
                    string_is(member(primitive, "kind"), "sequence-number") &&
                    string_is(member(primitive, "sequence_semantics"),
                              "increment-by-one-on-consecutive-bars")) {
                    ++matching_primitives;
                    if (events && events->is_array()) {
                        matching_events += events->size();
                        for (const auto& event : events->as_array()) {
                            const auto* index = member(event, "index");
                            const auto* source = member(event, "source_index");
                            const auto* label = member(event, "sequence_label");
                            const auto* anchor = member(event, "anchor");
                            if (index && source && index->is_number() && source->is_number() &&
                                index->as_number() > source->as_number() && label && label->is_string() &&
                                anchor && anchor->is_string() &&
                                (anchor->as_string() == "bar-low" ||
                                 anchor->as_string() == "bar-high"))
                                semantics = true;
                        }
                    }
                } else if (part_line &&
                           string_is(member(primitive, "function"), "PARTLINE")) {
                    ++matching_primitives;
                    const auto* directions = member(primitive, "segment_directions");
                    const bool direction_map = directions &&
                        string_is(member(*directions, "1"), "previous-to-current");
                    if (events && events->is_array()) {
                        matching_events += events->size();
                        for (const auto& event : events->as_array()) {
                            const auto* from = member(event, "segment_from_index");
                            const auto* to = member(event, "segment_to_index");
                            if (direction_map &&
                                string_is(member(event, "segment_direction"),
                                          "previous-to-current") &&
                                from && to && from->is_number() && to->is_number() &&
                                to->as_number() == from->as_number() + 1.0)
                                semantics = true;
                        }
                    }
                } else if (stickline &&
                           string_is(member(primitive, "function"), "STICKLINE") &&
                           number_is(member(primitive, "stick_width_standard"), 4.0) &&
                           bool_is(member(primitive,
                                          "stick_center_modes_price1_ignored"), true)) {
                    const auto* modes = member(primitive, "stick_modes");
                    const bool mode_map = modes &&
                        string_is(member(*modes, "0"), "solid") &&
                        string_is(member(*modes, "-1"), "dashed-hollow") &&
                        string_is(member(*modes, "2"), "center-full") &&
                        string_is(member(*modes, "3"), "center-half") &&
                        string_is(member(*modes, "other-nonzero"), "solid-hollow");
                    ++matching_primitives;
                    if (events && events->is_array()) {
                        matching_events += events->size();
                        for (const auto& event : events->as_array())
                            if (mode_map &&
                                string_is(member(event, "stick_mode"),
                                          ichimoku ? "solid-hollow" : "solid") &&
                                number_is(member(event, "stick_width"),
                                          ichimoku ? 2.0 : 1.0) &&
                                number_is(member(event, "stick_width_ratio"),
                                          ichimoku ? 0.5 : 0.25) &&
                                bool_is(member(event, "stick_price1_used"), true) &&
                                string_is(member(event, "stick_anchor"), "price-pair"))
                                semantics = true;
                    }
                } else if (cyx &&
                           string_is(member(primitive, "function"), "DRAWLINE") &&
                           string_is(member(primitive, "kind"), "draw-line") &&
                           string_is(member(primitive, "segment_coordinate_space"),
                                     "bar-price") &&
                           number_is(member(primitive, "line_start_condition_argument"), 0.0) &&
                           number_is(member(primitive, "line_start_price_argument"), 1.0) &&
                           number_is(member(primitive, "line_end_condition_argument"), 2.0) &&
                           number_is(member(primitive, "line_end_price_argument"), 3.0) &&
                           number_is(member(primitive, "line_expand_argument"), 4.0)) {
                    ++matching_primitives;
                    if (events && events->is_array()) {
                        matching_events += events->size();
                        for (const auto& event : events->as_array()) {
                            const auto* from = member(event, "segment_from_index");
                            const auto* anchor = member(event, "segment_anchor_to_index");
                            const auto* to = member(event, "segment_to_index");
                            if (from && from->is_number() && anchor && anchor->is_number() &&
                                to && to->is_number() &&
                                from->as_number() < anchor->as_number() &&
                                anchor->as_number() <= to->as_number() &&
                                string_is(member(event, "segment_expansion"), "right") &&
                                member(event, "segment_from_price") &&
                                member(event, "segment_from_price")->is_number() &&
                                member(event, "segment_anchor_to_price") &&
                                member(event, "segment_anchor_to_price")->is_number() &&
                                member(event, "segment_to_price") &&
                                member(event, "segment_to_price")->is_number() &&
                                member(event, "segment_slope_per_bar") &&
                                member(event, "segment_slope_per_bar")->is_number())
                                semantics = true;
                        }
                    }
                } else if (fkx &&
                           string_is(member(primitive, "function"), "DRAWKLINE") &&
                           string_is(member(primitive, "kind"), "candlestick") &&
                           string_is(member(primitive, "candle_color_rule"),
                                     "close>=open?up:down")) {
                    const auto* order = member(primitive, "candle_argument_order");
                    const bool argument_order = order && order->is_array() &&
                        order->size() == 4 &&
                        order->as_array()[0].is_string() &&
                        order->as_array()[0].as_string() == "high" &&
                        order->as_array()[1].is_string() &&
                        order->as_array()[1].as_string() == "open" &&
                        order->as_array()[2].is_string() &&
                        order->as_array()[2].as_string() == "low" &&
                        order->as_array()[3].is_string() &&
                        order->as_array()[3].as_string() == "close";
                    ++matching_primitives;
                    if (events && events->is_array()) {
                        matching_events += events->size();
                        for (const auto& event : events->as_array()) {
                            const auto* arguments = member(event, "arguments");
                            if (!argument_order || !arguments || !arguments->is_array() ||
                                arguments->size() != 4) continue;
                            const auto& values = arguments->as_array();
                            if (std::all_of(values.begin(), values.end(),
                                            [](const Json& value) {
                                                return value.is_number() &&
                                                       std::isfinite(value.as_number());
                                            }) &&
                                values[0].as_number() >=
                                    std::max(values[1].as_number(), values[3].as_number()) &&
                                values[2].as_number() <=
                                    std::min(values[1].as_number(), values[3].as_number()))
                                semantics = true;
                        }
                    }
                } else if (linestick &&
                           string_is(member(primitive, "statement"), "青龙") &&
                           string_is(member(primitive, "kind"), "line") &&
                           string_is(member(primitive, "series_native_mode"),
                                     "line-stick") &&
                           number_is(member(primitive, "series_native_render_type"), 5.0) &&
                           string_is(member(primitive, "series_native_renderer"),
                                     "tdxw-sub_957F70") &&
                           string_is(member(primitive, "series_native_stem_shape"),
                                     "one-pixel-vertical-line") &&
                           string_is(member(primitive, "series_native_missing_value_rule"),
                                     "break-contiguous-run") &&
                           bool_is(member(primitive, "line_stick"), true) &&
                           number_is(member(primitive, "line_stick_baseline"), 0.0) &&
                           string_is(member(primitive, "line_stick_draw_order"),
                                     "sticks-then-line")) {
                    const auto* components = member(primitive, "line_stick_components");
                    const auto* finite = member(primitive, "finite_point_count");
                    const bool component_order = components && components->is_array() &&
                        components->size() == 2 &&
                        components->as_array()[0].is_string() &&
                        components->as_array()[0].as_string() == "zero-baseline-stick" &&
                        components->as_array()[1].is_string() &&
                        components->as_array()[1].as_string() == "indicator-line";
                    ++matching_primitives;
                    if (component_order && finite && finite->is_number() &&
                        finite->as_number() > 0) {
                        semantics = true;
                        matching_events += static_cast<std::size_t>(finite->as_number());
                    }
                } else if (drawcframe &&
                           string_is(member(primitive, "function"), "DRAWTEXT") &&
                           bool_is(member(primitive, "annotation_frame_directive_present"), true) &&
                           bool_is(member(primitive, "annotation_frame_supported"), true) &&
                           string_is(member(primitive, "annotation_frame_directive_effect"),
                                     "native-price-text-frame") &&
                           string_is(member(primitive, "annotation_frame_native_renderer"),
                                     "tdxw-sub_961290") &&
                           string_is(member(primitive, "annotation_frame_anchor"),
                                     "bar-high-low") &&
                           bool_is(member(primitive,
                                          "annotation_frame_price_argument_ignored"), true) &&
                           bool_is(member(primitive,
                                          "annotation_frame_drawabove_ignored"), true) &&
                           string_is(member(primitive, "annotation_frame_side_rule"),
                                     "available-below<=available-above?above:below") &&
                           string_is(member(primitive, "annotation_frame_horizontal_rule"),
                                     "bar-x-minus-half-text-width") &&
                           number_is(member(primitive,
                                            "annotation_frame_leader_length_pixels"), 20.0) &&
                           string_is(member(primitive, "annotation_frame_leader_style"),
                                     "dotted") &&
                           number_is(member(primitive,
                                            "annotation_frame_leader_dot_step_pixels"), 4.0) &&
                           number_is(member(primitive,
                                            "annotation_frame_corner_radius_pixels"), 4.0) &&
                           number_is(member(primitive,
                                            "annotation_frame_width_padding_pixels"), 5.0) &&
                           number_is(member(primitive,
                                            "annotation_frame_height_padding_pixels"), 4.0) &&
                           number_is(member(primitive,
                                            "annotation_frame_text_inset_x_pixels"), 3.0) &&
                           number_is(member(primitive,
                                            "annotation_frame_text_inset_y_pixels"), 3.0) &&
                           number_is(member(primitive,
                                            "annotation_frame_fill_alpha_byte"), 80.0) &&
                           string_is(member(primitive, "annotation_frame_fill_compositing"),
                                     "gdiplus-argb-solid-fill") &&
                           number_is(member(primitive,
                                            "annotation_frame_border_alpha_byte"), 255.0) &&
                           number_is(member(primitive,
                                            "annotation_frame_border_width_pixels"), 1.0) &&
                           string_is(member(primitive, "annotation_frame_multiline_rule"),
                                     "per-line-overlap-same-anchor")) {
                    ++matching_primitives;
                    if (events && events->is_array()) {
                        matching_events += events->size();
                        for (const auto& event : events->as_array())
                            if (bool_is(member(event, "annotation_frame"), true) &&
                                bool_is(member(event, "annotation_text_available"), true))
                                semantics = true;
                    }
                } else if ((cpbs || fscage) &&
                           string_is(member(primitive, "function"),
                                     cpbs ? "DRAWTEXT" : "DRAWNUMBER") &&
                           string_is(member(primitive, "annotation_coordinate_space"),
                                     "bar-price") &&
                           number_is(member(primitive, "annotation_price_argument"), 1.0) &&
                           number_is(member(primitive, "annotation_text_argument"), 2.0) &&
                           string_is(member(primitive, "annotation_line_break"), "&") &&
                           number_is(member(primitive, "annotation_max_characters"), 250.0) &&
                           number_is(member(primitive, "annotation_native_render_type"),
                                     cpbs ? 4.0 : 6.0) &&
                           string_is(member(primitive, "annotation_native_renderer"),
                                     cpbs ? "tdxw-sub_961290" : "tdxw-sub_9593C0") &&
                           string_is(member(primitive, "annotation_font_selector"),
                                     "tdxw-sub_68F020") &&
                           number_is(member(primitive, "annotation_font_table_index"), 1.0) &&
                           number_is(member(primitive, "annotation_font_user_ini_ordinal"), 2.0) &&
                           string_is(member(primitive, "annotation_background_mode"),
                                     "transparent") &&
                           number_is(member(primitive, "annotation_background_mode_value"), 1.0) &&
                           string_is(member(primitive, "annotation_measurement_api"),
                                     "GetTextExtentPoint32A") &&
                           string_is(member(primitive, "annotation_text_api"), "TextOutA")) {
                    ++matching_primitives;
                    if (events && events->is_array()) {
                        matching_events += events->size();
                        for (const auto& event : events->as_array()) {
                            const auto* text = member(event, "annotation_text");
                            const auto* lines = member(event, "annotation_lines");
                            if (bool_is(member(event, "annotation_text_available"), true) &&
                                text && text->is_string() && !text->as_string().empty() &&
                                lines && lines->is_array() && lines->size() > 0 &&
                                member(event, "annotation_price") &&
                                member(event, "annotation_price")->is_number() &&
                                string_is(member(event, "annotation_horizontal_align"), "left") &&
                                member(event, "annotation_vertical_align") &&
                                member(event, "annotation_vertical_align")->is_string())
                                semantics = true;
                        }
                    }
                } else if (drawicon &&
                           string_is(member(primitive, "function"), "DRAWICON") &&
                           string_is(member(primitive, "kind"), "icon") &&
                           string_is(member(primitive, "icon_coordinate_space"), "bar-price") &&
                           string_is(member(primitive, "icon_renderer"),
                                     "tcalc-resource-bitmap") &&
                           string_is(member(primitive, "icon_sprite_endpoint"),
                                     "/api/v1/formulas/drawicon-strip.png") &&
                           number_is(member(primitive, "icon_resource_type"), 2.0) &&
                           number_is(member(primitive, "icon_resource_id"), 2060.0) &&
                           number_is(member(primitive, "icon_official_type_min"), 1.0) &&
                           number_is(member(primitive, "icon_official_type_max"), 51.0)) {
                    ++matching_primitives;
                    if (events && events->is_array()) {
                        matching_events += events->size();
                        for (const auto& event : events->as_array()) {
                            const bool common = bool_is(member(event,
                                    "icon_type_available"), true) &&
                                bool_is(member(event,
                                    "icon_sprite_cell_available"), true) &&
                                member(event, "icon_price") &&
                                member(event, "icon_price")->is_number();
                            if (common && number_is(member(event, "icon_type"), 49.0) &&
                                number_is(member(event, "icon_sprite_x"), 864.0) &&
                                string_is(member(event, "icon_vertical_align"),
                                          "below-price"))
                                icon49 = true;
                            if (common && number_is(member(event, "icon_type"), 51.0) &&
                                number_is(member(event, "icon_sprite_x"), 900.0) &&
                                string_is(member(event, "icon_vertical_align"),
                                          "above-price"))
                                icon51 = true;
                        }
                    }
                } else if (!sequence && !part_line && !stickline && !drawicon &&
                           string_is(member(primitive, "function"), "DRAWBAND") &&
                           string_is(member(primitive, "band_fill_rule"),
                                     "arg0>arg2?arg1:arg3") &&
                           number_is(member(primitive, "band_fill_opacity"), 1.0) &&
                           string_is(member(primitive, "band_fill_compositing"),
                                     "opaque-gdi-stroke-and-fill-path")) {
                    ++matching_primitives;
                    if (events && events->is_array()) {
                        matching_events += events->size();
                        for (const auto& event : events->as_array())
                            if (bool_is(member(event, "fill_color_available"), true) &&
                                member(event, "fill_color_argument") &&
                                member(event, "fill_color_argument")->is_number() &&
                                member(event, "fill_color_ref") &&
                                member(event, "fill_color_ref")->is_number() &&
                                member(event, "band_side") &&
                                member(event, "band_side")->is_string())
                                semantics = true;
                    }
                }
                if (drawcframe &&
                    string_is(member(primitive, "function"), "DRAWTEXT_FIX") &&
                    bool_is(member(primitive, "annotation_frame_directive_present"), true) &&
                    bool_is(member(primitive, "annotation_frame_supported"), false) &&
                    string_is(member(primitive, "annotation_frame_directive_effect"),
                              "ignored-by-native-renderer") &&
                    events && events->is_array()) {
                    for (const auto& event : events->as_array())
                        if (bool_is(member(event, "annotation_frame"), false))
                            fixed_frame_ignored = true;
                }
                if (drawcframe &&
                    string_is(member(primitive, "function"), "DRAWNUMBER_FIX") &&
                    bool_is(member(primitive, "annotation_frame_directive_present"), true) &&
                    bool_is(member(primitive, "annotation_frame_supported"), false) &&
                    string_is(member(primitive, "annotation_frame_directive_effect"),
                              "not-forwarded-to-native-renderer") &&
                    events && events->is_array()) {
                    for (const auto& event : events->as_array())
                        if (bool_is(member(event, "annotation_frame"), false))
                            number_frame_not_forwarded = true;
                }
                if (tjcjl &&
                    string_is(member(primitive, "function"), "DRAWTEXT_FIX") &&
                    string_is(member(primitive, "annotation_coordinate_space"),
                              "pane-fraction") &&
                    number_is(member(primitive, "annotation_native_render_type"), 7.0) &&
                    string_is(member(primitive, "annotation_native_renderer"),
                              "tdxw-sub_958F90") &&
                    number_is(member(primitive, "annotation_font_table_index"), 1.0) &&
                    string_is(member(primitive, "annotation_background_mode"),
                              "transparent") &&
                    string_is(member(primitive, "annotation_text_api"), "TextOutA")) {
                    const auto* alignments = member(primitive, "annotation_alignments");
                    if (events && events->is_array()) {
                        for (const auto& event : events->as_array())
                            if (alignments &&
                                string_is(member(*alignments, "0"), "left") &&
                                string_is(member(*alignments, "1"), "right") &&
                                bool_is(member(event, "annotation_text_available"), true) &&
                                member(event, "annotation_text") &&
                                member(event, "annotation_text")->is_string() &&
                                member(event, "annotation_lines") &&
                                member(event, "annotation_lines")->is_array() &&
                                number_is(member(event, "annotation_x"), 0.0) &&
                                number_is(member(event, "annotation_y"), 0.0) &&
                                string_is(member(event, "annotation_horizontal_align"), "left"))
                                fixed_annotation_semantics = true;
                    }
                }
                if (ichimoku &&
                    string_is(member(primitive, "function"), "PLOYLINE") &&
                    string_is(member(primitive, "kind"), "polyline") &&
                    string_is(member(primitive, "segment_coordinate_space"), "bar-price") &&
                    string_is(member(primitive, "polyline_vertex_rule"), "condition-true") &&
                    string_is(member(primitive, "polyline_connection_rule"),
                              "previous-vertex-to-current") &&
                    events && events->is_array()) {
                    for (const auto& event : events->as_array()) {
                        const auto* from = member(event, "segment_from_index");
                        const auto* to = member(event, "segment_to_index");
                        if (from && from->is_number() && to && to->is_number() &&
                            from->as_number() < to->as_number() &&
                            member(event, "segment_from_price") &&
                            member(event, "segment_from_price")->is_number() &&
                            member(event, "segment_to_price") &&
                            member(event, "segment_to_price")->is_number())
                            polyline_semantics = true;
                    }
                }
            }
        }
        if (drawicon) semantics = icon49 && icon51;
        const bool complete = semantics && matching_primitives >= (drawicon ? 2U : 1U) &&
                              matching_events > 0;
        add_assertion(result, sequence ? "expanded_sequence_events" :
                              part_line ? "directed_segment_events" :
                              stickline ? "native_stick_events" :
                              cyx ? "drawline_anchor_events" :
                              fkx ? "native_candle_events" :
                              linestick ? "line_stick_components" :
                              cpbs ? "price_text_events" :
                              fscage ? "price_number_events" :
                              drawcframe ? "native_drawcframe_geometry" :
                              drawicon ? "native_icon_events" : "dynamic_fill_events",
                      complete, true, complete);
        if (drawcframe) {
            add_assertion(result, "fixed_text_frame_ignored", fixed_frame_ignored,
                          true, fixed_frame_ignored);
            add_assertion(result, "fixed_number_frame_not_forwarded",
                          number_frame_not_forwarded, true,
                          number_frame_not_forwarded);
        }
        if (tjcjl)
            add_assertion(result, "fixed_text_annotation", fixed_annotation_semantics,
                          true, fixed_annotation_semantics);
        if (ichimoku)
            add_assertion(result, "polyline_vertex_segments", polyline_semantics,
                          true, polyline_semantics);
        if (sequence || part_line || ichimoku || cyx)
            add_assertion(result, "future_read_only",
                          string_is(member(document, "future_execution_mode"),
                                    "explicit-read-only-lookahead"),
                          "explicit-read-only-lookahead",
                          value_or_null(member(document, "future_execution_mode")));
    return true;
}

}  // namespace tdx::recon_contract_detail
