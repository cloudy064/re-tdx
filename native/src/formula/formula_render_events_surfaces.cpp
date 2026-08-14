#include "formula_render_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_runtime_detail;

void render_bitmap_events(const PrimitiveRenderContext& context, Json& events) {
    const auto& bars = context.bars;
    const auto& numeric_arguments = context.numeric_arguments;
    const auto& string_arguments = context.string_arguments;
    const auto& string_available = context.string_available;
    if (numeric_arguments.size() != 3 || string_arguments.size() != 3 ||
        !string_available[2])
        return;
    for (std::size_t index = 0; index < bars.size(); ++index) {
        if (!drawnumber_dif_native_true(numeric_arguments[0][index]) ||
            !std::isfinite(numeric_arguments[1][index]) ||
            index >= string_arguments[2].size() ||
            string_arguments[2][index].empty())
            continue;
        Json event = Json::object();
        event["index"] = static_cast<std::uint64_t>(index);
        event["arguments"] = numeric_arguments_at(numeric_arguments, index);
        event["bitmap_price"] = numeric_arguments[1][index];
        event["bitmap_name"] = string_arguments[2][index];
        event["bitmap_format"] = "bmp";
        events.push_back(std::move(event));
    }
}

void render_pane_background_events(
    const PrimitiveRenderContext& context, Json& events) {
    const auto& bars = context.bars;
    const auto& numeric_arguments = context.numeric_arguments;
    const auto& string_arguments = context.string_arguments;
    const auto& string_available = context.string_available;
    if (numeric_arguments.size() != 6 || bars.empty()) return;
    std::size_t trigger = bars.size();
    for (std::size_t index = 0; index < bars.size(); ++index) {
        if (drawnumber_dif_native_true(numeric_arguments[0][index])) {
            trigger = index;
            break;
        }
    }
    if (trigger == bars.size()) return;
    Json event = Json::object();
    event["index"] = static_cast<std::uint64_t>(trigger);
    event["arguments"] = numeric_arguments_at(numeric_arguments, trigger);
    int color1 = 0, color2 = 0, horizontal = 0, stretch = 0;
    const bool color1_available = native_biased_int(
        numeric_arguments[1][0], color1);
    const bool color2_available = native_biased_int(
        numeric_arguments[2][0], color2);
    (void)native_biased_int(numeric_arguments[3][0], horizontal);
    (void)native_biased_int(numeric_arguments[5][0], stretch);
    const bool gradient = color1 != 0 || color2 != 0;
    event["pane_background_mode"] = gradient ? "gradient" : "image";
    event["pane_background_color1_available"] = color1_available;
    event["pane_background_color2_available"] = color2_available;
    event["pane_background_color1_ref"] = color1_available
        ? Json(color1) : Json(nullptr);
    event["pane_background_color2_ref"] = color2_available
        ? Json(color2) : Json(nullptr);
    event["pane_background_horizontal"] = horizontal != 0;
    event["pane_background_stretch"] = stretch != 0;
    if (!gradient && string_arguments.size() == 6 && string_available[4] &&
        trigger < string_arguments[4].size() &&
        !string_arguments[4][trigger].empty()) {
        event["pane_background_name_available"] = true;
        event["pane_background_name"] = string_arguments[4][trigger];
        event["pane_background_format"] = "auto";
    } else {
        event["pane_background_name_available"] = false;
        event["pane_background_name"] = Json(nullptr);
        event["pane_background_format"] = Json(nullptr);
    }
    events.push_back(std::move(event));
}

void render_pane_rectangle_events(
    const PrimitiveRenderContext& context, Json& events) {
    const auto& statement = context.statement;
    const auto& bars = context.bars;
    const auto& numeric_arguments = context.numeric_arguments;
    if (numeric_arguments.size() != 5 || bars.empty()) return;
    std::array<int, 5> values{};
    bool values_available = true;
    for (std::size_t index = 0; index < values.size(); ++index)
        values_available = native_biased_int(
            numeric_arguments[index][0], values[index]) && values_available;
    if (!values_available) return;
    Json event = Json::object();
    event["index"] = 0;
    event["arguments"] = numeric_arguments_at(numeric_arguments, 0);
    event["rectangle_left"] = values[0];
    event["rectangle_top"] = values[1];
    event["rectangle_right"] = values[2];
    event["rectangle_bottom"] = values[3];
    event["rectangle_color_ref"] = values[4];
    event["rectangle_fill"] = values[4] != 0;
    event["rectangle_frame"] = !directive_present(statement, "NOFRAME");
    events.push_back(std::move(event));
}

}  // namespace tdx::formula_render_detail
