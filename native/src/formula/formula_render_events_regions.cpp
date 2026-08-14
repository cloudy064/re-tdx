#include "formula_render_internal.hpp"
#include "formula_native_constants.hpp"

#include "formula_engine_support_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace tdx::formula_render_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;
using namespace formula_language_detail;
using namespace formula_runtime_detail;

namespace {

// DRAWGBK_DIV keeps its own condition and rounding rules rather than reusing
// the shared drawnumber_dif_native_true / native_biased_int helpers: the vendor
// applies a wider epsilon here and a different rounding bias.
bool native_true(double value) {
    return std::isfinite(value) && std::abs(value - 1.0) < 0.0001;
}

int native_integer(double value) {
    return static_cast<int>(value + tdx::formula_engine_detail::tcalc_constants::integer_bias);
}

}  // namespace

// Emits one event per bar of each contiguous run of true condition bars, with
// the region-wide fields (mode, range, price extrema) resolved once from the
// leading bar and repeated on every member event.
void render_region_background_events(
    const PrimitiveRenderContext& context, Json& events) {
    const auto& bars = context.bars;
    const auto& numeric_arguments = context.numeric_arguments;
    if (numeric_arguments.size() != 5) return;
    std::size_t index = 0;
    while (index < bars.size()) {
        if (!native_true(numeric_arguments[0][index])) {
            ++index;
            continue;
        }
        const std::size_t region_start = index;
        while (index < bars.size() && native_true(numeric_arguments[0][index]))
            ++index;
        const std::size_t region_end = index;
        const bool mode_available =
            std::isfinite(numeric_arguments[3][region_start]);
        const bool range_available =
            std::isfinite(numeric_arguments[4][region_start]);
        const int mode = mode_available
            ? native_integer(numeric_arguments[3][region_start]) : -1;
        const int range = range_available
            ? native_integer(numeric_arguments[4][region_start]) : -1;

        double price_top = std::numeric_limits<double>::quiet_NaN();
        double price_bottom = std::numeric_limits<double>::quiet_NaN();
        std::string price_aggregation = "whole-pane";
        if (range == 1) {
            price_top = bars[region_start].high;
            price_bottom = bars[region_start].low;
            for (std::size_t bar_index = region_start + 1;
                 bar_index < region_end; ++bar_index) {
                price_top = std::max(price_top, bars[bar_index].high);
                price_bottom = std::min(price_bottom, bars[bar_index].low);
            }
            price_aggregation = "region-high-low-extrema";
        } else if (range == 2) {
            price_top = std::max(bars[region_start].open,
                                 bars[region_end - 1].close);
            price_bottom = std::min(bars[region_start].open,
                                    bars[region_end - 1].close);
            price_aggregation = "region-first-open-last-close";
        } else if (range != 0) {
            price_aggregation = "vendor-defined";
        }

        const std::string mode_name = mode == 0 ? "vertical-gradient" :
            mode == 1 ? "horizontal-gradient" : mode == 2 ? "border" :
            mode == 3 ? "border-and-fill" :
            mode >= 10 && mode <= 20 ? "alpha-solid" : "vendor-defined";
        const std::string compositing = mode == 0 || mode == 1
            ? "opaque-gdi-gradient-or-solid" : mode == 2
            ? "border-only" : mode == 3
            ? "opaque-color2-fill-color1-border" :
            mode >= 10 && mode <= 20
            ? "gdiplus-argb-solid-color1" : "unsupported";
        const bool has_fill = mode == 0 || mode == 1 || mode == 3 ||
                              (mode >= 10 && mode <= 20);
        const int alpha_byte = mode >= 10 && mode <= 20
            ? 255 * (mode - 10) / 10 : 255;

        for (std::size_t event_index = region_start;
             event_index < region_end; ++event_index) {
            Json event = Json::object();
            event["index"] = static_cast<std::uint64_t>(event_index);
            event["arguments"] =
                numeric_arguments_at(numeric_arguments, event_index);
            const bool first_available =
                std::isfinite(numeric_arguments[1][region_start]);
            const bool second_available =
                std::isfinite(numeric_arguments[2][region_start]);
            event["background_color1_available"] = first_available;
            event["background_color2_available"] = second_available;
            event["background_color1_ref"] = first_available
                ? Json(numeric_arguments[1][region_start]) : Json(nullptr);
            event["background_color2_ref"] = second_available
                ? Json(numeric_arguments[2][region_start]) : Json(nullptr);
            event["background_fill_mode"] =
                mode_available ? Json(mode) : Json(nullptr);
            event["background_fill_mode_name"] = mode_name;
            event["background_fill_compositing"] = compositing;
            event["background_fill_alpha_byte"] =
                has_fill ? Json(alpha_byte) : Json(nullptr);
            event["background_fill_alpha_denominator"] = 255;
            event["background_range"] =
                range_available ? Json(range) : Json(nullptr);
            event["background_range_name"] = range == 0 ? "pane" :
                range == 1 ? "bar-high-low" :
                range == 2 ? "bar-open-close" : "vendor-defined";
            event["background_region_leader"] =
                event_index == region_start;
            event["background_region_start_index"] =
                static_cast<std::uint64_t>(region_start);
            event["background_region_end_index"] =
                static_cast<std::uint64_t>(region_end - 1);
            event["background_region_end_index_exclusive"] =
                static_cast<std::uint64_t>(region_end);
            event["background_price_aggregation"] = price_aggregation;
            if (range == 1 || range == 2) {
                event["background_price_top"] = price_top;
                event["background_price_bottom"] = price_bottom;
            } else {
                event["background_price_top"] = Json(nullptr);
                event["background_price_bottom"] = Json(nullptr);
            }
            events.push_back(std::move(event));
        }
    }
}

}  // namespace tdx::formula_render_detail
