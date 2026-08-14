#include "formula_render_internal.hpp"

#include <cmath>
#include <cstdint>
#include <string>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_language_detail;
using namespace formula_runtime_detail;

void apply_primitive_metadata(Json& primitive,
                              const PrimitiveRenderContext& context) {
    const auto& function = context.function;
    const auto& statement = context.statement;
    auto& env = context.environment;
    const auto& number_format = context.number_format;
    const bool part_line = context.part_line;
    if (function == "DRAWLINE" || function == "PLOYLINE") {
        const auto found = env.find(statement.name);
        if (statement.output && found != env.end()) {
            std::size_t count = 0;
            for (const auto value : found->second)
                if (std::isfinite(value)) ++count;
            primitive["value_source"] = "points.values." + statement.name;
            primitive["finite_point_count"] = static_cast<std::uint64_t>(count);
        }
        primitive["segment_coordinate_space"] = "bar-price";
        primitive["segment_connection"] = "straight";
    }
    if (function == "PLOYLINE") {
        primitive["polyline_condition_argument"] = 0;
        primitive["polyline_price_argument"] = 1;
        primitive["polyline_vertex_rule"] = "condition-true";
        primitive["polyline_connection_rule"] = "previous-vertex-to-current";
    }
    if (function == "DRAWLINE") {
        primitive["line_start_condition_argument"] = 0;
        primitive["line_start_price_argument"] = 1;
        primitive["line_end_condition_argument"] = 2;
        primitive["line_end_price_argument"] = 3;
        primitive["line_expand_argument"] = 4;
        Json expansions = Json::object();
        expansions["0"] = "none";
        expansions["1"] = "right";
        primitive["line_expansions"] = std::move(expansions);
    }
    if (function == "DRAWSL") {
        primitive["slope_native_render_type"] = 20;
        primitive["slope_native_renderer"] = "tdxw-sub_957A30";
        primitive["slope_coordinate_space"] = "bar-price-or-pixel-vertical";
        primitive["slope_condition_argument"] = 0;
        primitive["slope_price_argument"] = 1;
        primitive["slope_per_bar_argument"] = 2;
        primitive["slope_length_argument"] = 3;
        primitive["slope_direction_argument"] = 4;
        primitive["slope_condition_true_rule"] = "finite-and-abs(value)>=1e-5";
        primitive["slope_direction_evaluation"] = "final-bar-float-to-int";
        primitive["slope_length_evaluation"] = "per-event-float-to-int-bound";
        primitive["slope_vertical_sentinel"] = 10000;
        primitive["slope_vertical_length_unit"] = "pixels";
        Json directions = Json::object();
        directions["0"] = "right";
        directions["1"] = "left";
        directions["2"] = "both";
        primitive["slope_directions"] = std::move(directions);
    }
    if (function == "DRAWBMP") {
        primitive["bitmap_native_render_type"] = 9;
        primitive["bitmap_native_renderer"] = "tdxw-sub_959F60";
        primitive["bitmap_coordinate_space"] = "bar-price";
        primitive["bitmap_price_argument"] = 1;
        primitive["bitmap_name_argument"] = 2;
        primitive["bitmap_condition_true_rule"] = "abs(value-1)<0.0001";
        primitive["bitmap_resource_directory"] = "T0002/signals";
        primitive["bitmap_resource_extension"] = ".bmp";
        primitive["bitmap_resource_endpoint"] = "/api/v1/formulas/signal-image";
        primitive["bitmap_position_rule"] = "left=bar-x;top=price-y;natural-size";
    }
    if (function == "DRAWGBK") {
        primitive["pane_background_native_render_type"] = 10;
        primitive["pane_background_native_renderer"] = "tdxw-sub_961A20";
        primitive["pane_background_condition_argument"] = 0;
        primitive["pane_background_color_arguments"] = Json::array();
        primitive["pane_background_color_arguments"].push_back(1);
        primitive["pane_background_color_arguments"].push_back(2);
        primitive["pane_background_horizontal_argument"] = 3;
        primitive["pane_background_name_argument"] = 4;
        primitive["pane_background_stretch_argument"] = 5;
        primitive["pane_background_condition_true_rule"] = "abs(value-1)<0.0001";
        primitive["pane_background_scalar_argument_scope"] = "evaluation-first-bar";
        primitive["pane_background_mode_rule"] =
            "color1!=0||color2!=0?gradient:image";
        primitive["pane_background_gradient_direction_rule"] =
            "horizontal==0?vertical:horizontal";
        primitive["pane_background_resource_directory"] = "T0002/signals";
        primitive["pane_background_resource_precedence"] = "bmp-then-png";
        primitive["pane_background_resource_endpoint"] =
            "/api/v1/formulas/signal-image";
        primitive["pane_background_image_stretch_rule"] =
            "stretch!=0?pane-size:natural-size-at-pane-origin";
    }
    if (function == "DRAWRECTREL") {
        primitive["rectangle_native_render_type"] = 11;
        primitive["rectangle_native_renderer"] = "tdxw-sub_95A970";
        primitive["rectangle_coordinate_space"] = "pane-thousandths";
        primitive["rectangle_argument_order"] = Json::array();
        for (const auto* name : {"left", "top", "right", "bottom", "color"})
            primitive["rectangle_argument_order"].push_back(name);
        primitive["rectangle_coordinate_min"] = 0;
        primitive["rectangle_coordinate_max"] = 999;
        primitive["rectangle_fill_rule"] = "color!=0?solid:no-fill";
        primitive["rectangle_frame_rule"] = "NOFRAME?none:native-frame";
        primitive["rectangle_no_frame"] = directive_present(statement, "NOFRAME");
    }

    if (function == "DRAWNUMBER_DIF") {
        primitive["kind"] = "sequence-number";
        primitive["sequence_semantics"] = "increment-by-one-on-consecutive-bars";
        primitive["sequence_argument_order"] = Json::array();
        primitive["sequence_argument_order"].push_back("condition");
        primitive["sequence_argument_order"].push_back("style");
        primitive["sequence_argument_order"].push_back("start");
        primitive["sequence_argument_order"].push_back("count");
        primitive["sequence_condition_true_rule"] = "abs(value-1)<0.0001";
        primitive["sequence_active_rule"] =
            "first-trigger-wins-through-source-plus-count-minus-one";
        primitive["sequence_overlap_rule"] = "ignore-trigger-while-active";
        primitive["sequence_style_evaluation"] = "per-rendered-bar-exact-float";
        primitive["sequence_anchor"] = directive_present(statement, "DRAWABOVE")
            ? "bar-high" : "bar-low";
        primitive["sequence_limit"] = 250;
        Json styles = Json::object();
        styles["0"] = "plain";
        styles["1"] = "leader";
        styles["2"] = "leader-box";
        styles["other"] = "offset-without-leader";
        primitive["sequence_styles"] = std::move(styles);
        primitive["sequence_anchor_gap_pixels"] = 2;
        primitive["sequence_nonzero_offset_pixels"] = 10;
        primitive["sequence_leader_length_pixels"] = 10;
        primitive["sequence_leader_style"] = "dotted";
        primitive["sequence_leader_dot_step_pixels"] = 4;
        primitive["sequence_leader_accelerated_step_pixels"] = 5;
        primitive["sequence_box_width_numeric_pixels"] = 8;
        primitive["sequence_box_width_alpha_pixels"] = 14;
        primitive["sequence_box_height_pixels"] = 14;
        primitive["sequence_box_fill_alpha_byte"] = 0x50;
        primitive["sequence_box_fill_compositing"] = "gdiplus-argb-solid-fill";
        primitive["sequence_box_border_alpha_byte"] = 0xFF;
        primitive["sequence_box_border_width_pixels"] = 1;
        primitive["sequence_box_border_path"] = "closed-gdi-polyline";
        primitive["sequence_text_numeric_alignment"] = "right";
        primitive["sequence_text_alpha_alignment"] = "center";
        primitive["sequence_text_vertical_alignment"] = "center";
        primitive["sequence_default_side"] = "below-low";
        primitive["sequence_default_flip_rule"] =
            "box-bottom>=pane-bottom-40?above-high:below-low";
        primitive["sequence_drawabove_side"] = "above-high";
        primitive["sequence_drawabove_flip_rule"] =
            "box-top<=pane-top+25?below-low:above-high";
    }
    if (function == "DRAWTEXT" || function == "DRAWTEXT_FIX" ||
        function == "DRAWNUMBER" || function == "DRAWNUMBER_FIX" ||
        function == "DRAWNUMBER_DIF") {
        const int render_type = function == "DRAWTEXT" ? 4 :
            function == "DRAWNUMBER" ? 6 :
            function == "DRAWTEXT_FIX" ? 7 :
            function == "DRAWNUMBER_FIX" ? 8 : 23;
        const char* renderer = function == "DRAWTEXT" ? "tdxw-sub_961290" :
            function == "DRAWNUMBER" ? "tdxw-sub_9593C0" :
            function == "DRAWTEXT_FIX" ? "tdxw-sub_958F90" :
            function == "DRAWNUMBER_FIX" ? "tdxw-sub_959620" :
            "tdxw-sub_9626E0";
        primitive["annotation_native_render_type"] = render_type;
        primitive["annotation_native_renderer"] = renderer;
        primitive["annotation_font_selector"] = "tdxw-sub_68F020";
        primitive["annotation_font_table_index"] = 1;
        primitive["annotation_font_user_ini_ordinal"] = 2;
        primitive["annotation_font_config_section"] = "Other";
        primitive["annotation_font_face_key"] = "FONTNAME2";
        primitive["annotation_font_height_key"] = "FONTSIZE2";
        primitive["annotation_font_weight_key"] = "FontWeigth2";
        primitive["annotation_background_mode"] = "transparent";
        primitive["annotation_background_mode_value"] = 1;
        primitive["annotation_measurement_api"] = "GetTextExtentPoint32A";
        primitive["annotation_text_encoding"] = "Win32-ANSI";
        primitive["annotation_text_api"] = function == "DRAWNUMBER_DIF"
            ? "DrawTextA" : "TextOutA";
        const bool frame_directive = directive_present(statement, "DRAWCFRAME");
        primitive["annotation_frame_directive_present"] = frame_directive;
        primitive["annotation_frame_supported"] = function == "DRAWTEXT";
        primitive["annotation_frame_directive_effect"] = function == "DRAWTEXT"
            ? "native-price-text-frame" : function == "DRAWTEXT_FIX"
            ? "ignored-by-native-renderer" :
              "not-forwarded-to-native-renderer";
        if (function == "DRAWTEXT") {
            primitive["annotation_frame_native_renderer"] = "tdxw-sub_961290";
            primitive["annotation_frame_anchor"] = "bar-high-low";
            primitive["annotation_frame_price_argument_ignored"] = true;
            primitive["annotation_frame_drawabove_ignored"] = true;
            primitive["annotation_frame_side_rule"] =
                "available-below<=available-above?above:below";
            primitive["annotation_frame_horizontal_rule"] =
                "bar-x-minus-half-text-width";
            primitive["annotation_frame_leader_length_pixels"] = 20;
            primitive["annotation_frame_leader_style"] = "dotted";
            primitive["annotation_frame_leader_dot_step_pixels"] = 4;
            primitive["annotation_frame_leader_accelerated_step_pixels"] = 5;
            primitive["annotation_frame_corner_radius_pixels"] = 4;
            primitive["annotation_frame_width_padding_pixels"] = 5;
            primitive["annotation_frame_height_padding_pixels"] = 4;
            primitive["annotation_frame_text_inset_x_pixels"] = 3;
            primitive["annotation_frame_text_inset_y_pixels"] = 3;
            primitive["annotation_frame_fill_alpha_byte"] = 0x50;
            primitive["annotation_frame_fill_compositing"] =
                "gdiplus-argb-solid-fill";
            primitive["annotation_frame_border_alpha_byte"] = 0xFF;
            primitive["annotation_frame_border_width_pixels"] = 1;
            primitive["annotation_frame_smoothing_mode"] = 4;
            primitive["annotation_frame_multiline_rule"] =
                "per-line-overlap-same-anchor";
        }
        if (function == "DRAWNUMBER_DIF") {
            primitive["annotation_conditional_font_table_index"] = 13;
            primitive["annotation_conditional_font_rule"] =
                "first-rendered-bar-arg1==2";
            primitive["annotation_conditional_font_scope"] =
                "renderer-wide-before-event-loop";
            primitive["annotation_drawtext_common_flags"] = 0x824;
            primitive["annotation_drawtext_numeric_flags"] = 0x826;
            primitive["annotation_drawtext_alpha_flags"] = 0x825;
        }
    }
    if (function == "DRAWICON") {
        primitive["icon_coordinate_space"] = "bar-price";
        primitive["icon_price_argument"] = 1;
        primitive["icon_type_argument"] = 2;
        primitive["icon_official_type_min"] = 1;
        primitive["icon_official_type_max"] = 51;
        primitive["icon_sprite_cell_min"] = 1;
        primitive["icon_sprite_cell_max"] = 100;
        primitive["icon_sprite_cell_width"] = 18;
        primitive["icon_sprite_cell_height"] = 18;
        primitive["icon_sprite_indexing"] = "one-based-left-to-right";
        primitive["icon_sprite_endpoint"] =
            "/api/v1/formulas/drawicon-strip.png";
        primitive["icon_bitmap_endpoint"] =
            "/api/v1/formulas/drawicon-strip.bmp";
        primitive["icon_resource_type"] = 2;
        primitive["icon_resource_id"] = 2060;
        primitive["icon_renderer"] = "tcalc-resource-bitmap";
    }
    if (part_line) {
        primitive["segment_color_argument"] = 1;
        primitive["segment_direction_argument"] = 2;
        Json directions = Json::object();
        directions["0"] = "current-to-next";
        directions["1"] = "previous-to-current";
        primitive["segment_directions"] = std::move(directions);
    }
    if (function == "DRAWBAND") {
        Json value_arguments = Json::array();
        value_arguments.push_back(0); value_arguments.push_back(2);
        primitive["band_value_arguments"] = std::move(value_arguments);
        Json color_arguments = Json::array();
        color_arguments.push_back(1); color_arguments.push_back(3);
        primitive["band_color_arguments"] = std::move(color_arguments);
        primitive["band_fill_rule"] = "arg0>arg2?arg1:arg3";
        primitive["band_fill_opacity"] = 1;
        primitive["band_fill_compositing"] = "opaque-gdi-stroke-and-fill-path";
    }
    if (function == "STICKLINE") {
        Json price_arguments = Json::array();
        price_arguments.push_back(1); price_arguments.push_back(2);
        primitive["stick_price_arguments"] = std::move(price_arguments);
        primitive["stick_width_argument"] = 3;
        primitive["stick_empty_argument"] = 4;
        primitive["stick_width_standard"] = 4;
        primitive["stick_width_unit"] = "bar-spacing-ratio";
        primitive["stick_center_modes_price1_ignored"] = true;
        Json modes = Json::object();
        modes["0"] = "solid";
        modes["-1"] = "dashed-hollow";
        modes["1"] = "solid-hollow";
        modes["2"] = "center-full";
        modes["3"] = "center-half";
        modes["other-nonzero"] = "solid-hollow";
        primitive["stick_modes"] = std::move(modes);
    }
    if (function == "DRAWTEXT" || function == "DRAWNUMBER") {
        primitive["annotation_coordinate_space"] = "bar-price";
        primitive["annotation_price_argument"] = 1;
        primitive["annotation_text_argument"] = 2;
        primitive["annotation_line_break"] = "&";
        primitive["annotation_max_characters"] = 250;
        primitive["annotation_condition_true_rule"] = "abs(value-1)<0.0001";
        primitive["annotation_missing_price_rule"] = "skip-event";
        primitive["annotation_edge_behavior"] = "no-clamp-no-flip";
        primitive["annotation_collision_behavior"] = "none-source-order-overpaint";
        primitive["annotation_textout_y_adjustment_rule"] =
            "font-aux-mode==0?0:font-aux-mode==1?-1:-3";
        if (function == "DRAWTEXT") {
            primitive["annotation_max_lines"] = 10;
            primitive["annotation_unframed_horizontal_rule"] = "bar-x";
            primitive["annotation_unframed_base_y_rule"] = "price-y-minus-8";
            primitive["annotation_unframed_drawabove_rule"] =
                "price-y-line-count*(native-chart-row-height-2)-8";
            primitive["annotation_unframed_line_step_rule"] =
                "measured-text-height;empty-line-measured-A-height";
        } else {
            primitive["annotation_unframed_horizontal_rule"] = "bar-x-minus-3";
            primitive["annotation_unframed_base_y_rule"] = "price-y";
            primitive["annotation_unframed_drawabove_rule"] =
                "price-y-(native-chart-row-height-2)";
            primitive["annotation_number_chart_precision"] =
                number_format.chart_precision;
            primitive["annotation_number_chart_precision_source"] =
                number_format.chart_precision_source;
            primitive["annotation_number_index_info_format_mode"] =
                number_format.index_info_mode;
            primitive["annotation_number_index_info_format_precision"] =
                number_format.index_info_precision;
            primitive["annotation_number_index_info_source"] =
                number_format.index_info_source;
            primitive["annotation_number_format_rule"] =
                number_format.index_info_mode == 0
                    ? "sub_591950-integer-or-2/3-decimals"
                    : "sub_59C390-fixed-0..5-decimals-plus-1e-6";
        }
    }
    if (function == "DRAWTEXT_FIX" || function == "DRAWNUMBER_FIX") {
        primitive["annotation_coordinate_space"] = "pane-fraction";
        primitive["annotation_x_argument"] = 1;
        primitive["annotation_y_argument"] = 2;
        primitive["annotation_alignment_argument"] = 3;
        primitive["annotation_text_argument"] = 4;
        primitive["annotation_line_break"] = "&";
        primitive["annotation_max_characters"] = 250;
        Json alignments = Json::object();
        alignments["0"] = "left";
        alignments["1"] = "right";
        primitive["annotation_alignments"] = std::move(alignments);
    }
    if (function == "DRAWKLINE") {
        Json order = Json::array();
        order.push_back("high"); order.push_back("open");
        order.push_back("low"); order.push_back("close");
        primitive["candle_argument_order"] = std::move(order);
        primitive["candle_color_rule"] = "close>=open?up:down";
    }
    if (function == "DRAWGBK_DIV") {
        Json colors = Json::array(); colors.push_back(1); colors.push_back(2);
        primitive["background_color_arguments"] = std::move(colors);
        primitive["background_fill_mode_argument"] = 3;
        primitive["background_range_argument"] = 4;
        primitive["background_condition_scope"] = "per-bar-contiguous-regions";
        primitive["background_native_render_type"] = 21;
        primitive["background_native_renderer"] =
            "tdxw-sub_95A6C0-sub_95A330";
        primitive["background_condition_true_rule"] = "abs(value-1)<0.0001";
        primitive["background_region_rule"] =
            "maximal-contiguous-native-true-bars";
        primitive["background_region_horizontal_bounds"] =
            "first-bar-half-spacing-to-last-bar-half-spacing";
        Json modes = Json::object();
        modes["0"] = "vertical-gradient";
        modes["1"] = "horizontal-gradient";
        modes["2"] = "border";
        modes["3"] = "border-and-fill";
        for (int mode = 10; mode <= 20; ++mode)
            modes[std::to_string(mode)] = "alpha-solid";
        primitive["background_fill_modes"] = std::move(modes);
        primitive["background_alpha_mode_min"] = 10;
        primitive["background_alpha_mode_max"] = 20;
        primitive["background_alpha_rule"] = "255*(mode-10)/10";
        primitive["background_alpha_byte_denominator"] = 255;
        primitive["background_alpha_color_argument"] = 1;
        primitive["background_alpha_compositing"] = "gdiplus-argb-solid-fill";
        Json ranges = Json::object();
        ranges["0"] = "pane";
        ranges["1"] = "bar-high-low";
        ranges["2"] = "bar-open-close";
        primitive["background_ranges"] = std::move(ranges);
        Json aggregations = Json::object();
        aggregations["0"] = "whole-pane";
        aggregations["1"] = "region-high-low-extrema";
        aggregations["2"] = "region-first-open-last-close";
        primitive["background_range_aggregations"] = std::move(aggregations);
    }
}

void apply_primitive_post_metadata(Json& primitive,
                                   const PrimitiveRenderContext& context) {
    const auto& function = context.function;
    const auto& numeric_arguments = context.numeric_arguments;
    if (function == "DRAWNUMBER_DIF") {
        Json style_series = Json::array();
        if (numeric_arguments.size() > 1) {
            for (const double value : numeric_arguments[1])
                style_series.push_back(std::isfinite(value) ? Json(value) : Json(nullptr));
        }
        primitive["sequence_style_series_alignment"] = "document-points";
        primitive["sequence_style_series"] = std::move(style_series);
        const bool style2_font = numeric_arguments.size() > 1 &&
            !numeric_arguments[1].empty() && numeric_arguments[1][0] == 2.0;
        primitive["annotation_effective_font_table_index"] = style2_font ? 13 : 1;
        primitive["annotation_effective_font_basis"] =
            "evaluation-window-first-bar-fallback;frontend-recomputes-visible-first-bar";
    }
}

}  // namespace tdx::formula_render_detail
