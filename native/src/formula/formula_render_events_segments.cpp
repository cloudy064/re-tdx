#include "formula_render_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_runtime_detail;

void render_polyline_events(const PrimitiveRenderContext& context, Json& events) {
    const auto& bars = context.bars;
    const auto& numeric_arguments = context.numeric_arguments;
    if (numeric_arguments.size() != 2) return;
    std::size_t previous = bars.size();
    for (std::size_t index = 0; index < bars.size(); ++index) {
        if (!truth(numeric_arguments[0][index]) ||
            !std::isfinite(numeric_arguments[1][index])) continue;
        Json event = Json::object();
        event["index"] = static_cast<std::uint64_t>(index);
        event["arguments"] = numeric_arguments_at(numeric_arguments, index);
        event["polyline_vertex_index"] = static_cast<std::uint64_t>(index);
        event["polyline_vertex_price"] = numeric_arguments[1][index];
        if (previous == bars.size()) {
            event["segment_from_index"] = Json(nullptr);
            event["segment_from_price"] = Json(nullptr);
        } else {
            event["segment_from_index"] = static_cast<std::uint64_t>(previous);
            event["segment_from_price"] = numeric_arguments[1][previous];
        }
        event["segment_to_index"] = static_cast<std::uint64_t>(index);
        event["segment_to_price"] = numeric_arguments[1][index];
        event["segment_expansion"] = "none";
        events.push_back(std::move(event));
        previous = index;
    }
}

void render_draw_line_events(const PrimitiveRenderContext& context, Json& events) {
    const auto& bars = context.bars;
    const auto& numeric_arguments = context.numeric_arguments;
    if (numeric_arguments.size() != 5) return;
    std::size_t start = bars.size();
    for (std::size_t index = 0; index < bars.size(); ++index) {
        if (truth(numeric_arguments[0][index]) &&
            std::isfinite(numeric_arguments[1][index]))
            start = index;
        if (start == bars.size() || !truth(numeric_arguments[2][index]) ||
            !std::isfinite(numeric_arguments[3][index])) continue;
        const auto width = index - start;
        const double first = numeric_arguments[1][start];
        const double last = numeric_arguments[3][index];
        const double slope = width
            ? (last - first) / static_cast<double>(width) : 0.0;
        const bool expanded = truth(numeric_arguments[4][index]);
        const std::size_t render_end = expanded && !bars.empty()
            ? bars.size() - 1 : index;
        const double render_price = last + slope *
            static_cast<double>(render_end - index);
        Json event = Json::object();
        event["index"] = static_cast<std::uint64_t>(index);
        event["arguments"] = numeric_arguments_at(numeric_arguments, index);
        event["segment_from_index"] = static_cast<std::uint64_t>(start);
        event["segment_from_price"] = first;
        event["segment_to_index"] = static_cast<std::uint64_t>(render_end);
        event["segment_to_price"] = render_price;
        event["segment_anchor_to_index"] = static_cast<std::uint64_t>(index);
        event["segment_anchor_to_price"] = last;
        event["segment_slope_per_bar"] = slope;
        event["segment_expansion"] = expanded ? "right" : "none";
        events.push_back(std::move(event));
        start = bars.size();
    }
}

void render_slope_line_events(const PrimitiveRenderContext& context, Json& events) {
    const auto& bars = context.bars;
    const auto& numeric_arguments = context.numeric_arguments;
    if (numeric_arguments.size() != 5 || bars.empty()) return;
    const double raw_direction = numeric_arguments[4].back();
    const bool direction_available = std::isfinite(raw_direction) &&
        raw_direction >= static_cast<double>(std::numeric_limits<int>::min()) &&
        raw_direction <= static_cast<double>(std::numeric_limits<int>::max());
    const int direction = direction_available
        ? static_cast<int>(raw_direction) : -1;
    for (std::size_t index = 0; index < bars.size(); ++index) {
        const double condition = numeric_arguments[0][index];
        const double price = numeric_arguments[1][index];
        const double slope = numeric_arguments[2][index];
        const double length = numeric_arguments[3][index];
        if (!std::isfinite(condition) || std::abs(condition) < 0.00001 ||
            !std::isfinite(price) || !std::isfinite(slope) ||
            !std::isfinite(length) || direction < 0 || direction > 2)
            continue;

        const auto append = [&](bool right) {
            Json event = Json::object();
            event["index"] = static_cast<std::uint64_t>(index);
            event["arguments"] = numeric_arguments_at(numeric_arguments, index);
            event["segment_from_index"] = static_cast<std::uint64_t>(index);
            event["segment_from_price"] = price;
            event["slope_per_bar"] = slope;
            event["slope_length"] = length;
            event["slope_direction_value"] = direction;
            event["slope_direction"] = right ? "right" : "left";
            const bool vertical = slope == 10000.0;
            event["slope_vertical"] = vertical;
            if (vertical) {
                event["segment_to_index"] = static_cast<std::uint64_t>(index);
                event["segment_to_price"] = price;
                event["slope_vertical_pixel_delta"] = right ? -length : length;
            } else if (right) {
                const double bound_value = static_cast<double>(index) + length + 1.0;
                if (bound_value < static_cast<double>(std::numeric_limits<int>::min()) ||
                    bound_value > static_cast<double>(std::numeric_limits<int>::max()))
                    return;
                const int bound = std::min<int>(
                    static_cast<int>(bars.size()), static_cast<int>(bound_value));
                if (bound <= static_cast<int>(index)) return;
                const std::size_t target = static_cast<std::size_t>(bound - 1);
                event["segment_to_index"] = static_cast<std::uint64_t>(target);
                event["segment_to_price"] = price +
                    static_cast<double>(target - index) * slope;
            } else {
                const double bound_value = static_cast<double>(index) - length;
                if (bound_value < static_cast<double>(std::numeric_limits<int>::min()) ||
                    bound_value > static_cast<double>(std::numeric_limits<int>::max()))
                    return;
                const int bound = std::max(0, static_cast<int>(bound_value));
                if (bound > static_cast<int>(index)) return;
                const std::size_t target = static_cast<std::size_t>(bound);
                event["segment_to_index"] = static_cast<std::uint64_t>(target);
                event["segment_to_price"] = price -
                    static_cast<double>(index - target) * slope;
            }
            events.push_back(std::move(event));
        };
        if (direction == 0 || direction == 2) append(true);
        if (direction == 1 || direction == 2) append(false);
    }
}

}  // namespace tdx::formula_render_detail
