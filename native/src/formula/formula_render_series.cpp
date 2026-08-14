#include "formula_render_internal.hpp"

#include <cmath>
#include <cstdint>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_language_detail;
using namespace formula_runtime_detail;

bool apply_series_render(Json& primitive,
                         const Statement& statement,
                         Environment& env,
                         const std::vector<Bar>& bars) {
    const auto found = env.find(statement.name);
    if (found == env.end()) return false;
        std::size_t count = 0;
        for (const auto value : found->second) if (std::isfinite(value)) ++count;
        primitive["value_source"] = "points.values." + statement.name;
        primitive["finite_point_count"] = static_cast<std::uint64_t>(count);
        const auto selected_series_mode = native_series_mode(statement);
        const bool color_stick = selected_series_mode == "color-stick";
        const bool volume_stick = selected_series_mode == "volume-stick";
        if (!color_stick && !volume_stick) {
            const auto& mode = selected_series_mode;
            primitive["series_native_mode"] = mode;
            primitive["series_native_coordinate_space"] = "bar-price";
            primitive["series_native_pen_width_source"] =
                "LINETHICK-or-default-1";

            if (mode == "stick" || mode == "line-stick") {
                primitive["series_native_render_type"] = mode == "stick" ? 4 : 5;
                primitive["series_native_renderer"] = mode == "stick"
                    ? "tdxw-sub_957D70" : "tdxw-sub_957F70";
                primitive["series_native_pen_style"] = "PS_SOLID";
                primitive["series_native_stem_baseline"] = 0;
                primitive["series_native_stem_shape"] =
                    "one-pixel-vertical-line";
                if (mode == "line-stick") {
                    primitive["series_native_missing_value_rule"] =
                        "break-contiguous-run";
                    primitive["line_stick"] = true;
                    Json components = Json::array();
                    components.push_back("zero-baseline-stick");
                    components.push_back("indicator-line");
                    primitive["line_stick_components"] = std::move(components);
                    primitive["line_stick_baseline"] = 0;
                    primitive["line_stick_draw_order"] = "sticks-then-line";
                }
            } else if (mode == "circle-dot") {
                primitive["series_native_render_type"] = 6;
                primitive["series_native_renderer"] = "tdxw-sub_95ABA0";
                primitive["series_native_pen_style"] = "PS_SOLID";
                primitive["series_native_point_geometry_rule"] =
                    "spacing<6?four-cardinal-pixels:hollow-circle-radius-3";
            } else if (mode == "cross-dot") {
                primitive["series_native_render_type"] = 7;
                primitive["series_native_renderer"] = "tdxw-sub_95AE00";
                primitive["series_native_pen_style"] = "PS_SOLID";
                primitive["series_native_point_geometry_rule"] =
                    "spacing<5?diagonal-radius-1:spacing<10?diagonal-radius-2:diagonal-radius-3";
            } else if (mode == "point-dot") {
                primitive["series_native_render_type"] = 8;
                primitive["series_native_renderer"] = "tdxw-sub_95B0A0";
                primitive["series_native_pen_style"] = "PS_SOLID";
                primitive["series_native_point_geometry_rule"] =
                    "width<2?one-pixel:filled-ellipse-diameter-width";
            } else {
                primitive["series_native_render_type"] = mode == "dot-line" ? 9 : 0;
                primitive["series_native_renderer"] = "tdxw-sub_957620";
                primitive["series_native_pen_style"] = mode == "dot-line"
                    ? "PS_DOT" : "PS_SOLID";
                primitive["series_native_missing_value_rule"] =
                    "break-contiguous-run";
                primitive["series_native_single_point_rule"] =
                    "x-minus-3-to-x-horizontal";
                primitive["series_native_bold_width_config_key"] =
                    "Other/BoldZBLine";
                primitive["series_native_pen_width_rule"] =
                    "width==1&&BoldZBLine?2:width;skip-if-width<1";
            }
        }
        if (color_stick || volume_stick) {
            primitive["series_stick_mode"] = color_stick
                ? "color-stick" : "volume-stick";
            primitive["series_stick_native_render_type"] = color_stick ? 2 : 1;
            primitive["series_stick_native_renderer"] = color_stick
                ? "tdxw-sub_9555B0" : "tdxw-sub_957030";
            primitive["series_stick_baseline"] = 0;
            primitive["series_stick_coordinate_space"] = "bar-zero-baseline";
            primitive["series_stick_value_float_cast"] = true;
            primitive["series_stick_epsilon"] =
                static_cast<double>(native_stick_color_epsilon);
            primitive["series_stick_width_rule"] =
                "spacing>=3?spacing-max(spacing*0.25,2):min(spacing,1);half=floor(body*0.5);pixels=2*half+1";
            if (color_stick) {
                primitive["series_stick_shape"] = "one-pixel-vertical-line";
                primitive["series_stick_color_rule"] =
                    "float(value)*10000>epsilon?up:<epsilon?down:none";
            } else {
                primitive["series_stick_shape"] = "volume-body";
                primitive["series_stick_color_rule"] =
                    "VolKUseZT?previous-close:open-close-with-flat-previous-close-fallback";
                primitive["series_stick_up_fill_rule"] = "RealUPK?solid:hollow";
                primitive["series_stick_down_fill"] = "solid";
                primitive["series_stick_real_up_k_config_key"] = "Other/RealUPK";
                primitive["series_stick_vol_k_use_zt_config_key"] =
                    "Other/VolKUseZT";
            }
            Json events = Json::array();
            for (std::size_t index = 0; index < found->second.size(); ++index) {
                const double value = found->second[index];
                if (!std::isfinite(value)) continue;
                Json event = Json::object();
                event["index"] = static_cast<std::uint64_t>(index);
                Json arguments = Json::array();
                arguments.push_back(value);
                event["arguments"] = std::move(arguments);
                event["series_stick_value"] = value;
                if (color_stick) {
                    event["series_stick_color_role"] =
                        native_colorstick_role(value);
                } else {
                    event["series_stick_open_close_color_role"] =
                        native_volstick_open_close_up(bars, index) ? "up" : "down";
                    event["series_stick_previous_close_color_role"] =
                        native_volstick_previous_close_up(bars, index) ? "up" : "down";
                }
                events.push_back(std::move(event));
            }
            primitive["event_count"] = static_cast<std::uint64_t>(events.size());
            primitive["events"] = std::move(events);
        }
    return true;
}

}  // namespace tdx::formula_render_detail
