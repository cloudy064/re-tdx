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

constexpr float tcalc_missing_sentinel = -4.0398103e34F;
constexpr double tcalc_integer_bias = tdx::formula_engine_detail::tcalc_constants::integer_bias;

float tcalc_scalar_operand(double value) {
    return std::isfinite(value) ? static_cast<float>(value)
                                : tcalc_missing_sentinel;
}

double tcalc_scalar_output(float value) {
    return value == tcalc_missing_sentinel || !std::isfinite(value)
        ? missing : static_cast<double>(value);
}

bool tcalc_sqrt_carries(float value) {
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    const float magnitude = static_cast<float>(std::fabs(value));
    return static_cast<double>(magnitude) * relative_epsilon +
               static_cast<double>(value) + absolute_epsilon <= 0.0;
}

Series tcalc_square_root(const Series& source) {
    Series out(source.size(), missing);

    // TCalc!sub_1001D820 skips the leading canonical sentinel.  Its first
    // bar has a separate carry check because no previous output exists.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_scalar_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    if (first == source.size()) return out;

    std::size_t index = first;
    if (first == 0 && tcalc_sqrt_carries(tcalc_scalar_operand(source[0])))
        index = 1;
    for (; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        const float result = tcalc_sqrt_carries(operand)
            ? tcalc_scalar_operand(out[index - 1])
            : static_cast<float>(std::sqrt(operand));
        out[index] = tcalc_scalar_output(result);
    }
    return out;
}

int tcalc_safe_integer(double value) {
    if (!std::isfinite(value) || value >= 2147483648.0 ||
        value < -2147483648.0)
        return std::numeric_limits<int>::min();
    return static_cast<int>(std::trunc(value));
}

Series tcalc_integer_part(const Series& source) {
    Series out(source.size(), missing);

    // TCalc!sub_1001DD50 skips only the leading sentinel.  Later sentinels
    // pass through the native float adjustment and integer conversion.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_scalar_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    constexpr float adjustment = 0.000099999997F;
    for (std::size_t index = first; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        const float magnitude = static_cast<float>(std::fabs(operand));
        const bool negative = static_cast<double>(operand) -
                                  static_cast<double>(magnitude) *
                                      relative_epsilon -
                                  absolute_epsilon <
                              0.0;
        const double adjusted = static_cast<double>(operand) +
            static_cast<double>(negative ? -adjustment : adjustment);
        const int converted = tcalc_safe_integer(adjusted);
        out[index] = static_cast<double>(static_cast<float>(converted));
    }
    return out;
}

Series tcalc_round(const Series& source) {
    Series out(source.size(), missing);

    // TCalc!sub_1000C860 checks the canonical sentinel on every bar.  All
    // other operands first land in float, receive the sign-specific native
    // bias, and are converted through the runtime's signed i32 rule.
    for (std::size_t index = 0; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        if (operand == tcalc_missing_sentinel) continue;
        const double adjusted = static_cast<double>(operand) +
            (operand < 0.0F ? -tcalc_integer_bias : tcalc_integer_bias);
        out[index] = static_cast<double>(
            static_cast<float>(tcalc_safe_integer(adjusted)));
    }
    return out;
}

Series tcalc_ceiling(const Series& source) {
    Series out(source.size(), missing);
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

    // TCalc!sub_1001DAC0 skips only the leading canonical sentinel.  Later
    // sentinels participate in the ordinary float/tolerance conversion.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_scalar_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    for (std::size_t index = first; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        const float magnitude = static_cast<float>(std::fabs(operand));
        const double relative = static_cast<double>(magnitude) *
                                relative_epsilon;
        const double lower = (static_cast<double>(operand) - relative) -
                             absolute_epsilon;
        int converted = tcalc_safe_integer(operand);
        if (lower >= 0.0) {
            const int increment = static_cast<double>(converted) <= lower
                ? 1 : 0;
            converted = tcalc_safe_integer(
                static_cast<double>(operand) + increment);
        }
        out[index] = static_cast<double>(static_cast<float>(converted));
    }
    return out;
}

