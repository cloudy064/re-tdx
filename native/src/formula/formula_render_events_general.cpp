#include "formula_render_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_runtime_detail;

// Shared per-bar loop for the primitives without a dedicated renderer.  Gating
// differs by primitive, so the guards below stay inline; the per-primitive
// fields are appended by the helpers in the order the original chain used,
// because that order is the field order of the emitted JSON object.
void render_general_events(const PrimitiveRenderContext& context, Json& events) {
    const auto& function = context.function;
    const auto& bars = context.bars;
    const auto& numeric_arguments = context.numeric_arguments;
    const auto& string_arguments = context.string_arguments;
    const auto& string_available = context.string_available;
    const bool part_line = context.part_line;
    for (std::size_t index = 0; index < bars.size(); ++index) {
        const bool conditional = function != "DRAWKLINE" && function != "DRAWBAND" &&
                                 function != "PARTLINE";
        const bool native_annotation = function == "DRAWTEXT" ||
            function == "DRAWNUMBER" || function == "DRAWTEXT_FIX" ||
            function == "DRAWNUMBER_FIX";
        if (conditional && (numeric_arguments.empty() ||
                (native_annotation
                    ? !drawnumber_dif_native_true(numeric_arguments[0][index])
                    : !truth(numeric_arguments[0][index])))) continue;
        const bool price_annotation = function == "DRAWTEXT" ||
                                       function == "DRAWNUMBER";
        if (price_annotation && (numeric_arguments.size() <= 1 ||
                                 !std::isfinite(numeric_arguments[1][index])))
            continue;
        if (part_line && (numeric_arguments.empty() ||
                          !std::isfinite(numeric_arguments[0][index]))) continue;
        if (function == "DRAWKLINE" &&
            (numeric_arguments.size() != 4 ||
             !std::all_of(numeric_arguments.begin(), numeric_arguments.end(),
                          [&](const Series& value) { return std::isfinite(value[index]); })))
            continue;
        Json event = Json::object();
        event["index"] = static_cast<std::uint64_t>(index);
        event["arguments"] = numeric_arguments_at(numeric_arguments, index);
        if (part_line) {
            const bool color_available = numeric_arguments.size() > 1 &&
                std::isfinite(numeric_arguments[1][index]);
            const int direction = numeric_arguments.size() > 2 &&
                std::isfinite(numeric_arguments[2][index])
                ? static_cast<int>(numeric_arguments[2][index]) : 0;
            event["segment_color_available"] = color_available;
            event["segment_direction"] = direction == 1
                ? "previous-to-current" : "current-to-next";
            if (direction == 1) {
                event["segment_from_index"] = index > 0
                    ? Json(static_cast<std::uint64_t>(index - 1)) : Json(nullptr);
                event["segment_to_index"] = static_cast<std::uint64_t>(index);
            } else {
                event["segment_from_index"] = static_cast<std::uint64_t>(index);
                event["segment_to_index"] = index + 1 < bars.size()
                    ? Json(static_cast<std::uint64_t>(index + 1)) : Json(nullptr);
            }
        }
        if (function == "DRAWBAND" && numeric_arguments.size() == 4) {
            const bool first_above = numeric_arguments[0][index] >
                numeric_arguments[2][index];
            const std::size_t color_argument = first_above ? 1 : 3;
            const bool color_available =
                std::isfinite(numeric_arguments[color_argument][index]);
            event["band_side"] = first_above ? "arg0-above" : "arg2-above";
            event["fill_color_argument"] =
                static_cast<std::uint64_t>(color_argument);
            event["fill_color_available"] = color_available;
            event["fill_color_ref"] = color_available
                ? Json(numeric_arguments[color_argument][index]) : Json(nullptr);
        }
        if (function == "STICKLINE" && numeric_arguments.size() >= 5)
            apply_stick_event_fields(event, context, index);
        if (function == "DRAWICON" && numeric_arguments.size() >= 3)
            apply_icon_event_fields(event, context, index);
        apply_annotation_event_fields(event, context, index);
        const auto text = string_arguments_at(string_arguments, string_available, index);
        if (text.size()) event["string_arguments"] = text;
        events.push_back(std::move(event));
    }
}

}  // namespace tdx::formula_render_detail
