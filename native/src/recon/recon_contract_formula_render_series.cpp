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

bool validate_formula_series_sticks_contract(const std::string& contract_id,
                                             const Json& document,
                                             Json& result) {
    (void)contract_id;
        const bool identity =
            string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
            string_is(member(document, "execution_mode"), "native-cpp") &&
            string_is(member(document, "formula_source_mode"), "inline-post") &&
            string_is(member(document, "formula"), "CONTRACT_SERIES_STICKS") &&
            string_is(member(document, "market"), "sz") &&
            string_is(member(document, "code"), "000001");
        add_assertion(result, "native_formula_identity", identity,
                      "inline-post native-cpp sz000001/CONTRACT_SERIES_STICKS",
                      identity);

        const auto* environment = member(document, "render_environment");
        const auto* sticks = environment ? member(*environment, "series_sticks") : nullptr;
        const auto* keys = sticks ? member(*sticks, "config_keys") : nullptr;
        const bool environment_shape = environment && sticks && keys &&
            string_is(member(*environment, "schema"),
                      "tdx-formula-render-environment-v1") &&
            string_is(member(*environment, "native_source"), "TdxW.exe") &&
            string_is(member(*keys, "section"), "Other") &&
            string_is(member(*keys, "real_up_k"), "RealUPK") &&
            string_is(member(*keys, "vol_k_use_zt"), "VolKUseZT") &&
            bool_is(member(*sticks, "real_up_k"), false) &&
            bool_is(member(*sticks, "vol_k_use_zt"), false) &&
            string_is(member(*sticks, "volume_color_rule"),
                      "open-close-with-flat-previous-close-fallback") &&
            string_is(member(*sticks, "volume_up_fill"), "hollow") &&
            string_is(member(*sticks, "volume_down_fill"), "solid") &&
            number_is(member(*sticks, "native_up_pen_index"), 2.0) &&
            number_is(member(*sticks, "native_down_pen_index"), 3.0);
        add_assertion(result, "native_series_stick_environment", environment_shape,
                      "Other/RealUPK=0,Other/VolKUseZT=0 native render profile",
                      environment_shape);

        bool color_shape = false, volume_shape = false;
        bool color_up = false, color_down = false;
        std::size_t color_events = 0, volume_events = 0;
        const auto* ir = member(document, "render_ir");
        const auto* primitives = ir ? member(*ir, "primitives") : nullptr;
        if (ir && string_is(member(*ir, "schema"), "tdx-formula-render-ir-v1") &&
            primitives && primitives->is_array()) {
            for (const auto& primitive : primitives->as_array()) {
                const auto* events = member(primitive, "events");
                if (!events || !events->is_array()) continue;
                if (string_is(member(primitive, "series_stick_mode"), "color-stick")) {
                    color_shape =
                        number_is(member(primitive, "series_stick_native_render_type"), 2.0) &&
                        string_is(member(primitive, "series_stick_native_renderer"),
                                  "tdxw-sub_9555B0") &&
                        number_is(member(primitive, "series_stick_baseline"), 0.0) &&
                        string_is(member(primitive, "series_stick_coordinate_space"),
                                  "bar-zero-baseline") &&
                        bool_is(member(primitive, "series_stick_value_float_cast"), true) &&
                        string_is(member(primitive, "series_stick_shape"),
                                  "one-pixel-vertical-line") &&
                        string_is(member(primitive, "series_stick_color_rule"),
                                  "float(value)*10000>epsilon?up:<epsilon?down:none") &&
                        string_is(member(primitive, "series_stick_width_rule"),
                                  "spacing>=3?spacing-max(spacing*0.25,2):min(spacing,1);half=floor(body*0.5);pixels=2*half+1") &&
                        number_is(member(primitive, "event_count"),
                                  static_cast<double>(events->size()));
                    for (const auto& event : events->as_array()) {
                        const auto* role = member(event, "series_stick_color_role");
                        const auto* value = member(event, "series_stick_value");
                        const auto* arguments = member(event, "arguments");
                        color_shape = color_shape && role && role->is_string() && value &&
                            value->is_number() && arguments && arguments->is_array() &&
                            arguments->size() == 1 && arguments->as_array()[0].is_number();
                        if (role && role->is_string() && role->as_string() == "up") color_up = true;
                        if (role && role->is_string() && role->as_string() == "down") color_down = true;
                    }
                    color_events += events->size();
                }
                if (string_is(member(primitive, "series_stick_mode"), "volume-stick")) {
                    volume_shape =
                        number_is(member(primitive, "series_stick_native_render_type"), 1.0) &&
                        string_is(member(primitive, "series_stick_native_renderer"),
                                  "tdxw-sub_957030") &&
                        string_is(member(primitive, "series_stick_shape"), "volume-body") &&
                        string_is(member(primitive, "series_stick_color_rule"),
                                  "VolKUseZT?previous-close:open-close-with-flat-previous-close-fallback") &&
                        string_is(member(primitive, "series_stick_up_fill_rule"),
                                  "RealUPK?solid:hollow") &&
                        string_is(member(primitive, "series_stick_down_fill"), "solid") &&
                        string_is(member(primitive, "series_stick_real_up_k_config_key"),
                                  "Other/RealUPK") &&
                        string_is(member(primitive, "series_stick_vol_k_use_zt_config_key"),
                                  "Other/VolKUseZT") &&
                        number_is(member(primitive, "event_count"),
                                  static_cast<double>(events->size()));
                    for (const auto& event : events->as_array()) {
                        const auto* open_close = member(
                            event, "series_stick_open_close_color_role");
                        const auto* previous = member(
                            event, "series_stick_previous_close_color_role");
                        const auto valid_role = [](const Json* role) {
                            return role && role->is_string() &&
                                (role->as_string() == "up" || role->as_string() == "down");
                        };
                        volume_shape = volume_shape && valid_role(open_close) &&
                            valid_role(previous) &&
                            member(event, "series_stick_value") &&
                            member(event, "series_stick_value")->is_number();
                    }
                    volume_events += events->size();
                }
            }
        }
        const bool native_color = color_shape && color_up && color_down && color_events > 1;
        const bool native_volume = volume_shape && volume_events > 1;
        add_assertion(result, "native_colorstick_events", native_color,
                      "type 2 one-pixel lines with materialized up/down roles",
                      native_color);
        add_assertion(result, "native_volstick_events", native_volume,
                      "type 1 volume bodies with both native color roles",
                      native_volume);
    return true;
}

