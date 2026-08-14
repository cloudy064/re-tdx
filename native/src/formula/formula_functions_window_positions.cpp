#include "formula_function_dispatch_internal.hpp"
#include "formula_native_constants.hpp"

#include <cmath>
#include <cstddef>

namespace tdx::formula_engine_detail {
namespace {

constexpr float tcalc_missing_sentinel = -4.0398103e34F;
constexpr double tcalc_absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
constexpr double tcalc_relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

float native_value(const Series& input, std::size_t index) {
    if (index >= input.size() || !std::isfinite(input[index]))
        return tcalc_missing_sentinel;
    return static_cast<float>(input[index]);
}

Series tcalc_extreme_bars(const Series& input, const Series& periods,
                          bool highest) {
    Series out(input.size(), missing);
    if (input.empty()) return out;

    // TCalc!sub_10018860/sub_10019860 skip only the leading source sentinel.
    // Once started, an internal sentinel remains an ordinary float candidate.
    std::size_t first = 0;
    while (first < input.size() &&
           native_value(input, first) == tcalc_missing_sentinel)
        ++first;

    for (std::size_t index = first; index < input.size(); ++index) {
        const float native_period = native_value(periods, index);
        if (native_period == tcalc_missing_sentinel) continue;

        // N is narrowed to float and truncated to i32 per bar. Non-positive
        // and not-yet-available windows become all available history.
        int window = tcalc_external_integer(native_period);
        const int available = static_cast<int>(index + 1);
        if (window <= 0 || window > available)
            window = tcalc_external_integer(static_cast<float>(available));
        const auto begin = index + 1 - static_cast<std::size_t>(window);

        std::size_t selected_index = begin;
        float selected = native_value(input, begin);
        for (std::size_t candidate_index = begin;
             candidate_index <= index; ++candidate_index) {
            const float candidate = native_value(input, candidate_index);
            const double scaled_tolerance =
                std::fabs(static_cast<double>(candidate)) *
                tcalc_relative_epsilon;
            const bool replace = highest
                ? static_cast<double>(selected) <
                      scaled_tolerance + static_cast<double>(candidate) +
                          tcalc_absolute_epsilon
                : static_cast<double>(selected) >
                      static_cast<double>(candidate) - scaled_tolerance -
                          tcalc_absolute_epsilon;
            if (replace) {
                selected = candidate;
                selected_index = candidate_index;
            }
        }
        out[index] = static_cast<double>(
            static_cast<float>(index - selected_index));
    }
    return out;
}

bool native_filter_signal(const Series& input, std::size_t index) {
    const float value = native_value(input, index);
    return value != tcalc_missing_sentinel &&
           (static_cast<double>(value) >= tcalc_absolute_epsilon ||
            static_cast<double>(value) <= -tcalc_absolute_epsilon);
}

}  // namespace

Series tcalc_highest_value_bars(const Series& input, const Series& periods) {
    return tcalc_extreme_bars(input, periods, true);
}

Series tcalc_lowest_value_bars(const Series& input, const Series& periods) {
    return tcalc_extreme_bars(input, periods, false);
}

Series tcalc_filter(const Series& input, const Series& periods) {
    Series out(input.size(), 0.0);
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (!native_filter_signal(input, index)) continue;
        out[index] = 1.0;

        const int blocked =
            tcalc_external_integer(native_value(periods, index));
        if (blocked < 0) continue;
        const auto remaining = input.size() - index - 1;
        if (static_cast<std::size_t>(blocked) >= remaining) break;
        index += static_cast<std::size_t>(blocked);
    }
    return out;
}

Series tcalc_filterx(const Series& input, const Series& periods) {
    Series out(input.size(), 0.0);
    for (std::ptrdiff_t cursor = static_cast<std::ptrdiff_t>(input.size()) - 1;
         cursor >= 0; --cursor) {
        const auto index = static_cast<std::size_t>(cursor);
        if (!native_filter_signal(input, index)) continue;
        out[index] = 1.0;

        const int blocked = tcalc_external_integer(native_value(periods, index));
        // The reverse handler clears and skips only if all N preceding bars
        // exist. An oversized N leaves the available prefix eligible.
        if (blocked >= 0 && static_cast<std::ptrdiff_t>(blocked) <= cursor)
            cursor -= static_cast<std::ptrdiff_t>(blocked);
    }
    return out;
}

}  // namespace tdx::formula_engine_detail