Series tcalc_floor(const Series& source) {
    Series out(source.size(), missing);
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

    // TCalc!sub_1001DC00 has the same leading-only sentinel gate as CEILING,
    // with the mirrored lower-integer adjustment for negative operands.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_scalar_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    for (std::size_t index = first; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        const float magnitude = static_cast<float>(std::fabs(operand));
        const double relative = static_cast<double>(magnitude) *
                                relative_epsilon;
        const double upper = static_cast<double>(operand) + relative +
                             absolute_epsilon;
        int converted = tcalc_safe_integer(operand);
        if (upper <= 0.0) {
            const int decrement = static_cast<double>(converted) >= upper
                ? 1 : 0;
            converted = tcalc_safe_integer(
                static_cast<double>(operand) - decrement);
        }
        out[index] = static_cast<double>(static_cast<float>(converted));
    }
    return out;
}

enum class TcalcExponentialLogarithmKind {
    exponential,
    natural_logarithm,
    common_logarithm,
};

bool tcalc_exponential_logarithm_invalid(
    float operand, TcalcExponentialLogarithmKind kind) {
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    const float magnitude = static_cast<float>(std::fabs(operand));
    if (kind == TcalcExponentialLogarithmKind::exponential) {
        return static_cast<double>(magnitude) * relative_epsilon +
                   static_cast<double>(operand) + absolute_epsilon >
               88.0;
    }
    return static_cast<double>(operand) -
               static_cast<double>(magnitude) * relative_epsilon -
               absolute_epsilon <
           0.0;
}

float tcalc_exponential_logarithm_value(
    float operand, TcalcExponentialLogarithmKind kind) {
    const double value = static_cast<double>(operand);
    if (kind == TcalcExponentialLogarithmKind::exponential)
        return static_cast<float>(std::exp(value));
    if (kind == TcalcExponentialLogarithmKind::natural_logarithm)
        return static_cast<float>(std::log(value));
    return static_cast<float>(std::log10(value));
}

Series tcalc_exponential_logarithm(
    const Series& source, TcalcExponentialLogarithmKind kind,
    bool literal_constant) {
    Series out(source.size(), missing);
    if (source.empty()) return out;

    if (literal_constant) {
        // TCalc's type-3 path reads the final raw f32 value, validates it
        // once, computes once, then fills the entire result buffer.
        const float operand = tcalc_scalar_operand(source.back());
        if (operand == tcalc_missing_sentinel ||
            tcalc_exponential_logarithm_invalid(operand, kind))
            return out;
        const double value = tcalc_scalar_output(
            tcalc_exponential_logarithm_value(operand, kind));
        return constant(value, source.size());
    }

    // The Series handlers skip only the leading canonical sentinel.  Invalid
    // values after evaluation starts carry the previous raw-f32 output.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_scalar_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    if (first == source.size()) return out;

    if (first == 0) {
        const float operand = tcalc_scalar_operand(source.front());
        if (kind == TcalcExponentialLogarithmKind::exponential) {
            if (tcalc_exponential_logarithm_invalid(operand, kind)) ++first;
        } else {
            constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
            constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
            const float magnitude = static_cast<float>(std::fabs(operand));
            if (static_cast<double>(magnitude) * relative_epsilon +
                    static_cast<double>(operand) + absolute_epsilon <=
                1.0)
                ++first;
        }
    }

    float previous = tcalc_missing_sentinel;
    for (std::size_t index = first; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        if (!tcalc_exponential_logarithm_invalid(operand, kind))
            previous = tcalc_exponential_logarithm_value(operand, kind);
        out[index] = tcalc_scalar_output(previous);
    }
    return out;
}

enum class TcalcTrigonometricKind {
    arc_cosine,
    arc_sine,
    tangent,
};

enum class TcalcDirectTrigonometricKind {
    arc_tangent,
    cosine,
    sine,
};

bool tcalc_inverse_trigonometric_invalid(
    float operand, TcalcTrigonometricKind kind, bool literal_constant) {
    if (literal_constant && kind == TcalcTrigonometricKind::arc_cosine)
        return operand < -1.0F || operand > 1.0F;
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    const float magnitude = static_cast<float>(std::fabs(operand));
    const double relative = static_cast<double>(magnitude) *
                            relative_epsilon;
    return static_cast<double>(operand) + relative + absolute_epsilon <= -1.0 ||
           static_cast<double>(operand) - relative - absolute_epsilon >= 1.0;
}

