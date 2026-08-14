#include "formula_render_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_runtime_detail;

void apply_stick_event_fields(
    Json& event, const PrimitiveRenderContext& context, std::size_t index) {
    const auto& numeric_arguments = context.numeric_arguments;
    const double width = numeric_arguments[3][index];
    const double empty = numeric_arguments[4][index];
    const bool width_available = std::isfinite(width);
    const bool empty_available = std::isfinite(empty);
    event["stick_width_available"] = width_available;
    event["stick_width"] = width_available ? Json(width) : Json(nullptr);
    event["stick_width_ratio"] = width_available && width > 0.0
        ? Json(width / 4.0) : Json(0.0);
    event["stick_hairline"] = width_available && width <= 0.0;
    if (empty_available) {
        const auto mode = stickline_mode(empty);
        const bool centered = mode == "center-full" || mode == "center-half";
        event["stick_mode"] = mode;
        event["stick_hollow"] = mode == "dashed-hollow" ||
                                  mode == "solid-hollow";
        event["stick_border_dashed"] = mode == "dashed-hollow";
        event["stick_price1_used"] = !centered;
        event["stick_anchor"] = centered ? "pane-middle" : "price-pair";
        event["stick_occupancy"] = mode == "center-full" ? "full" :
                                    mode == "center-half" ? "half" :
                                    "specified";
    } else {
        event["stick_mode"] = Json(nullptr);
        event["stick_hollow"] = false;
        event["stick_border_dashed"] = false;
        event["stick_price1_used"] = true;
        event["stick_anchor"] = "price-pair";
        event["stick_occupancy"] = "specified";
    }
}

void apply_icon_event_fields(
    Json& event, const PrimitiveRenderContext& context, std::size_t index) {
    const auto& statement = context.statement;
    const auto& numeric_arguments = context.numeric_arguments;
    const auto price = numeric_arguments[1][index];
    const auto raw_type = numeric_arguments[2][index];
    const bool integer_type = std::isfinite(raw_type) &&
        std::floor(raw_type) == raw_type && raw_type >= 1.0 &&
        raw_type <= 4294967295.0;
    const auto icon_type = integer_type
        ? static_cast<std::uint32_t>(raw_type) : 0U;
    const bool official = integer_type && icon_type <= 51;
    const bool sprite_available = integer_type && icon_type <= 100;
    event["icon_price"] = std::isfinite(price) ? Json(price) : Json(nullptr);
    event["icon_type"] = integer_type
        ? Json(static_cast<std::uint64_t>(icon_type)) : Json(nullptr);
    event["icon_type_available"] = official;
    event["icon_sprite_cell_available"] = sprite_available;
    event["icon_sprite_x"] = sprite_available
        ? Json(static_cast<std::uint64_t>((icon_type - 1) * 18U))
        : Json(nullptr);
    event["icon_vertical_align"] = directive_present(statement, "DRAWABOVE")
        ? "above-price" : "below-price";
}

// Handles the four text/number annotations.  Called unconditionally by the
// shared loop, so it re-tests the primitive itself and returns without
// touching the event for every other primitive.
void apply_annotation_event_fields(
    Json& event, const PrimitiveRenderContext& context, std::size_t index) {
    const auto& function = context.function;
    const auto& statement = context.statement;
    const auto& number_format = context.number_format;
    const auto& numeric_arguments = context.numeric_arguments;
    const auto& string_arguments = context.string_arguments;
    const auto& string_available = context.string_available;
    const bool price_text = function == "DRAWTEXT" || function == "DRAWNUMBER";
    const bool fixed_text = function == "DRAWTEXT_FIX" ||
                            function == "DRAWNUMBER_FIX";
    if (!price_text && !fixed_text) return;
    const std::size_t text_argument = fixed_text ? 4 : 2;
    std::string label;
    bool label_available = false;
    if (function == "DRAWTEXT" || function == "DRAWTEXT_FIX") {
        if (text_argument < string_arguments.size() &&
            string_available[text_argument] &&
            index < string_arguments[text_argument].size()) {
            label = annotation_text_limit(string_arguments[text_argument][index]);
            label_available = true;
        }
    } else if (text_argument < numeric_arguments.size() &&
               std::isfinite(numeric_arguments[text_argument][index])) {
        label = annotation_number_label(
            numeric_arguments[text_argument][index], number_format);
        label_available = true;
    }
    event["annotation_text_available"] = label_available;
    if (label_available) {
        event["annotation_text"] = label;
        event["annotation_lines"] = annotation_lines(label);
    } else {
        event["annotation_text"] = Json(nullptr);
        event["annotation_lines"] = Json::array();
    }
    event["annotation_frame"] = function == "DRAWTEXT" &&
        directive_present(statement, "DRAWCFRAME");
    if (price_text) {
        const bool price_available = numeric_arguments.size() > 1 &&
            std::isfinite(numeric_arguments[1][index]);
        event["annotation_price"] = price_available
            ? Json(numeric_arguments[1][index]) : Json(nullptr);
        event["annotation_horizontal_align"] = "left";
        const bool draw_above = directive_present(statement, "DRAWABOVE");
        event["annotation_drawabove"] = draw_above;
        event["annotation_vertical_align"] = draw_above
            ? "above-price" : "price-origin";
        event["annotation_x_offset_pixels"] =
            function == "DRAWNUMBER" ? -3 : 0;
        event["annotation_y_offset_pixels"] =
            function == "DRAWTEXT" ? -8 : 0;
        event["annotation_line_count"] = label_available
            ? static_cast<std::uint64_t>(annotation_lines(label).size()) : 0;
        event["annotation_drawabove_row_adjustment_pixels"] = -2;
    } else {
        const bool coordinates_available = numeric_arguments.size() > 3 &&
            std::isfinite(numeric_arguments[1][index]) &&
            std::isfinite(numeric_arguments[2][index]) &&
            std::isfinite(numeric_arguments[3][index]);
        event["annotation_x"] = coordinates_available
            ? Json(numeric_arguments[1][index]) : Json(nullptr);
        event["annotation_y"] = coordinates_available
            ? Json(numeric_arguments[2][index]) : Json(nullptr);
        event["annotation_horizontal_align"] = coordinates_available &&
            static_cast<int>(numeric_arguments[3][index]) == 1
                ? "right" : "left";
        event["annotation_vertical_align"] = "top";
    }
}

}  // namespace tdx::formula_render_detail
