#include "formula_engine_internal.hpp"
#include "formula_native_constants.hpp"
#include "formula_function_dispatch_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/options.hpp"
#include "tdx/security_status.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

namespace tdx::formula_engine_detail {

namespace {

constexpr float tcalc_sar_missing_sentinel = -4.0398103e34F;
constexpr double tcalc_sar_relative_tolerance =
    tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
constexpr double tcalc_sar_absolute_tolerance =
    tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;

enum class TcalcSarDirection {
    rising,
    falling,
};

float tcalc_sar_operand(double value) {
    return std::isfinite(value)
        ? static_cast<float>(value) : tcalc_sar_missing_sentinel;
}

double tcalc_sar_tolerance(float value) {
    return std::abs(static_cast<double>(value)) *
               tcalc_sar_relative_tolerance +
           tcalc_sar_absolute_tolerance;
}

double tcalc_sar_safe_output(float value) {
    return value == tcalc_sar_missing_sentinel || !std::isfinite(value)
        ? missing : static_cast<double>(value);
}

int tcalc_sar_period(double value) {
    const float narrowed = tcalc_sar_operand(value);
    if (!std::isfinite(narrowed) || narrowed >= 2147483648.0F ||
        narrowed < -2147483648.0F)
        return std::numeric_limits<int>::min();
    return static_cast<int>(std::trunc(narrowed));
}

float tcalc_sar_percent(float value) {
    return static_cast<float>(static_cast<double>(value) / 100.0);
}

float tcalc_sar_acceleration(float current, float step, float maximum) {
    const double advanced = static_cast<double>(current) +
                            static_cast<double>(step) / 100.0;
    const double limit = static_cast<double>(maximum) / 100.0;
    return static_cast<float>(limit <= advanced ? limit : advanced);
}

struct TcalcSarNativeResult {
    std::vector<float> value;
    std::vector<float> turn;
};

TcalcSarNativeResult tcalc_parabolic_sar(
    const std::vector<Series>& args, const Environment& env,
    std::size_t size) {
    TcalcSarNativeResult result{
        std::vector<float>(size, tcalc_sar_missing_sentinel),
        std::vector<float>(size, tcalc_sar_missing_sentinel)};
    if (!size) return result;

    const int period = tcalc_sar_period(args[0].back());
    if (period < 1 || period >= static_cast<int>(size)) return result;

    const auto& high = env.at("HIGH");
    const auto& low = env.at("LOW");
    const auto& close = env.at("CLOSE");
    std::vector<float> native_high(size);
    std::vector<float> native_low(size);
    std::vector<float> native_close(size);
    for (std::size_t index = 0; index < size; ++index) {
        native_high[index] = tcalc_sar_operand(high[index]);
        native_low[index] = tcalc_sar_operand(low[index]);
        native_close[index] = tcalc_sar_operand(close[index]);
    }

    float seed_low = native_low[0];
    for (int index = 1; index < period; ++index) {
        const float candidate_low =
            native_low[static_cast<std::size_t>(index)];
        if (static_cast<double>(seed_low) >=
            static_cast<double>(candidate_low) +
                tcalc_sar_tolerance(candidate_low))
            seed_low = candidate_low;
    }

    const std::size_t seed_index = static_cast<std::size_t>(period - 1);
    result.value[seed_index] = seed_low;
    float extreme = native_high[0];
    const float step = tcalc_sar_operand(args[1].back());
    const float maximum = tcalc_sar_operand(args[2].back());
    float acceleration = tcalc_sar_percent(step);
    TcalcSarDirection direction = TcalcSarDirection::rising;

    for (std::size_t index = static_cast<std::size_t>(period);
         index < size; ++index) {
        const float previous = result.value[index - 1];
        const float previous_extreme = extreme;
        const float current_high = native_high[index];
        const float current_low = native_low[index];
        double projected = 0.0;
        double boundary = 0.0;

        if (direction == TcalcSarDirection::rising) {
            if (static_cast<double>(previous) >=
                static_cast<double>(current_low) +
                    tcalc_sar_tolerance(current_low)) {
                direction = TcalcSarDirection::falling;
                extreme = current_low;
                acceleration = tcalc_sar_percent(step);
                boundary = std::max(
                    static_cast<double>(native_high[index - 1]),
                    static_cast<double>(current_high));
                // The native rising-to-falling branch advances from the old
                // extreme, unlike the opposite reversal branch below.
                projected = static_cast<double>(previous_extreme) +
                    (static_cast<double>(extreme) -
                     static_cast<double>(previous_extreme)) *
                        static_cast<double>(acceleration);
                result.value[index] = static_cast<float>(
                    std::max(boundary, projected));
                continue;
            }

            if (static_cast<double>(current_high) -
                    tcalc_sar_tolerance(current_high) >=
                static_cast<double>(extreme)) {
                extreme = current_high;
                acceleration = tcalc_sar_acceleration(
                    acceleration, step, maximum);
            }
            boundary = std::min(
                static_cast<double>(native_low[index - 1]),
                static_cast<double>(current_low));
            projected = static_cast<double>(previous) +
                (static_cast<double>(extreme) -
                 static_cast<double>(previous)) *
                    static_cast<double>(acceleration);
            result.value[index] = static_cast<float>(
                std::min(boundary, projected));
            continue;
        }

        if (static_cast<double>(previous) <=
            static_cast<double>(current_high) -
                tcalc_sar_tolerance(current_high)) {
            direction = TcalcSarDirection::rising;
            extreme = current_high;
            acceleration = tcalc_sar_percent(step);
            boundary = std::min(
                static_cast<double>(native_low[index - 1]),
                static_cast<double>(current_low));
            projected = static_cast<double>(previous) +
                (static_cast<double>(extreme) -
                 static_cast<double>(previous_extreme)) *
                    static_cast<double>(acceleration);
            result.value[index] = static_cast<float>(
                std::min(boundary, projected));
            continue;
        }

        if (static_cast<double>(current_low) +
                tcalc_sar_tolerance(current_low) <=
            static_cast<double>(extreme)) {
            extreme = current_low;
            acceleration = tcalc_sar_acceleration(
                acceleration, step, maximum);
        }
        boundary = std::max(
            static_cast<double>(native_high[index - 1]),
            static_cast<double>(current_high));
        projected = static_cast<double>(previous) +
            (static_cast<double>(extreme) -
             static_cast<double>(previous)) *
                static_cast<double>(acceleration);
        result.value[index] = static_cast<float>(
            std::max(boundary, projected));
    }

    std::size_t first = 0;
    while (first < size &&
           result.value[first] == tcalc_sar_missing_sentinel)
        ++first;
    if (first == size) return result;

    const auto is_below = [&](std::size_t index) {
        const float sar = result.value[index];
        return static_cast<double>(native_close[index]) <=
               static_cast<double>(sar) - tcalc_sar_tolerance(sar);
    };
    bool previous_below = is_below(first);
    result.turn[first] = result.value[first];
    for (std::size_t index = first + 1; index < size; ++index) {
        const float sar = result.value[index];
        const double tolerance = tcalc_sar_tolerance(sar);
        const bool current_below = is_below(index);
        if (static_cast<double>(native_close[index]) >=
                static_cast<double>(sar) + tolerance &&
            previous_below) {
            result.turn[index] = 1.0F;
        } else if (static_cast<double>(native_close[index]) <=
                       static_cast<double>(sar) - tolerance &&
                   !previous_below) {
            result.turn[index] = -1.0F;
        } else {
            result.turn[index] = 0.0F;
        }
        previous_below = current_below;
    }
    return result;
}

}  // namespace

SarSeries parabolic_sar(const std::vector<Series>& args, const Environment& env,
                        std::size_t size) {
    require_arity("SAR", args, 3, 3);
    auto native = tcalc_parabolic_sar(args, env, size);
    SarSeries result{Series(size, missing), Series(size, missing)};
    for (std::size_t index = 0; index < size; ++index) {
        result.value[index] = tcalc_sar_safe_output(native.value[index]);
        result.turn[index] = tcalc_sar_safe_output(native.turn[index]);
    }
    return result;
}

struct NativeComplex {
    double real{};
    double imaginary{};
};

Series native_fftrans(const std::vector<Series>& args, std::size_t size) {
    require_arity("FFTRANS", args, 2, 2);
    Series out(size, missing);
    if (!size) return out;

    constexpr float tcalc_missing_sentinel = -4.0398103e34f;
    constexpr double parameter_bias = tdx::formula_engine_detail::tcalc_constants::integer_bias;
    constexpr double pi = 3.141592653589793238462643383279502884;
    std::vector<NativeComplex> scratch(1024);
    std::size_t cursor = 0;
    while (cursor < size && !std::isfinite(args[0][cursor])) ++cursor;

    while (cursor < size) {
        const double raw_period = args[1][cursor];
        const int requested = std::isfinite(raw_period)
            ? static_cast<int>(raw_period + parameter_bias) : 0;
        if (requested < 1 || requested > 1024) {
            ++cursor;
            continue;
        }
        const int count = std::min<int>(requested, static_cast<int>(size - cursor));
        for (int index = 0; index < count; ++index) {
            const double source = args[0][cursor + static_cast<std::size_t>(index)];
            const double value = static_cast<double>(std::isfinite(source)
                ? static_cast<float>(source) : tcalc_missing_sentinel);
            scratch[static_cast<std::size_t>(index)] = NativeComplex{value, value};
        }

        const double stages = std::log(static_cast<double>(count)) / std::log(2.0);
        int bit_count = 0;
        for (double remaining = stages; remaining > 0.0; remaining -= 1.0)
            ++bit_count;
        for (int index = 0; index < count; ++index) {
            std::uint16_t source = static_cast<std::uint16_t>(index);
            std::uint16_t reversed = 0;
            for (int bit = 0; bit < bit_count; ++bit) {
                reversed = static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(reversed * 2U) | (source & 1U));
                source = static_cast<std::uint16_t>(source >> 1U);
            }
            if (reversed > static_cast<std::uint16_t>(index))
                std::swap(scratch[static_cast<std::size_t>(index)],
                          scratch[static_cast<std::size_t>(reversed)]);
        }

        std::vector<NativeComplex> twiddles(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            const double angle = 2.0 * pi * static_cast<double>(index) /
                                 static_cast<double>(count);
            twiddles[static_cast<std::size_t>(index)] =
                NativeComplex{std::cos(angle), -std::sin(angle)};
        }
        for (int stage = 0; static_cast<double>(stage) < stages; ++stage) {
            const int half = 1 << stage;
            for (int group = 0; group < count; group += 2 * half) {
                for (int index = 0; index < half; ++index) {
                    const int twiddle_index = index * count / (2 * half);
                    const int left_index = group + index;
                    const int right_index = group + half + index;
                    if (twiddle_index < 0 || twiddle_index >= count ||
                        left_index >= count || right_index >= count)
                        break;
                    const auto right = scratch[static_cast<std::size_t>(right_index)];
                    const auto twiddle = twiddles[static_cast<std::size_t>(twiddle_index)];
                    const NativeComplex product{
                        right.real * twiddle.real - right.imaginary * twiddle.imaginary,
                        right.real * twiddle.imaginary + right.imaginary * twiddle.real};
                    const auto left = scratch[static_cast<std::size_t>(left_index)];
                    scratch[static_cast<std::size_t>(left_index)] =
                        NativeComplex{left.real + product.real,
                                      left.imaginary + product.imaginary};
                    scratch[static_cast<std::size_t>(right_index)] =
                        NativeComplex{left.real - product.real,
                                      left.imaginary - product.imaginary};
                }
            }
        }
        for (int index = 0; index < count; ++index) {
            const float value = static_cast<float>(
                scratch[static_cast<std::size_t>(index)].real);
            out[cursor + static_cast<std::size_t>(index)] =
                value == tcalc_missing_sentinel ? missing : static_cast<double>(value);
        }
        cursor += static_cast<std::size_t>(count);
    }
    return out;
}