float tcalc_trigonometric_value(float operand,
                                TcalcTrigonometricKind kind) {
    const double value = static_cast<double>(operand);
    if (kind == TcalcTrigonometricKind::arc_cosine)
        return static_cast<float>(std::acos(value));
    if (kind == TcalcTrigonometricKind::arc_sine)
        return static_cast<float>(std::asin(value));
    return static_cast<float>(std::tan(value));
}

bool tcalc_tangent_carries(float operand) {
    return static_cast<float>(std::cos(static_cast<double>(operand))) == 0.0F;
}

Series tcalc_trigonometric(const Series& source,
                           TcalcTrigonometricKind kind,
                           bool literal_constant) {
    Series out(source.size(), missing);
    if (source.empty()) return out;

    if (literal_constant) {
        const float operand = tcalc_scalar_operand(source.back());
        if (operand == tcalc_missing_sentinel) return out;
        const bool invalid = kind == TcalcTrigonometricKind::tangent
            ? tcalc_tangent_carries(operand)
            : tcalc_inverse_trigonometric_invalid(
                  operand, kind, true);
        if (invalid) return out;
        return constant(tcalc_scalar_output(
            tcalc_trigonometric_value(operand, kind)), source.size());
    }

    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_scalar_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    if (first == source.size()) return out;

    if (first == 0) {
        const float operand = tcalc_scalar_operand(source.front());
        const bool invalid = kind == TcalcTrigonometricKind::tangent
            ? tcalc_tangent_carries(operand)
            : tcalc_inverse_trigonometric_invalid(
                  operand, kind, false);
        if (invalid) ++first;
    }

    float previous = tcalc_missing_sentinel;
    for (std::size_t index = first; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        const bool invalid = kind == TcalcTrigonometricKind::tangent
            ? tcalc_tangent_carries(operand)
            : tcalc_inverse_trigonometric_invalid(
                  operand, kind, false);
        if (!invalid)
            previous = tcalc_trigonometric_value(operand, kind);
        out[index] = tcalc_scalar_output(previous);
    }
    return out;
}

float tcalc_direct_trigonometric_value(
    float operand, TcalcDirectTrigonometricKind kind) {
    const double value = static_cast<double>(operand);
    if (kind == TcalcDirectTrigonometricKind::arc_tangent)
        return static_cast<float>(std::atan(value));
    if (kind == TcalcDirectTrigonometricKind::cosine)
        return static_cast<float>(std::cos(value));
    return static_cast<float>(std::sin(value));
}

Series tcalc_direct_trigonometric(
    const Series& source, TcalcDirectTrigonometricKind kind,
    bool literal_constant) {
    Series out(source.size(), missing);
    if (source.empty()) return out;

    // TCalc!sub_1001CBF0/sub_1001CCE0/sub_1001CDD0 use the final raw-f32
    // operand for their direct-literal type-3 path and broadcast the f32
    // result.  A normal numeric Series is processed independently per bar.
    // Their separate 6*N+2 composite-buffer pass-through representation is
    // not part of this interpreter's numeric Series value type.
    if (literal_constant) {
        const float operand = tcalc_scalar_operand(source.back());
        if (operand == tcalc_missing_sentinel) return out;
        return constant(tcalc_scalar_output(
            tcalc_direct_trigonometric_value(operand, kind)), source.size());
    }

    for (std::size_t index = 0; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        if (operand == tcalc_missing_sentinel) continue;
        out[index] = tcalc_scalar_output(
            tcalc_direct_trigonometric_value(operand, kind));
    }
    return out;
}

Series tcalc_sign(const Series& source) {
    Series out(source.size(), missing);
    constexpr float epsilon = 0.0000099999997F;
    for (std::size_t index = 0; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        if (operand == tcalc_missing_sentinel) continue;
        out[index] = operand >= epsilon ? 1.0
            : operand <= -epsilon ? -1.0 : 0.0;
    }
    return out;
}