bool validate_formula_native_series_styles_contract(const std::string& contract_id,
                                                    const Json& document,
                                                    Json& result) {
    (void)contract_id;
        const bool identity =
            string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
            string_is(member(document, "execution_mode"), "native-cpp") &&
            string_is(member(document, "formula_source_mode"), "inline-post") &&
            string_is(member(document, "formula"), "CONTRACT_NATIVE_SERIES_STYLES") &&
            string_is(member(document, "market"), "sz") &&
            string_is(member(document, "code"), "000001");
        add_assertion(result, "native_formula_identity", identity,
                      "inline-post native-cpp sz000001/CONTRACT_NATIVE_SERIES_STYLES",
                      identity);

        const auto* environment = member(document, "render_environment");
        const auto* series_lines = environment ? member(*environment, "series_lines") : nullptr;
        const auto* keys = series_lines ? member(*series_lines, "config_keys") : nullptr;
        const bool environment_shape = environment && series_lines && keys &&
            string_is(member(*keys, "section"), "Other") &&
            string_is(member(*keys, "bold_zb_line"), "BoldZBLine") &&
            bool_is(member(*series_lines, "bold_zb_line"), false) &&
            string_is(member(*series_lines, "effective_default_width_rule"),
                      "width==1&&BoldZBLine?2:width;skip-if-width<1") &&
            string_is(member(*series_lines, "native_source"),
                      "TdxW.exe!sub_957620");
        add_assertion(result, "native_series_line_environment", environment_shape,
                      "Other/BoldZBLine=0 and sub_957620 width rule",
                      environment_shape);

        bool base = false, dotted = false, stem = false, both = false;
        bool circle = false, cross = false, point = false;
        bool last_dot = false, last_color = false;
        const auto* ir = member(document, "render_ir");
        const auto* primitives = ir ? member(*ir, "primitives") : nullptr;
        const bool ir_shape = ir &&
            string_is(member(*ir, "schema"), "tdx-formula-render-ir-v1") &&
            primitives && primitives->is_array() && primitives->size() == 9;
        if (ir_shape) {
            for (const auto& primitive : primitives->as_array()) {
                const bool common =
                    string_is(member(primitive, "series_native_coordinate_space"),
                              "bar-price") &&
                    string_is(member(primitive, "series_native_pen_width_source"),
                              "LINETHICK-or-default-1");
                if (string_is(member(primitive, "statement"), "BASE")) {
                    base = common &&
                        string_is(member(primitive, "series_native_mode"), "line") &&
                        number_is(member(primitive, "series_native_render_type"), 0.0) &&
                        string_is(member(primitive, "series_native_renderer"),
                                  "tdxw-sub_957620") &&
                        string_is(member(primitive, "series_native_pen_style"),
                                  "PS_SOLID") &&
                        string_is(member(primitive, "series_native_missing_value_rule"),
                                  "break-contiguous-run") &&
                        string_is(member(primitive, "series_native_single_point_rule"),
                                  "x-minus-3-to-x-horizontal");
                } else if (string_is(member(primitive, "statement"), "DOTTED")) {
                    const auto* style = member(primitive, "style");
                    dotted = common && style && bool_is(member(*style, "dot_line"), true) &&
                        number_is(member(*style, "line_thickness"), 2.0) &&
                        string_is(member(primitive, "series_native_mode"), "dot-line") &&
                        number_is(member(primitive, "series_native_render_type"), 9.0) &&
                        string_is(member(primitive, "series_native_renderer"),
                                  "tdxw-sub_957620") &&
                        string_is(member(primitive, "series_native_pen_style"), "PS_DOT") &&
                        string_is(member(primitive, "series_native_missing_value_rule"),
                                  "break-contiguous-run") &&
                        string_is(member(primitive, "series_native_single_point_rule"),
                                  "x-minus-3-to-x-horizontal") &&
                        number_is(member(primitive, "finite_point_count"), 2.0);
                } else if (string_is(member(primitive, "statement"), "STEM")) {
                    stem = common &&
                        string_is(member(primitive, "series_native_mode"), "stick") &&
                        number_is(member(primitive, "series_native_render_type"), 4.0) &&
                        string_is(member(primitive, "series_native_renderer"),
                                  "tdxw-sub_957D70") &&
                        number_is(member(primitive, "series_native_stem_baseline"), 0.0) &&
                        string_is(member(primitive, "series_native_stem_shape"),
                                  "one-pixel-vertical-line");
                } else if (string_is(member(primitive, "statement"), "BOTH")) {
                    both = common &&
                        string_is(member(primitive, "series_native_mode"), "line-stick") &&
                        number_is(member(primitive, "series_native_render_type"), 5.0) &&
                        string_is(member(primitive, "series_native_renderer"),
                                  "tdxw-sub_957F70") &&
                        bool_is(member(primitive, "line_stick"), true) &&
                        string_is(member(primitive, "line_stick_draw_order"),
                                  "sticks-then-line") &&
                        string_is(member(primitive, "series_native_missing_value_rule"),
                                  "break-contiguous-run") &&
                        string_is(member(primitive, "series_native_stem_shape"),
                                  "one-pixel-vertical-line");
                } else if (string_is(member(primitive, "statement"), "CIRCLE")) {
                    circle = common &&
                        string_is(member(primitive, "series_native_mode"), "circle-dot") &&
                        number_is(member(primitive, "series_native_render_type"), 6.0) &&
                        string_is(member(primitive, "series_native_renderer"),
                                  "tdxw-sub_95ABA0") &&
                        string_is(member(primitive, "series_native_point_geometry_rule"),
                                  "spacing<6?four-cardinal-pixels:hollow-circle-radius-3");
                } else if (string_is(member(primitive, "statement"), "CROSS")) {
                    cross = common &&
                        string_is(member(primitive, "series_native_mode"), "cross-dot") &&
                        number_is(member(primitive, "series_native_render_type"), 7.0) &&
                        string_is(member(primitive, "series_native_renderer"),
                                  "tdxw-sub_95AE00") &&
                        string_is(member(primitive, "series_native_point_geometry_rule"),
                                  "spacing<5?diagonal-radius-1:spacing<10?diagonal-radius-2:diagonal-radius-3");
                } else if (string_is(member(primitive, "statement"), "POINT")) {
                    point = common &&
                        string_is(member(primitive, "series_native_mode"), "point-dot") &&
                        number_is(member(primitive, "series_native_render_type"), 8.0) &&
                        string_is(member(primitive, "series_native_renderer"),
                                  "tdxw-sub_95B0A0") &&
                        string_is(member(primitive, "series_native_point_geometry_rule"),
                                  "width<2?one-pixel:filled-ellipse-diameter-width");
                } else if (string_is(member(primitive, "statement"), "LASTDOT")) {
                    const auto* style = member(primitive, "style");
                    last_dot = common && style && bool_is(member(*style, "dot_line"), true) &&
                        string_is(member(primitive, "series_native_mode"), "dot-line") &&
                        number_is(member(primitive, "series_native_render_type"), 9.0) &&
                        member(primitive, "series_stick_mode") == nullptr;
                } else if (string_is(member(primitive, "statement"), "LASTCOLOR")) {
                    const auto* style = member(primitive, "style");
                    last_color = style && bool_is(member(*style, "dot_line"), false) &&
                        string_is(member(primitive, "series_stick_mode"), "color-stick") &&
                        number_is(member(primitive, "series_stick_native_render_type"), 2.0) &&
                        member(primitive, "series_native_mode") == nullptr;
                }
            }
        }
        const bool line_shapes = ir_shape && base && dotted && stem && both;
        const bool point_shapes = ir_shape && circle && cross && point;
        add_assertion(result, "native_line_and_stem_styles", line_shapes,
                      "type 0/4/5/9 renderers, broken runs and singleton rule",
                      line_shapes);
        add_assertion(result, "native_point_styles", point_shapes,
                      "type 6/7/8 zoom-sensitive native point geometry",
                      point_shapes);
        const bool last_renderer = ir_shape && last_dot && last_color;
        add_assertion(result, "last_renderer_directive_wins", last_renderer,
                      "COLORSTICK,DOTLINE=>type9; DOTLINE,COLORSTICK=>type2",
                      last_renderer);
    return true;
}

}  // namespace tdx::recon_contract_detail
