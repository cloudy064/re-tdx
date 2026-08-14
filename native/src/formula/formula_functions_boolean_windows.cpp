#include "formula_function_dispatch_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace tdx::formula_engine_detail {
namespace {

constexpr float tcalc_missing_sentinel = -4.0398103e34F;
constexpr float tcalc_boolean_epsilon = 0.0000099999997F;

float native_series_value(double value) {
    return std::isfinite(value)
        ? static_cast<float>(value) : tcalc_missing_sentinel;
}

std::size_t first_valid(const Series& input) {
    std::size_t first = 0;
    while (first < input.size() && !std::isfinite(input[first])) ++first;
    return first;
}

std::int64_t signed_i32(std::uint32_t bits) {
    return bits <= static_cast<std::uint32_t>(
                       std::numeric_limits<std::int32_t>::max())
        ? static_cast<std::int64_t>(bits)
        : static_cast<std::int64_t>(bits) - 0x100000000LL;
}

}  // namespace

Series tcalc_count(const Series& input, const Series& periods) {
    Series out(input.size(), 0.0);
    if (input.empty() || periods.empty()) return out;

    // TCalc!sub_10017650 first decides whether N is effectively constant
    // from the first valid source through the final bar.  Constant N near
    // zero means all available bars, while a dynamic zero means an empty
    // window; materially negative values select all available bars.
    const auto first = first_valid(input);
    bool constant_period = true;
    const float final_period = native_series_value(periods.back());
    if (first + 1 < input.size()) {
        for (std::size_t index = first; index + 1 < input.size(); ++index) {
            const float difference = static_cast<float>(
                native_series_value(periods[index]) - final_period);
            if (std::fabs(difference) > tcalc_boolean_epsilon) {
                constant_period = false;
                break;
            }
        }
    }

    const float total_bars = static_cast<float>(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        const float native_period = native_series_value(periods[index]);
        const float selected_period = constant_period
            ? (native_period >= tcalc_boolean_epsilon
                   ? native_period : total_bars)
            : (native_period >= -tcalc_boolean_epsilon
                   ? native_period : total_bars);
        int window = tcalc_external_integer(selected_period);
        const int available = static_cast<int>(index + 1);
        if (window >= available) window = available;

        int count = 0;
        for (int offset = 0; offset < window; ++offset) {
            const auto at = index - static_cast<std::size_t>(offset);
            const float difference = static_cast<float>(
                native_series_value(input[at]) - 1.0F);
            if (std::fabs(difference) < tcalc_boolean_epsilon) ++count;
        }
        out[index] = static_cast<double>(count);
    }
    return out;
}

Series tcalc_every(const Series& input, const Series& periods) {
    Series out(input.size(), missing);
    if (input.empty() || periods.empty()) return out;

    const auto first = first_valid(input);
    int consecutive = 0;
    for (std::size_t index = first; index < input.size(); ++index) {
        // A missing or sub-one N emits zero without changing the retained
        // run length.  Once X has started, its missing sentinel is a large
        // non-zero float and therefore counts as true in the native loop.
        if (!std::isfinite(periods[index])) {
            out[index] = 0.0;
            continue;
        }
        const int period = tcalc_external_integer(periods[index]);
        if (period < 1) {
            out[index] = 0.0;
            continue;
        }
        const float source = native_series_value(input[index]);
        if (source >= tcalc_boolean_epsilon ||
            source <= -tcalc_boolean_epsilon) {
            ++consecutive;
        } else {
            consecutive = 0;
        }
        out[index] = consecutive < period ? 0.0 : 1.0;
    }
    return out;
}

Series tcalc_exist(const Series& input, const Series& periods) {
    Series out(input.size(), missing);
    if (input.empty() || periods.empty()) return out;

    // TCalc!sub_100167C0 reads N only from the final bar.  Keep the native
    // signed 32-bit wrap of -N and i-N explicit so hostile numeric inputs
    // remain deterministic in C++.
    const int period = tcalc_external_integer(periods.back());
    std::int64_t last_true = signed_i32(
        0U - static_cast<std::uint32_t>(period));
    const auto first = first_valid(input);
    for (std::size_t index = first; index < input.size(); ++index) {
        if (!std::isfinite(input[index])) continue;
        const float source = static_cast<float>(input[index]);
        if (std::fabs(source) > tcalc_boolean_epsilon)
            last_true = signed_i32(static_cast<std::uint32_t>(index));
        const auto threshold = signed_i32(
            static_cast<std::uint32_t>(index) -
            static_cast<std::uint32_t>(period));
        out[index] = last_true <= threshold ? 0.0 : 1.0;
    }
    return out;
}

Series tcalc_bars_last(const Series& input) {
    Series out(input.size(), missing);
    std::size_t first = 0;
    while (first < input.size()) {
        const float source = native_series_value(input[first]);
        if (source != tcalc_missing_sentinel && source != 0.0F)
            break;
        ++first;
    }

    int bars = 0;
    for (std::size_t index = first; index < input.size(); ++index) {
        // TCalc!sub_100069C0 only checks sentinel/zero while finding the
        // first signal.  An internal sentinel is non-zero and resets the
        // distance just like any later native true value.
        const float source = native_series_value(input[index]);
        const int value = source == 0.0F ? bars : 0;
        out[index] = static_cast<double>(value);
        bars = value + 1;
    }
    return out;
}

Series tcalc_bars_last_count(const Series& input) {
    Series out(input.size(), missing);
    std::size_t first = 0;
    while (first < input.size() &&
           native_series_value(input[first]) == tcalc_missing_sentinel)
        ++first;
    for (std::size_t index = first; index < input.size(); ++index) {
        int count = 0;
        for (std::size_t cursor = index + 1; cursor-- > first;) {
            if (!std::isfinite(input[cursor])) continue;
            const float source = static_cast<float>(input[cursor]);
            if (std::fabs(static_cast<float>(source - 1.0F)) <
                tcalc_boolean_epsilon)
                ++count;
            if (std::fabs(source) < tcalc_boolean_epsilon) break;
        }
        out[index] = static_cast<double>(count);
    }
    return out;
}

}  // namespace tdx::formula_engine_detail