Series tcalc_fractional_part(const Series& source) {
    Series out(source.size(), missing);
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    constexpr float adjustment = 0.000099999997F;

    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_scalar_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    for (std::size_t index = first; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        const float magnitude = static_cast<float>(std::fabs(operand));
        const bool negative = static_cast<double>(operand) -
                                  static_cast<double>(magnitude) *
                                      relative_epsilon -
                                  absolute_epsilon <
                              0.0;
        const int integer = tcalc_safe_integer(
            static_cast<double>(operand) +
            static_cast<double>(negative ? -adjustment : adjustment));
        const float result = static_cast<float>(
            static_cast<double>(operand) - static_cast<double>(integer));
        out[index] = tcalc_scalar_output(result);
    }
    return out;
}

Series tcalc_constant(const Series& source) {
    if (source.empty()) return {};

    // TCalc!sub_1000F3B0 reads only the final raw-f32 source slot and copies
    // that exact value across the output buffer.
    const float value = tcalc_scalar_operand(source.back());
    return constant(tcalc_scalar_output(value), source.size());
}

Series tcalc_constant_at(const Series& source,
                         const Series& offset_source) {
    const auto size = std::min(source.size(), offset_source.size());
    if (!size) return {};

    // TCalc!sub_1000F360 reads only the final raw-f32 offset, converts it
    // through __ftol2_sse, clamps it to the available history, and broadcasts
    // the selected raw-f32 source slot.
    const float offset_operand = tcalc_scalar_operand(
        offset_source[size - 1]);
    const int converted = tcalc_safe_integer(
        static_cast<double>(offset_operand));
    const int offset = std::clamp(converted, 0,
                                  static_cast<int>(size) - 1);
    const float value = tcalc_scalar_operand(
        source[size - 1 - static_cast<std::size_t>(offset)]);
    return constant(tcalc_scalar_output(value), size);
}

Series tcalc_round_to_precision(const Series& source,
                                const Series& precision_source) {
    const auto size = std::min(source.size(), precision_source.size());
    Series out(size, missing);
    if (!size) return out;

    // TCalc!sub_10038870 reads precision from the first raw-f32 slot, then
    // truncates and clamps it to 0..4 for the entire series.
    const float precision_operand = tcalc_scalar_operand(
        precision_source.front());
    const int precision = std::clamp(
        tcalc_safe_integer(static_cast<double>(precision_operand)), 0, 4);
    float scale = 1.0F;
    for (int step = 0; step < precision; ++step) scale *= 10.0F;

    for (std::size_t index = 0; index < size; ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        if (operand == tcalc_missing_sentinel) continue;
        const float adjusted = static_cast<float>(
            static_cast<double>(scale) * static_cast<double>(operand) +
            (operand < 0.0F ? -tcalc_integer_bias : tcalc_integer_bias));
        const float integer = static_cast<float>(
            operand < 0.0F ? std::ceil(adjusted) : std::floor(adjusted));
        out[index] = tcalc_scalar_output(
            static_cast<float>(integer / scale));
    }
    return out;
}

Series tcalc_absolute_value(const Series& source) {
    Series out(source.size(), missing);

    // TCalc!sub_1001D9F0 skips only the leading canonical sentinel.  Once
    // started, it preserves later sentinels and applies fabs through a float
    // landing point to every other operand.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_scalar_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    for (std::size_t index = first; index < source.size(); ++index) {
        const float operand = tcalc_scalar_operand(source[index]);
        const float result = operand == tcalc_missing_sentinel
            ? operand : static_cast<float>(std::fabs(operand));
        out[index] = tcalc_scalar_output(result);
    }
    return out;
}

int tcalc_biased_integer(float value) {
    const double adjusted = static_cast<double>(value) + tcalc_integer_bias;
    return tcalc_safe_integer(adjusted);
}

Series tcalc_modulo(const Series& dividends, const Series& divisors) {
    const auto size = std::min(dividends.size(), divisors.size());
    Series out(size, missing);
    for (std::size_t index = 0; index < size; ++index) {
        const float dividend = tcalc_scalar_operand(dividends[index]);
        const float divisor_operand = tcalc_scalar_operand(divisors[index]);
        if (dividend == tcalc_missing_sentinel ||
            divisor_operand == tcalc_missing_sentinel)
            continue;

        const int divisor = tcalc_biased_integer(divisor_operand);
        if (divisor == 0) continue;
        const int numerator = tcalc_biased_integer(dividend);

        // Widen before `%` so the native signed-remainder rule remains
        // deterministic for INT_MIN / -1 instead of invoking C++ overflow.
        const auto remainder = static_cast<std::int64_t>(numerator) %
                               static_cast<std::int64_t>(divisor);
        out[index] = tcalc_scalar_output(static_cast<float>(remainder));
    }
    return out;
}