std::pair<float, float> newsar_window(const Series& high, const Series& low,
                                      std::size_t end, int period) {
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    const std::size_t begin = end + 1 > static_cast<std::size_t>(period)
        ? end + 1 - static_cast<std::size_t>(period) : 0;
    float lowest = static_cast<float>(low[begin]);
    float highest = static_cast<float>(high[begin]);
    for (std::size_t index = begin + 1; index <= end; ++index) {
        const float current_high = static_cast<float>(high[index]);
        const double high_tolerance = std::abs(current_high) * relative_epsilon +
                                      native_epsilon;
        if (highest <= static_cast<double>(current_high) - high_tolerance)
            highest = current_high;
        const float current_low = static_cast<float>(low[index]);
        const double low_tolerance = std::abs(current_low) * relative_epsilon +
                                     native_epsilon;
        if (lowest >= static_cast<double>(current_low) + low_tolerance)
            lowest = current_low;
    }
    return {lowest, highest};
}

Series native_newsar(const std::vector<Series>& args, const Environment& env,
                     std::size_t size) {
    require_arity("NEWSAR", args, 2, 2);
    Series out(size, missing);
    if (!size || !std::isfinite(args[0].front()) ||
        !std::isfinite(args[1].front()))
        return out;
    const int period = static_cast<int>(args[0].front());
    if (period < 1 || period > static_cast<int>(size)) return out;
    const auto& high = env.at("HIGH");
    const auto& low = env.at("LOW");
    const auto& close = env.at("CLOSE");
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    const auto tolerance = [&](float value) {
        return std::abs(value) * relative_epsilon + native_epsilon;
    };
    for (std::size_t index = 0; index < size; ++index)
        if (!std::isfinite(high[index]) || !std::isfinite(low[index]) ||
            !std::isfinite(close[index]))
            return out;

    const std::size_t initial = static_cast<std::size_t>(period - 1);
    const float latest_close = static_cast<float>(close[initial]);
    const float previous_close = initial
        ? static_cast<float>(close[initial - 1]) : 0.0f;
    bool falling = previous_close >= static_cast<double>(latest_close) +
                                  tolerance(latest_close);
    const auto initial_window = newsar_window(high, low, initial, period);
    float previous = falling ? initial_window.second : initial_window.first;
    out[initial] = static_cast<double>(previous);
    const float base_acceleration = static_cast<float>(args[1].front() / 1000.0);
    float acceleration = base_acceleration;
    bool continued = false;

    for (std::size_t index = initial + 1; index < size; ++index) {
        const float current_high = static_cast<float>(high[index]);
        const float current_low = static_cast<float>(low[index]);
        const float current_close = static_cast<float>(close[index]);
        if (falling) {
            const float previous_low = static_cast<float>(low[index - 1]);
            if (continued && previous_low >= static_cast<double>(current_low) +
                                              tolerance(current_low)) {
                const float increment = std::isfinite(args[1][index])
                    ? static_cast<float>(args[1][index] / 1000.0) : 0.0f;
                acceleration = static_cast<float>(acceleration + increment);
            }
            const float projected = static_cast<float>(
                (static_cast<double>(current_low) - previous) * acceleration + previous);
            if (static_cast<double>(current_close) - tolerance(current_close) >= projected) {
                const auto window = newsar_window(high, low, index - 1, period);
                previous = window.first;
                falling = false;
                continued = false;
                acceleration = base_acceleration;
            } else {
                previous = projected;
                continued = true;
            }
        } else {
            const float previous_high = static_cast<float>(high[index - 1]);
            if (continued && previous_high <= static_cast<double>(current_high) -
                                               tolerance(current_high)) {
                const float increment = std::isfinite(args[1][index])
                    ? static_cast<float>(args[1][index] / 1000.0) : 0.0f;
                acceleration = static_cast<float>(acceleration + increment);
            }
            const float projected = static_cast<float>(
                (static_cast<double>(current_high) - previous) * acceleration + previous);
            if (static_cast<double>(current_close) + tolerance(current_close) <= projected) {
                const auto window = newsar_window(high, low, index - 1, period);
                previous = window.second;
                falling = true;
                continued = false;
                acceleration = base_acceleration;
            } else {
                previous = projected;
                continued = true;
            }
        }
        out[index] = static_cast<double>(previous);
    }
    return out;
}

}  // namespace tdx::formula_engine_detail