bool tcalc_pow_integer_exponent(float exponent) {
    return static_cast<double>(exponent) ==
           static_cast<double>(tcalc_safe_integer(exponent));
}

bool tcalc_pow_within_output_range(float magnitude_base, float exponent) {
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

    // The logarithm is stored to float, but its product with the float
    // exponent remains wide on the x87 stack. Only the magnitude used for
    // the tolerance term is narrowed through another float landing point.
    const float native_log = static_cast<float>(
        std::log10(static_cast<double>(magnitude_base)));
    const double product = static_cast<double>(native_log) *
                           static_cast<double>(exponent);
    const float native_product = static_cast<float>(product);
    return product +
               static_cast<double>(std::fabs(native_product)) *
                   relative_epsilon +
               absolute_epsilon <=
           38.0;
}

Series tcalc_power(const Series& bases, const Series& exponents) {
    const auto size = std::min(bases.size(), exponents.size());
    Series out(size, missing);
    constexpr float zero_epsilon = 0.0000099999997F;

    // TCalc!sub_1001D5E0 starts only when both operands are non-sentinel.
    // Later sentinels participate as their finite native float value.
    std::size_t first = 0;
    while (first < size &&
           (tcalc_scalar_operand(bases[first]) == tcalc_missing_sentinel ||
            tcalc_scalar_operand(exponents[first]) ==
                tcalc_missing_sentinel))
        ++first;

    float previous = tcalc_missing_sentinel;
    for (std::size_t index = first; index < size; ++index) {
        const float base = tcalc_scalar_operand(bases[index]);
        const float exponent = tcalc_scalar_operand(exponents[index]);
        bool valid = false;
        float result = previous;

        if (base > zero_epsilon) {
            valid = tcalc_pow_within_output_range(base, exponent);
        } else if (base >= -zero_epsilon) {
            if (exponent > zero_epsilon) {
                result = 0.0F;
                valid = true;
            }
        } else if (tcalc_pow_integer_exponent(exponent)) {
            valid = tcalc_pow_within_output_range(-base, exponent);
        }

        if (valid && !(base >= -zero_epsilon && base <= zero_epsilon)) {
            result = static_cast<float>(
                std::pow(static_cast<double>(base),
                         static_cast<double>(exponent)));
        }
        previous = result;
        out[index] = tcalc_scalar_output(result);
    }
    return out;
}

Series tcalc_between(const Series& values, const Series& first_bounds,
                     const Series& second_bounds) {
    const auto size = std::min(values.size(),
                               std::min(first_bounds.size(),
                                        second_bounds.size()));
    Series out(size, missing);
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

    // TCalc!sub_1001DE70 skips a leading bar only while both bounds are the
    // canonical sentinel. The value does not participate in this gate, and
    // all three raw float operands participate without revalidation later.
    std::size_t first = 0;
    while (first < size &&
           tcalc_scalar_operand(first_bounds[first]) ==
               tcalc_missing_sentinel &&
           tcalc_scalar_operand(second_bounds[first]) ==
               tcalc_missing_sentinel)
        ++first;

    for (std::size_t index = first; index < size; ++index) {
        const float value = tcalc_scalar_operand(values[index]);
        const float first_bound = tcalc_scalar_operand(first_bounds[index]);
        const float second_bound = tcalc_scalar_operand(second_bounds[index]);
        const float maximum = second_bound >= first_bound
            ? second_bound : first_bound;
        const float minimum = second_bound <= first_bound
            ? second_bound : first_bound;

        // fabs(value) lands in float before the relative term. Preserve the
        // native x87 evaluation order on both open tolerance boundaries.
        const float magnitude = static_cast<float>(std::fabs(value));
        const double relative = static_cast<double>(magnitude) *
                                relative_epsilon;
        const double lower =
            (static_cast<double>(value) - relative) - absolute_epsilon;
        const double upper =
            (relative + static_cast<double>(value)) + absolute_epsilon;
        out[index] = static_cast<double>(
            !(static_cast<double>(maximum) <= lower ||
              static_cast<double>(minimum) >= upper));
    }
    return out;
}

Series tcalc_range(const Series& values, const Series& lower_bounds,
                   const Series& upper_bounds) {
    const auto size = std::min(values.size(),
                               std::min(lower_bounds.size(),
                                        upper_bounds.size()));
    Series out(size, missing);
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

    // TCalc!sub_10016900 delays output only while both bound buffers carry
    // their leading canonical sentinel. The value buffer does not
    // participate in that gate, and every raw-f32 operand participates after
    // the first usable bound pair.
    std::size_t first = 0;
    while (first < size &&
           tcalc_scalar_operand(lower_bounds[first]) ==
               tcalc_missing_sentinel &&
           tcalc_scalar_operand(upper_bounds[first]) ==
               tcalc_missing_sentinel)
        ++first;

    for (std::size_t index = first; index < size; ++index) {
        const float value = tcalc_scalar_operand(values[index]);
        const float lower = tcalc_scalar_operand(lower_bounds[index]);
        const float upper = tcalc_scalar_operand(upper_bounds[index]);
        const float magnitude = static_cast<float>(std::fabs(value));
        const double relative = static_cast<double>(magnitude) *
                                relative_epsilon;
        const double lower_edge = static_cast<double>(value) - relative -
                                  absolute_epsilon;
        const double upper_edge = relative + static_cast<double>(value) +
                                  absolute_epsilon;
        out[index] = static_cast<double>(
            static_cast<float>(static_cast<double>(lower) <= lower_edge &&
                               static_cast<double>(upper) >= upper_edge));
    }
    return out;
}

Series tcalc_binary_extreme(const Series& left, const Series& right,
                            bool maximum) {
    const auto size = std::min(left.size(), right.size());
    Series out(size, missing);
    std::size_t first = 0;
    while (first < size &&
           (!std::isfinite(left[first]) || !std::isfinite(right[first])))
        ++first;
    if (first == size) return out;

    for (std::size_t index = first; index < size; ++index) {
        // The native handlers validate both operands only while finding their
        // first jointly valid bar. Afterwards the finite missing sentinel goes
        // through the ordinary float comparison.
        const float native_left = std::isfinite(left[index])
            ? static_cast<float>(left[index]) : tcalc_missing_sentinel;
        const float native_right = std::isfinite(right[index])
            ? static_cast<float>(right[index]) : tcalc_missing_sentinel;

        // sub_1000B920 selects right when right >= left; sub_1000BA70 selects
        // right when right <= left. Preserve those equality directions even
        // though equal float operands have the same numeric result.
        const float selected = maximum
            ? (native_right >= native_left ? native_right : native_left)
            : (native_right <= native_left ? native_right : native_left);
        out[index] = selected == tcalc_missing_sentinel
            ? missing : static_cast<double>(selected);
    }
    return out;
}

}  // namespace

std::optional<Series> evaluate_literal_scalar_builtin_function(
    const std::string& name, double value, std::size_t size) {
    if (name == "EXP")
        return tcalc_exponential_logarithm(
            constant(value, size),
            TcalcExponentialLogarithmKind::exponential, true);
    if (name == "LN")
        return tcalc_exponential_logarithm(
            constant(value, size),
            TcalcExponentialLogarithmKind::natural_logarithm, true);
    if (name == "LOG")
        return tcalc_exponential_logarithm(
            constant(value, size),
            TcalcExponentialLogarithmKind::common_logarithm, true);
    if (name == "ACOS")
        return tcalc_trigonometric(
            constant(value, size), TcalcTrigonometricKind::arc_cosine,
            true);
    if (name == "ASIN")
        return tcalc_trigonometric(
            constant(value, size), TcalcTrigonometricKind::arc_sine,
            true);
    if (name == "TAN")
        return tcalc_trigonometric(
            constant(value, size), TcalcTrigonometricKind::tangent, true);
    if (name == "ATAN")
        return tcalc_direct_trigonometric(
            constant(value, size),
            TcalcDirectTrigonometricKind::arc_tangent, true);
    if (name == "COS")
        return tcalc_direct_trigonometric(
            constant(value, size), TcalcDirectTrigonometricKind::cosine,
            true);
    if (name == "SIN")
        return tcalc_direct_trigonometric(
            constant(value, size), TcalcDirectTrigonometricKind::sine,
            true);
    return std::nullopt;
}

std::optional<Series> evaluate_scalar_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    (void)env;
    if (name == "IF" || name == "IFF") {
        require_arity(name, args, 3, 3); Series out(size, missing);
        bool condition_started = false;
        for (std::size_t i = 0; i < size; ++i) {
            if (!condition_started && !std::isfinite(args[0][i])) continue;
            condition_started = true;
            // TCalc!sub_1000B670 writes the selected branch through a
            // dword-sized float slot.  This also covers the numeric
            // placeholder evaluated for a string IF; its StringSeries is
            // selected separately by evaluate_string_node.
            out[i] = native_float(tcalc_if_select_true(args[0][i])
                ? args[1][i] : args[2][i]);
        }
        return out;
    }
    if (name == "IFN") {
        require_arity(name, args, 3, 3); Series out(size, missing);
        bool condition_started = false;
        for (std::size_t i = 0; i < size; ++i) {
            if (!condition_started && !std::isfinite(args[0][i])) continue;
            condition_started = true;
            out[i] = native_float(tcalc_if_select_true(args[0][i])
                ? args[2][i] : args[1][i]);
        }
        return out;
    }
    if (name == "ISVALID") {
        require_arity(name, args, 1, 1); Series out(size, 0.0);
        for (std::size_t i = 0; i < size; ++i)
            out[i] = std::isfinite(args[0][i]) ? 1.0 : 0.0;
        return out;
    }
    if (name == "MAX" || name == "MIN") {
        require_arity(name, args, 2, 64);
        Series out = tcalc_binary_extreme(args[0], args[1], name == "MAX");
        // TCalc registers MAX/MIN as binary handlers. Keep the interpreter's
        // variadic extension as an explicit left fold through that handler.
        for (std::size_t argument = 2; argument < args.size(); ++argument)
            out = tcalc_binary_extreme(out, args[argument], name == "MAX");
        return out;
    }
    if (name == "MAX6" || name == "MIN6") {
        require_arity(name, args, 6, 6);
        Series out(size, missing);
        const bool maximum = name == "MAX6";
        for (std::size_t i = 0; i < size; ++i) {
            bool valid = true; double value = args.front()[i];
            if (!std::isfinite(value)) valid = false;
            for (std::size_t a = 1; a < args.size(); ++a) {
                if (!std::isfinite(args[a][i])) { valid = false; break; }
                value = maximum ? std::max(value, args[a][i])
                                : std::min(value, args[a][i]);
            }
            if (valid) out[i] = value;
        }
        return out;
    }
    if (name == "ABS") {
        require_arity(name, args, 1, 1);
        return tcalc_absolute_value(args[0]);
    }
    if (name == "SQRT") {
        require_arity(name, args, 1, 1);
        return tcalc_square_root(args[0]);
    }
    if (name == "INTPART") {
        require_arity(name, args, 1, 1);
        return tcalc_integer_part(args[0]);
    }
    if (name == "ROUND") {
        require_arity(name, args, 1, 1);
        return tcalc_round(args[0]);
    }
    if (name == "CEILING") {
        require_arity(name, args, 1, 1);
        return tcalc_ceiling(args[0]);
    }
    if (name == "FLOOR") {
        require_arity(name, args, 1, 1);
        return tcalc_floor(args[0]);
    }
    if (name == "EXP") {
        require_arity(name, args, 1, 1);
        return tcalc_exponential_logarithm(
            args[0], TcalcExponentialLogarithmKind::exponential, false);
    }
    if (name == "LN") {
        require_arity(name, args, 1, 1);
        return tcalc_exponential_logarithm(
            args[0], TcalcExponentialLogarithmKind::natural_logarithm, false);
    }
    if (name == "LOG") {
        require_arity(name, args, 1, 1);
        return tcalc_exponential_logarithm(
            args[0], TcalcExponentialLogarithmKind::common_logarithm, false);
    }
    if (name == "ACOS") {
        require_arity(name, args, 1, 1);
        return tcalc_trigonometric(
            args[0], TcalcTrigonometricKind::arc_cosine, false);
    }
    if (name == "ASIN") {
        require_arity(name, args, 1, 1);
        return tcalc_trigonometric(
            args[0], TcalcTrigonometricKind::arc_sine, false);
    }
    if (name == "TAN") {
        require_arity(name, args, 1, 1);
        return tcalc_trigonometric(
            args[0], TcalcTrigonometricKind::tangent, false);
    }
    if (name == "FRACPART") {
        require_arity(name, args, 1, 1);
        return tcalc_fractional_part(args[0]);
    }
    if (name == "SIGN" || name == "SGN") {
        require_arity(name, args, 1, 1);
        return tcalc_sign(args[0]);
    }
    if (name == "ATAN") {
        require_arity(name, args, 1, 1);
        return tcalc_direct_trigonometric(
            args[0], TcalcDirectTrigonometricKind::arc_tangent, false);
    }
    if (name == "COS") {
        require_arity(name, args, 1, 1);
        return tcalc_direct_trigonometric(
            args[0], TcalcDirectTrigonometricKind::cosine, false);
    }
    if (name == "SIN") {
        require_arity(name, args, 1, 1);
        return tcalc_direct_trigonometric(
            args[0], TcalcDirectTrigonometricKind::sine, false);
    }
    if (name == "POW") {
        require_arity(name, args, 2, 2);
        return tcalc_power(args[0], args[1]);
    }
    if (name == "MOD") {
        require_arity(name, args, 2, 2);
        return tcalc_modulo(args[0], args[1]);
    }
    if (name == "BETWEEN") {
        require_arity(name, args, 3, 3);
        return tcalc_between(args[0], args[1], args[2]);
    }
    if (name == "RANGE") {
        require_arity(name, args, 3, 3);
        return tcalc_range(args[0], args[1], args[2]);
    }
    if (name == "CONST") {
        require_arity(name, args, 1, 1);
        return tcalc_constant(args[0]);
    }
    if (name == "CONSTA") {
        require_arity(name, args, 2, 2);
        return tcalc_constant_at(args[0], args[1]);
    }
    if (name == "ROUND2") {
        require_arity(name, args, 2, 2);
        return tcalc_round_to_precision(args[0], args[1]);
    }
    if (name == "TMA") {
        require_arity(name, args, 3, 3);
        Series out(size, missing);
        if (!size || !std::isfinite(args[1].back()) ||
            !std::isfinite(args[2].back()))
            return out;
        // TCalc!sub_10007C10 reads A/B from the final argument slots and
        // applies Y=A*Y'+B*X, seeding the first valid point with X.
        const double previous_weight = native_float(args[1].back());
        const double input_weight = native_float(args[2].back());
        double previous = missing;
        bool started = false;
        for (std::size_t i = 0; i < size; ++i) {
            if (!std::isfinite(args[0][i])) {
                if (started) previous = missing;
                continue;
            }
            if (!started) {
                previous = native_float(args[0][i]);
                started = true;
            } else if (std::isfinite(previous)) {
                previous = native_float(native_float(args[0][i]) * input_weight +
                                        previous * previous_weight);
            }
            out[i] = previous;
        }
        return out;
    }
    if (name == "AMA") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        if (!size || !std::isfinite(args[1].back())) return out;
        // TCalc!sub_10007CF0 reads the adaptive coefficient from the final
        // argument slot and applies Y=Y'+A*(X-Y'), seeding the first valid X.
        const double alpha = native_float(args[1].back());
        double previous = missing;
        for (std::size_t i = 0; i < size; ++i) {
            if (!std::isfinite(args[0][i])) continue;
            previous = std::isfinite(previous)
                ? native_float(previous +
                    alpha * (native_float(args[0][i]) - previous))
                : native_float(args[0][i]);
            out[i] = previous;
        }
        return out;
    }
    return std::nullopt;
}
}  // namespace tdx::formula_engine_detail
