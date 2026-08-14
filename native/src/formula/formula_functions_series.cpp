#include "formula_function_dispatch_internal.hpp"
#include "formula_native_constants.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_engine_detail {

namespace {

constexpr float tcalc_missing_sentinel = -4.0398103e34F;
constexpr double tcalc_absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
constexpr double tcalc_relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
constexpr std::string_view tcalc_type120_security_class_binding =
    "HOST_TYPE120_SECURITY_CLASS_RAW";
constexpr std::string_view tcalc_evaluator_market_word_binding =
    "HOST_EVALUATOR_MARKET_WORD_RAW";

float tcalc_operand(double value) {
    return std::isfinite(value) ? static_cast<float>(value)
                                : tcalc_missing_sentinel;
}

int tcalc_u16_context(const Environment& env, std::string_view name) {
    const auto found = env.find(std::string(name));
    if (found == env.end() || found->second.empty())
        throw Error("formula requires explicit context binding " +
                    std::string(name));
    const double value = found->second.back();
    if (!std::isfinite(value) || std::floor(value) != value ||
        value < 0.0 || value > 65535.0)
        throw Error("formula context binding " + std::string(name) +
                    " must be an unsigned 16-bit integer");
    return static_cast<int>(value);
}

std::int32_t tcalc_ftol2_low_i32(long double value) {
    // TCalc!sub_100702A0 converts through __ftol2_sse (signed i64) but then
    // stores only EAX and uses that dword as a signed integer.  Masked invalid
    // conversion yields the x87 integer-indefinite value, whose low dword is 0.
    constexpr long double minimum = -9223372036854775808.0L;
    constexpr long double upper_exclusive = 9223372036854775808.0L;
    if (!std::isfinite(value) || value < minimum || value >= upper_exclusive)
        return 0;
    const auto wide = static_cast<std::int64_t>(std::trunc(value));
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(wide));
}

float tcalc_limit_price_value(float price, float rate,
                              bool thousandths_precision,
                              bool upper_limit,
                              bool ordinary_market_rounding) {
    const float scale = thousandths_precision ? 1000.0F : 100.0F;
    const long double native_scale = static_cast<long double>(scale);
    const long double native_price = static_cast<long double>(price);
    const long double native_rate = static_cast<long double>(rate);
    const auto landed_quotient = [&](long double numerator) {
        const auto integer = tcalc_ftol2_low_i32(numerator);
        return static_cast<float>(
            static_cast<long double>(integer) / native_scale);
    };

    if (!ordinary_market_rounding) {
        const long double bias = upper_limit ? 0.003L : 0.997L;
        const long double multiplier = upper_limit
            ? native_rate + 1.0L : 1.0L - native_rate;
        return landed_quotient(
            multiplier * native_price * native_scale + bias);
    }
    if (!upper_limit)
        return landed_quotient(
            (1.0L - native_rate) * native_price * native_scale +
            tdx::formula_engine_detail::tcalc_constants::integer_bias_l);

    // The upper-limit path has an observable intermediate float landing:
    // first round the increment, add that float back to the original price,
    // then round the combined price a second time.
    const float increment = landed_quotient(
        native_price * native_rate * native_scale +
        tdx::formula_engine_detail::tcalc_constants::integer_bias_l);
    return landed_quotient(
        (native_price + static_cast<long double>(increment)) * native_scale +
        tdx::formula_engine_detail::tcalc_constants::integer_bias_l);
}

Series tcalc_limit_price(const Series& price, const Series& rate,
                         const Environment& env, bool upper_limit) {
    Series out(price.size(), missing);
    if (price.empty() || rate.empty()) return out;

    // TCalc ZTPRICE/DTPRICE obtain both flags through the host type-120
    // callback.  They are deliberately explicit inputs here: sub_5A3810 is
    // not equivalent to any one TNF field, so silently deriving either flag
    // from market/code would recreate the approximation this helper replaces.
    const int security_class = tcalc_u16_context(
        env, tcalc_type120_security_class_binding);
    const int market_word = tcalc_u16_context(
        env, tcalc_evaluator_market_word_binding);
    const bool thousandths_precision = security_class == 3;
    const bool ordinary_market_rounding = market_word != 44 && market_word != 2;
    const float final_rate = tcalc_operand(rate.back());
    for (std::size_t index = 0; index < price.size(); ++index) {
        const float operand = tcalc_operand(price[index]);
        if (operand == tcalc_missing_sentinel) continue;
        const float value = tcalc_limit_price_value(
            operand, final_rate, thousandths_precision, upper_limit,
            ordinary_market_rounding);
        if (value != tcalc_missing_sentinel && std::isfinite(value))
            out[index] = static_cast<double>(value);
    }
    return out;
}

Series tcalc_bars_count(const Series& source) {
    Series out(source.size(), missing);

    // TCalc!sub_10006860 skips only the leading canonical sentinel.  The
    // first valid bar is zero and, after that gate, every bar advances the
    // sequence without consulting the source again.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;

    int count = 0;
    for (std::size_t index = first; index < source.size(); ++index)
        out[index] = static_cast<double>(static_cast<float>(count++));
    return out;
}

double tcalc_dma_output(float value) {
    return value == tcalc_missing_sentinel || !std::isfinite(value)
        ? missing : static_cast<double>(value);
}

Series tcalc_dma(const Series& source, const Series& weights) {
    Series out(source.size(), missing);
    if (source.empty() || weights.empty()) return out;

    // TCalc!sub_10018410 scans only for the first bar where both native
    // float operands are non-sentinel.  Later sentinels participate in the
    // recurrence as the finite 0xF8F8F8F8 float value.
    std::size_t first = 0;
    while (first < source.size() &&
           (tcalc_operand(source[first]) == tcalc_missing_sentinel ||
            tcalc_operand(weights[first]) == tcalc_missing_sentinel))
        ++first;
    if (first == source.size()) return out;

    float previous = tcalc_operand(source[first]);
    out[first] = tcalc_dma_output(previous);
    for (std::size_t index = first + 1; index < source.size(); ++index) {
        const float current = tcalc_operand(source[index]);
        const float alpha = tcalc_operand(weights[index]);
        const double native_upper = static_cast<double>(alpha) +
            std::fabs(static_cast<double>(alpha)) * tcalc_relative_epsilon +
            tcalc_absolute_epsilon;

        // The upper branch is strict and returns X directly.  Otherwise the
        // native handler evaluates with float inputs/state and stores the
        // recurrence back through a float landing point.  Negative A is not
        // clamped and therefore extrapolates.
        previous = native_upper > 1.0
            ? current
            : static_cast<float>(
                  static_cast<double>(previous) *
                      (1.0 - static_cast<double>(alpha)) +
                  static_cast<double>(current) *
                      static_cast<double>(alpha));
        out[index] = tcalc_dma_output(previous);
    }
    return out;
}

int tcalc_slope_period(float value) {
    constexpr double native_bias = tdx::formula_engine_detail::tcalc_constants::integer_bias;
    const double adjusted = static_cast<double>(value) + native_bias;
    if (!std::isfinite(adjusted) || adjusted >= 2147483648.0 ||
        adjusted < -2147483648.0)
        return std::numeric_limits<int>::min();
    return static_cast<int>(std::trunc(adjusted));
}

double tcalc_slope_output(float value) {
    return value == tcalc_missing_sentinel || !std::isfinite(value)
        ? missing : static_cast<double>(value);
}

Series tcalc_slope(const Series& source, const Series& periods) {
    Series out(source.size(), missing);
    if (source.empty() || periods.empty()) return out;

    // TCalc!sub_1000CDD0 uses sub_10001280 to skip only the leading
    // canonical sentinel.  Internal sentinels remain ordinary float data.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;

    for (std::size_t index = first; index < source.size(); ++index) {
        const int window = tcalc_slope_period(tcalc_operand(periods[index]));
        const auto available = index - first + 1;
        if (window < 1 || static_cast<std::size_t>(window) > available)
            continue;

        float position_sum = 0.0F;
        float position_square_sum = 0.0F;
        for (int position = 1; position <= window; ++position) {
            position_sum = static_cast<float>(
                static_cast<double>(position_sum) + position);
            const auto square_bits = static_cast<std::uint32_t>(position) *
                                     static_cast<std::uint32_t>(position);
            const double signed_square =
                square_bits <= static_cast<std::uint32_t>(
                                   std::numeric_limits<std::int32_t>::max())
                    ? static_cast<double>(square_bits)
                    : static_cast<double>(static_cast<std::int64_t>(square_bits) -
                                          0x100000000LL);
            position_square_sum = static_cast<float>(
                static_cast<double>(position_square_sum) + signed_square);
        }

        float source_sum = 0.0F;
        float weighted_source_sum = 0.0F;
        for (int position = window; position >= 1; --position) {
            const auto offset = static_cast<std::size_t>(window - position);
            const float value = tcalc_operand(source[index - offset]);
            source_sum = static_cast<float>(
                static_cast<double>(source_sum) + static_cast<double>(value));
            weighted_source_sum = static_cast<float>(
                static_cast<double>(position) * static_cast<double>(value) +
                static_cast<double>(weighted_source_sum));
        }

        const float native_window = static_cast<float>(window);
        const float mean_position = static_cast<float>(
            static_cast<double>(position_sum) /
            static_cast<double>(native_window));
        const float mean_source = static_cast<float>(
            static_cast<double>(source_sum) /
            static_cast<double>(native_window));
        // The native x87 path reloads the float landing points above, but it
        // does not store either side of the final quotient through another
        // float temporary.  Keep those two expressions wide until the result
        // itself is written back to the float output series.
        const double numerator =
            static_cast<double>(weighted_source_sum) -
            static_cast<double>(mean_position) *
                static_cast<double>(mean_source) *
                static_cast<double>(native_window);
        const double denominator =
            static_cast<double>(position_square_sum) -
            static_cast<double>(native_window) *
                (static_cast<double>(mean_position) *
                 static_cast<double>(mean_position));
        const float result = static_cast<float>(
            numerator / denominator);
        out[index] = tcalc_slope_output(result);
    }
    return out;
}

int tcalc_avedev_period(float value) {
    if (!std::isfinite(value) || value >= 2147483648.0F ||
        value < -2147483648.0F)
        return std::numeric_limits<int>::min();
    return static_cast<int>(std::trunc(value));
}

double tcalc_avedev_output(float value) {
    return value == tcalc_missing_sentinel || !std::isfinite(value)
        ? missing : static_cast<double>(value);
}

Series tcalc_average_deviation(const Series& source,
                               const Series& periods) {
    Series out(source.size(), missing);
    if (source.empty() || periods.empty()) return out;

    // TCalc!sub_1001E160 reads N only from the final bar, after narrowing it
    // to float and truncating it to i32. The data gate skips only the leading
    // source sentinel; later sentinels are ordinary values in every window.
    const int window = tcalc_avedev_period(tcalc_operand(periods.back()));
    if (window < 1) return out;
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    const auto native_window = static_cast<std::size_t>(window);
    if (first == source.size() || native_window > source.size() - first)
        return out;

    // The seed mean walks oldest to newest and lands in float after every
    // divide-and-add. Rolling means also land in float once per bar.
    const float float_window = static_cast<float>(window);
    float mean = 0.0F;
    for (std::size_t offset = 0; offset < native_window; ++offset) {
        const float value = tcalc_operand(source[first + offset]);
        mean = static_cast<float>(
            static_cast<double>(value) / static_cast<double>(float_window) +
            static_cast<double>(mean));
    }

    const auto emit_window = [&](std::size_t end, float current_mean) {
        // Native deviation traversal is newest to oldest. Each subtraction
        // and fabs lands in float, while their sum stays wide until the final
        // quotient is stored through the float output buffer.
        double deviation = 0.0;
        for (std::size_t offset = 0; offset < native_window; ++offset) {
            const float value = tcalc_operand(source[end - offset]);
            const float delta = static_cast<float>(
                static_cast<double>(value) -
                static_cast<double>(current_mean));
            const float magnitude = static_cast<float>(std::fabs(delta));
            deviation += static_cast<double>(magnitude);
        }
        const float result = static_cast<float>(
            deviation / static_cast<double>(window));
        out[end] = tcalc_avedev_output(result);
    };

    std::size_t end = first + native_window - 1;
    emit_window(end, mean);
    for (++end; end < source.size(); ++end) {
        const float incoming = tcalc_operand(source[end]);
        const float outgoing = tcalc_operand(source[end - native_window]);
        mean = static_cast<float>(
            static_cast<double>(mean) +
            (static_cast<double>(incoming) - static_cast<double>(outgoing)) /
                static_cast<double>(float_window));
        emit_window(end, mean);
    }
    return out;
}

Series tcalc_last(const Series& source, const Series& begin_periods,
                  const Series& end_periods) {
    Series out(source.size(), missing);
    if (source.empty() || begin_periods.empty() || end_periods.empty())
        return out;

    // TCalc!sub_10005970 narrows and truncates both offsets from their final
    // bars. It gates only on the leading source sentinel. A zero begin offset
    // is the native all-history special case; otherwise the inclusive source
    // interval is [max(first, index - begin), index - end].
    const int begin = tcalc_avedev_period(tcalc_operand(begin_periods.back()));
    const int end = tcalc_avedev_period(tcalc_operand(end_periods.back()));
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    if (first == source.size()) return out;

    const auto first_index = static_cast<std::int64_t>(first);
    const auto source_size = static_cast<std::int64_t>(source.size());
    for (std::size_t index = first; index < source.size(); ++index) {
        const auto native_index = static_cast<std::int64_t>(index);
        std::int64_t cursor = native_index - static_cast<std::int64_t>(begin);
        if (begin == 0 || first_index > cursor) cursor = first_index;
        const std::int64_t finish =
            native_index - static_cast<std::int64_t>(end) + 1;

        bool yes = true;
        for (; cursor < finish; ++cursor) {
            // Native callers provide offsets whose scanned interval remains
            // in the source allocation. Keep hostile offsets memory-safe.
            if (cursor < 0 || cursor >= source_size) {
                yes = false;
                break;
            }
            const float value =
                tcalc_operand(source[static_cast<std::size_t>(cursor)]);
            if (static_cast<double>(value) < tcalc_absolute_epsilon &&
                static_cast<double>(value) > -tcalc_absolute_epsilon) {
                yes = false;
                break;
            }
        }
        out[index] = yes ? 1.0 : 0.0;
    }
    return out;
}

Series tcalc_nday(const Series& left, const Series& right,
                  const Series& periods) {
    const auto size = std::min(left.size(), right.size());
    Series out(size, missing);
    if (!size || periods.empty()) return out;

    // TCalc!sub_100165F0 rejects a final period sentinel before truncation,
    // then gates on the first bar where both operands are non-sentinel.
    const float final_period = tcalc_operand(periods.back());
    if (final_period == tcalc_missing_sentinel) return out;
    const int window = tcalc_avedev_period(final_period);
    if (window < 1) return out;

    std::size_t first = 0;
    while (first < size &&
           (tcalc_operand(left[first]) == tcalc_missing_sentinel ||
            tcalc_operand(right[first]) == tcalc_missing_sentinel))
        ++first;
    const auto native_window = static_cast<std::size_t>(window);
    if (first == size || native_window > size - first) return out;

    const auto first_output = first + native_window - 1;
    std::fill(out.begin() + static_cast<std::ptrdiff_t>(first_output),
              out.end(), 0.0);
    int run = 0;
    for (std::size_t index = first; index < size; ++index) {
        const float left_value = tcalc_operand(left[index]);
        const float right_value = tcalc_operand(right[index]);
        const float magnitude = static_cast<float>(std::fabs(left_value));
        const double threshold =
            static_cast<double>(left_value) -
            static_cast<double>(magnitude) * tcalc_relative_epsilon -
            tcalc_absolute_epsilon;
        if (static_cast<double>(right_value) > threshold ||
            std::isnan(right_value)) {
            run = 0;
        } else {
            ++run;
        }
        if (run == window) {
            --run;
            out[index] = 1.0;
        }
    }
    return out;
}

Series tcalc_up_nday(const Series& source, const Series& periods) {
    Series out(source.size(), missing);
    if (source.empty() || periods.empty()) return out;

    // TCalc!sub_10016330 reads N only from its final float bar, then skips
    // only the leading canonical source sentinel. The first source bar is a
    // baseline rather than a comparison, so initialized zero output begins
    // at first + N - 1 while the run starts updating at first + 1.
    const float final_period = tcalc_operand(periods.back());
    if (final_period == tcalc_missing_sentinel) return out;
    const int window = tcalc_avedev_period(final_period);
    if (window < 1) return out;
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    const auto native_window = static_cast<std::size_t>(window);
    if (first == source.size() || native_window > source.size() - first)
        return out;

    std::fill(out.begin() + static_cast<std::ptrdiff_t>(
                  first + native_window - 1),
              out.end(), 0.0);
    int run = 0;
    for (std::size_t index = first + 1; index < source.size(); ++index) {
        const float current = tcalc_operand(source[index]);
        const float previous = tcalc_operand(source[index - 1]);
        const float magnitude = static_cast<float>(std::fabs(current));
        const double lower =
            static_cast<double>(current) -
            static_cast<double>(magnitude) * tcalc_relative_epsilon -
            tcalc_absolute_epsilon;
        if (static_cast<double>(previous) > lower || std::isnan(previous))
            run = 0;
        else
            ++run;
        if (run == window) {
            --run;
            out[index] = 1.0;
        }
    }
    return out;
}

Series tcalc_down_nday(const Series& source, const Series& periods) {
    Series out(source.size(), missing);
    if (source.empty() || periods.empty()) return out;

    // TCalc!sub_10016490 has the same final-N and source-only leading gate,
    // but counts a decline only when previous is at or above current plus the
    // native relative/absolute tolerance. Post-start sentinels remain raw.
    const float final_period = tcalc_operand(periods.back());
    if (final_period == tcalc_missing_sentinel) return out;
    const int window = tcalc_avedev_period(final_period);
    if (window < 1) return out;
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    const auto native_window = static_cast<std::size_t>(window);
    if (first == source.size() || native_window > source.size() - first)
        return out;

    std::fill(out.begin() + static_cast<std::ptrdiff_t>(
                  first + native_window - 1),
              out.end(), 0.0);
    int run = 0;
    for (std::size_t index = first + 1; index < source.size(); ++index) {
        const float current = tcalc_operand(source[index]);
        const float previous = tcalc_operand(source[index - 1]);
        const float magnitude = static_cast<float>(std::fabs(current));
        const double upper =
            static_cast<double>(magnitude) * tcalc_relative_epsilon +
            static_cast<double>(current) + tcalc_absolute_epsilon;
        if (static_cast<double>(previous) < upper || std::isnan(previous))
            run = 0;
        else
            ++run;
        if (run == window) {
            --run;
            out[index] = 1.0;
        }
    }
    return out;
}

Series tcalc_low_range(const Series& source) {
    Series out(source.size(), missing);

    // TCalc!sub_100072D0 gates only on the leading canonical source
    // sentinel. For each started bar it holds the current raw float fixed and
    // counts all prior bars, newest to oldest, while current - prior is
    // strictly below the native absolute epsilon. Both leading bars reached
    // by that backward scan and internal sentinels are ordinary raw float
    // operands rather than a new validity boundary.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    for (std::size_t index = first; index < source.size(); ++index) {
        const float current = tcalc_operand(source[index]);
        std::size_t count = 0;
        for (std::size_t cursor = index; cursor > 0; --cursor) {
            const float previous = tcalc_operand(source[cursor - 1]);
            const double difference =
                static_cast<double>(current) -
                static_cast<double>(previous);
            if (difference >= -tcalc_absolute_epsilon) break;
            ++count;
        }
        out[index] = static_cast<double>(static_cast<float>(count));
    }
    return out;
}

Series tcalc_top_range(const Series& source) {
    Series out(source.size(), missing);

    // TCalc!sub_10007170 skips only the leading canonical source sentinel.
    // Once started, the current raw float is held fixed while prior raw
    // floats are scanned newest-to-oldest. Internal and even leading
    // sentinels reached by that scan remain ordinary finite operands.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    for (std::size_t index = first; index < source.size(); ++index) {
        const float current = tcalc_operand(source[index]);
        std::size_t count = 0;
        for (std::size_t cursor = index; cursor > 0; --cursor) {
            const float previous = tcalc_operand(source[cursor - 1]);
            const double difference =
                static_cast<double>(current) -
                static_cast<double>(previous);
            if (difference <= tcalc_absolute_epsilon) break;
            ++count;
        }
        out[index] = static_cast<double>(static_cast<float>(count));
    }
    return out;
}

double tcalc_value_when_output(float value) {
    return value == tcalc_missing_sentinel || !std::isfinite(value)
        ? missing : static_cast<double>(value);
}

Series tcalc_value_when(const Series& conditions,
                        const Series& selected_values) {
    const auto size = std::min(conditions.size(), selected_values.size());
    Series out(size, missing);

    // TCalc!sub_1000B830 gates only on the leading condition sentinel. Once
    // started, exact float zero carries the prior raw float output; every
    // other condition, including the sentinel, selects the current raw value.
    std::size_t first = 0;
    while (first < size &&
           tcalc_operand(conditions[first]) == tcalc_missing_sentinel)
        ++first;

    float held = tcalc_missing_sentinel;
    for (std::size_t index = first; index < size; ++index) {
        const float condition = tcalc_operand(conditions[index]);
        if (condition != 0.0F)
            held = tcalc_operand(selected_values[index]);
        out[index] = tcalc_value_when_output(held);
    }
    return out;
}

Series tcalc_sum_bars(const Series& source, const Series& target) {
    Series out(source.size(), missing);
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;

    for (std::size_t index = first; index < source.size(); ++index) {
        const float current = tcalc_operand(source[index]);
        const float threshold = tcalc_operand(target[index]);
        std::ptrdiff_t cursor = static_cast<std::ptrdiff_t>(index);
        const double lower = static_cast<double>(current) -
            std::abs(static_cast<double>(current)) * tcalc_relative_epsilon -
            tcalc_absolute_epsilon;
        if (static_cast<double>(threshold) > lower) {
            --cursor;
            float total = current;
            while (cursor >= static_cast<std::ptrdiff_t>(first)) {
                const double upper = static_cast<double>(total) +
                    std::abs(static_cast<double>(total)) *
                        tcalc_relative_epsilon +
                    tcalc_absolute_epsilon;
                if (static_cast<double>(threshold) < upper) break;
                total = static_cast<float>(
                    static_cast<double>(total) +
                    static_cast<double>(tcalc_operand(
                        source[static_cast<std::size_t>(cursor)])));
                --cursor;
            }
        }
        out[index] = cursor >= static_cast<std::ptrdiff_t>(first)
            ? static_cast<double>(static_cast<std::ptrdiff_t>(index) - cursor)
            : static_cast<double>(index - first);
    }
    return out;
}

Series tcalc_stddev(const Series& source, const Series& period) {
    Series out(source.size(), missing);
    if (source.empty() || !std::isfinite(period.back())) return out;
    const int window = static_cast<int>(
        static_cast<float>(period.back()));
    if (window < 2) return out;

    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    const auto start = first + static_cast<std::size_t>(window);
    if (first == source.size() || start >= source.size()) return out;

    constexpr float native_epsilon = 0.0000099999997F;
    const auto return_count = static_cast<std::size_t>(window - 1);
    const float divisor = static_cast<float>(window - 1);
    std::vector<float> returns(return_count, 0.0F);
    for (std::size_t index = start; index < source.size(); ++index) {
        float sum = 0.0F;
        for (std::size_t offset = 0; offset < return_count; ++offset) {
            const auto newer = index - 1 - offset;
            const auto older = newer - 1;
            const float current = tcalc_operand(source[newer]);
            const float previous = tcalc_operand(source[older]);
            float value = 0.0F;
            if (native_epsilon < current && native_epsilon < previous) {
                const float ratio = static_cast<float>(
                    static_cast<double>(current) /
                    static_cast<double>(previous));
                value = static_cast<float>(std::log(
                    static_cast<double>(ratio)));
            }
            returns[offset] = value;
            sum = static_cast<float>(static_cast<double>(sum) +
                                     static_cast<double>(value));
        }
        const float mean = static_cast<float>(
            static_cast<double>(sum) / static_cast<double>(divisor));
        float deviation = 0.0F;
        for (const float value : returns) {
            const double delta = static_cast<double>(value) -
                                 static_cast<double>(mean);
            deviation = static_cast<float>(
                delta * delta + static_cast<double>(deviation));
        }
        const float variance = static_cast<float>(
            static_cast<double>(deviation) / static_cast<double>(divisor));
        out[index] = variance >= -9.9999997e-10F
            ? static_cast<double>(static_cast<float>(std::sqrt(
                  static_cast<double>(variance))))
            : missing;
    }
    return out;
}

}  // namespace

std::optional<Series> evaluate_series_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    if (name == "COUNT" || name == "EVERY" || name == "EXIST") {
        require_arity(name, args, 2, 2);
        if (name == "COUNT") return tcalc_count(args[0], args[1]);
        if (name == "EVERY") return tcalc_every(args[0], args[1]);
        return tcalc_exist(args[0], args[1]);
    }
    if (name == "EXISTR") {
        require_arity(name, args, 3, 3);
        Series out(size, missing);
        if (!size) return out;
        const int begin_offset = std::isfinite(args[1].back())
            ? static_cast<int>(args[1].back()) : 0;
        const int end_offset = std::isfinite(args[2].back())
            ? static_cast<int>(args[2].back()) : 0;
        std::size_t first = 0;
        while (first < size && !std::isfinite(args[0][first])) ++first;
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        for (std::size_t i = first; i < size; ++i) {
            if (!std::isfinite(args[0][i])) continue;
            std::int64_t begin = static_cast<std::int64_t>(first);
            const auto rolling_begin = static_cast<std::int64_t>(i) - begin_offset;
            if (begin_offset != 0 && static_cast<std::int64_t>(first) <= rolling_begin)
                begin = rolling_begin;
            const auto end = static_cast<std::int64_t>(i) - end_offset;
            bool found = false;
            for (auto at = begin; at <= end; ++at) {
                if (at < 0 || at >= static_cast<std::int64_t>(size)) continue;
                const double value = args[0][static_cast<std::size_t>(at)];
                // The original compares the float missing sentinel numerically
                // inside the window, so an internal missing value is truthy;
                // only a missing value at the current output bar suppresses it.
                if (!std::isfinite(value) || value >= native_epsilon ||
                    value <= -native_epsilon) {
                    found = true;
                    break;
                }
            }
            out[i] = found ? 1.0 : 0.0;
        }
        return out;
    }
    if (name == "UPNDAY" || name == "DOWNNDAY") {
        require_arity(name, args, 2, 2);
        return name == "UPNDAY"
            ? tcalc_up_nday(args[0], args[1])
            : tcalc_down_nday(args[0], args[1]);
    }
    if (name == "UPDOWN") {
        require_arity(name, args, 1, 1);
        Series out(size, missing);
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        std::size_t first = 0;
        while (first < size && !std::isfinite(args[0][first])) ++first;
        for (std::size_t i = first + 1; i < size; ++i) {
            if (!std::isfinite(args[0][i]) || !std::isfinite(args[0][i - 1]))
                continue;
            const double current = native_float(args[0][i]);
            const double previous = native_float(args[0][i - 1]);
            const double tolerance = std::abs(current) * relative_epsilon +
                                     native_epsilon;
            if (previous <= current - tolerance) out[i] = 1.0;
            else if (previous < current + tolerance) out[i] = 0.0;
            else out[i] = -1.0;
        }
        return out;
    }
    if (name == "NDAY") {
        require_arity(name, args, 3, 3);
        return tcalc_nday(args[0], args[1], args[2]);
    }
    if (name == "LAST") {
        require_arity(name, args, 3, 3);
        return tcalc_last(args[0], args[1], args[2]);
    }
    if (name == "FILTER") {
        require_arity(name, args, 2, 2);
        return tcalc_filter(args[0], args[1]);
    }
    if (name == "FILTERX") {
        require_arity(name, args, 2, 2);
        return tcalc_filterx(args[0], args[1]);
    }
    if (name == "BARSLASTCOUNT") {
        require_arity(name, args, 1, 1);
        return tcalc_bars_last_count(args[0]);
    }
    if (name == "CROSS") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        constexpr float native_missing_sentinel = -4.0398103e34f;
        bool started = false;
        bool was_strictly_below = false;
        for (std::size_t i = 0; i < size; ++i) {
            if (!started) {
                if (!std::isfinite(args[0][i]) ||
                    !std::isfinite(args[1][i]))
                    continue;
                started = true;
            }

            // TCalc!sub_1001C3E0 emits the crossing pulse from the previous
            // hysteresis state, then updates that state from the current bar.
            // Values inside the tolerance band leave the state unchanged.
            // After the leading validity scan the native loop no longer
            // checks its finite 0xF8F8F8F8 missing sentinel.  Restore that
            // representation locally so a later one-sided missing operand
            // updates the hysteresis state exactly like TCalc.
            const double a = std::isfinite(args[0][i])
                ? native_float(args[0][i])
                : static_cast<double>(native_missing_sentinel);
            const double b = std::isfinite(args[1][i])
                ? native_float(args[1][i])
                : static_cast<double>(native_missing_sentinel);
            const double tolerance = std::abs(a) * relative_epsilon +
                                     native_epsilon;
            out[i] = was_strictly_below && b <= a - tolerance ? 1.0 : 0.0;
            if (b >= a + tolerance) was_strictly_below = true;
            else if (b <= a - tolerance) was_strictly_below = false;
        }
        return out;
    }
    if (name == "AVEDEV") {
        require_arity(name, args, 2, 2);
        return tcalc_average_deviation(args[0], args[1]);
    }
    if (name == "DEVSQ") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        if (!size || !std::isfinite(args[1].back())) return out;
        const int window = static_cast<int>(args[1].back());
        if (window < 1) return out;
        std::size_t first = 0;
        while (first < size && !std::isfinite(args[0][first])) ++first;
        if (first == size || first + static_cast<std::size_t>(window) > size)
            return out;
        for (std::size_t i = first + static_cast<std::size_t>(window) - 1;
             i < size; ++i) {
            const auto begin = i + 1 - static_cast<std::size_t>(window);
            double mean = 0.0;
            bool valid = true;
            for (std::size_t at = begin; at <= i; ++at) {
                if (!std::isfinite(args[0][at])) { valid = false; break; }
                mean += native_float(args[0][at]) /
                        static_cast<double>(window);
            }
            if (!valid) continue;
            double sum = 0.0;
            for (std::size_t at = begin; at <= i; ++at) {
                const double delta = native_float(args[0][at]) - mean;
                sum += delta * delta;
            }
            out[i] = native_float(sum);
        }
        return out;
    }
    if (name == "VAR" || name == "VARP" ||
        name == "STD" || name == "STDP") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        std::size_t first = 0;
        while (first < size && !std::isfinite(args[0][first])) ++first;
        if (first == size) return out;
        const bool population = name == "VARP" || name == "STDP";
        const bool square_root = name == "STD" || name == "STDP";
        constexpr double native_negative_epsilon = -tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        for (std::size_t i = first; i < size; ++i) {
            if (!std::isfinite(args[1][i])) continue;
            const int window = static_cast<int>(
                native_float(args[1][i]) + tdx::formula_engine_detail::tcalc_constants::integer_bias);
            const auto available = i - first + 1;
            if (window < 1) {
                if (population && !square_root) out[i] = 0.0;
                continue;
            }
            if (population && available <= static_cast<std::size_t>(window)) {
                if (!square_root || available == static_cast<std::size_t>(window))
                    out[i] = 0.0;
                continue;
            }
            if (!population && available < static_cast<std::size_t>(window))
                continue;
            const auto begin = i + 1 - static_cast<std::size_t>(window);
            double sum = 0.0, sum_squares = 0.0;
            for (std::size_t at = begin; at <= i; ++at) {
                if (!std::isfinite(args[0][at])) continue;
                const double value = native_float(args[0][at]);
                sum += value;
                sum_squares += value * value;
            }
            double variance = 0.0;
            if (!population && window <= 1) {
                variance = 0.0;
            } else {
                const double n = static_cast<double>(window);
                variance = population
                    ? (n * sum_squares - sum * sum) / (n * n)
                    : (n * sum_squares - sum * sum) / (n * (n - 1.0));
                if (variance < native_negative_epsilon) variance = 0.0;
            }
            variance = native_float(variance);
            out[i] = square_root
                ? native_float(std::sqrt(std::max(0.0, variance)))
                : variance;
        }
        return out;
    }
    if (name == "STDDEV") {
        require_arity(name, args, 2, 2);
        return tcalc_stddev(args[0], args[1]);
    }
    if (name == "BETA") {
        require_arity(name, args, 1, 1);
        const auto close = env.find("CLOSE");
        const auto benchmark = env.find("__BETA_BENCHMARK_CLOSE");
        if (close == env.end() || benchmark == env.end() ||
            benchmark->second.size() != size)
            throw Error("missing BETA benchmark K-line formula context");
        Series security_returns(size, missing), benchmark_returns(size, missing);
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        std::size_t first = 0;
        while (first < size && (!std::isfinite(close->second[first]) ||
               !std::isfinite(benchmark->second[first]))) ++first;
        if (first < size) {
            // TCalc leaves the first aligned close untouched while converting
            // all following points to simple returns.  This affects only the
            // first complete window and is intentionally preserved.
            security_returns[first] = native_float(close->second[first]);
            benchmark_returns[first] = native_float(benchmark->second[first]);
            for (std::size_t i = first + 1; i < size; ++i) {
                if (!std::isfinite(close->second[i]) ||
                    !std::isfinite(benchmark->second[i]) ||
                    !std::isfinite(close->second[i - 1]) ||
                    !std::isfinite(benchmark->second[i - 1])) continue;
                const double previous_security = native_float(close->second[i - 1]);
                const double previous_benchmark = native_float(benchmark->second[i - 1]);
                security_returns[i] = previous_security >= native_epsilon
                    ? native_float(native_float(close->second[i]) / previous_security - 1.0)
                    : 0.0;
                benchmark_returns[i] = previous_benchmark >= native_epsilon
                    ? native_float(native_float(benchmark->second[i]) /
                                   previous_benchmark - 1.0)
                    : 0.0;
            }
        }
        return evaluate_call("BETAEX", {security_returns, benchmark_returns, args[0]},
                             env, size);
    }
    if (name == "COVAR" || name == "RELATE" || name == "BETAEX") {
        require_arity(name, args, 3, 3);
        Series out(size, missing);
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        for (std::size_t i = 0; i < size; ++i) {
            if (!std::isfinite(args[2][i])) continue;
            const int window = static_cast<int>(native_float(args[2][i]) +
                                                tdx::formula_engine_detail::tcalc_constants::integer_bias);
            if (window < 1 || static_cast<std::size_t>(window) > i + 1) continue;
            const auto begin = i + 1 - static_cast<std::size_t>(window);
            double sum_x = 0.0, sum_y = 0.0;
            bool valid = true;
            for (std::size_t at = begin; at <= i; ++at) {
                if (!std::isfinite(args[0][at]) || !std::isfinite(args[1][at])) {
                    valid = false;
                    break;
                }
                sum_x = native_float(sum_x + native_float(args[0][at]));
                sum_y = native_float(sum_y + native_float(args[1][at]));
            }
            if (!valid) continue;
            const double divisor = static_cast<double>(window);
            const double mean_x = native_float(sum_x / divisor);
            const double mean_y = native_float(sum_y / divisor);
            double covariance_sum = 0.0, variance_x_sum = 0.0,
                   variance_y_sum = 0.0;
            for (std::size_t at = begin; at <= i; ++at) {
                const double x = native_float(args[0][at]) - mean_x;
                const double y = native_float(args[1][at]) - mean_y;
                covariance_sum = native_float(covariance_sum + x * y);
                if (name == "RELATE")
                    variance_x_sum = native_float(variance_x_sum + x * x);
                if (name == "RELATE" || name == "BETAEX")
                    variance_y_sum = native_float(variance_y_sum + y * y);
            }
            if (name == "COVAR") {
                out[i] = window == 1 ? 0.0 : native_float(
                    covariance_sum / static_cast<double>(window - 1));
            } else if (name == "BETAEX") {
                out[i] = variance_y_sum >= native_epsilon
                    ? native_float(covariance_sum / variance_y_sum)
                    : (i ? out[i - 1] : 0.0);
            } else {
                const double variance_x = native_float(variance_x_sum / divisor);
                const double variance_y = native_float(variance_y_sum / divisor);
                if (variance_x * variance_y <= native_epsilon)
                    out[i] = i ? out[i - 1] : missing;
                else
                    out[i] = native_float(native_float(covariance_sum / divisor) /
                        std::sqrt(variance_x) / std::sqrt(variance_y));
            }
        }
        return out;
    }
    if (name == "BARSCOUNT") {
        require_arity(name, args, 1, 1);
        return tcalc_bars_count(args[0]);
    }
    if (name == "BARSLAST") {
        require_arity(name, args, 1, 1);
        return tcalc_bars_last(args[0]);
    }
    if (name == "DMA") {
        require_arity(name, args, 2, 2);
        return tcalc_dma(args[0], args[1]);
    }
    if (name == "HOD" || name == "LOD") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        if (!size || !std::isfinite(args[1].back())) return out;
        int requested = static_cast<int>(args[1].back());
        if (requested <= 0 || requested + 1 > static_cast<int>(size))
            requested = static_cast<int>(size);
        const bool high_rank = name == "HOD";
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        for (std::size_t i = 0; i < size; ++i) {
            const double current = args[0][i];
            if (!std::isfinite(current)) continue;
            const auto window = static_cast<std::size_t>(requested);
            const auto begin = i + 1 > window ? i + 1 - window : 0;
            const double tolerance = std::abs(current) * relative_epsilon +
                                     native_epsilon;
            int rank = 1;
            for (std::size_t at = begin; at <= i; ++at) {
                if (!std::isfinite(args[0][at])) continue;
                if (high_rank ? args[0][at] >= current + tolerance
                              : args[0][at] <= current - tolerance)
                    ++rank;
            }
            out[i] = static_cast<double>(rank);
        }
        return out;
    }
    if (name == "HHVLLV") {
        require_arity(name, args, 4, 4);
        Series out(size, missing);
        if (!size || !std::isfinite(args[1].back())) return out;
        const bool lowest = static_cast<int>(args[1].back()) == 1;
        for (std::size_t i = 0; i < size; ++i) {
            if (!std::isfinite(args[2][i]) || !std::isfinite(args[3][i]))
                continue;
            int older = static_cast<int>(args[2][i]);
            const int newer = static_cast<int>(args[3][i]);
            if (older < 1 || older > static_cast<int>(i + 1))
                older = static_cast<int>(i + 1);
            if (newer < 0 || newer > static_cast<int>(i)) continue;
            const auto begin = older >= static_cast<int>(i + 1)
                ? 0 : i - static_cast<std::size_t>(older);
            const auto end = i - static_cast<std::size_t>(newer);
            if (begin > end) continue;
            double selected = missing;
            for (std::size_t at = begin; at <= end; ++at) {
                if (!std::isfinite(args[0][at])) continue;
                if (!std::isfinite(selected) ||
                    (lowest ? args[0][at] <= selected
                            : args[0][at] >= selected))
                    selected = args[0][at];
            }
            if (std::isfinite(selected)) out[i] = native_float(selected);
        }
        return out;
    }
    if (name == "HHVBARS" || name == "LLVBARS") {
        require_arity(name, args, 2, 2);
        return name == "HHVBARS"
            ? tcalc_highest_value_bars(args[0], args[1])
            : tcalc_lowest_value_bars(args[0], args[1]);
    }
    if (name == "FINDHIGH" || name == "FINDHIGHBARS" ||
        name == "FINDLOW" || name == "FINDLOWBARS") {
        require_arity(name, args, 4, 4);
        Series out(size, missing);
        const bool lowest = name == "FINDLOW" || name == "FINDLOWBARS";
        const bool bars = name == "FINDHIGHBARS" || name == "FINDLOWBARS";
        for (std::size_t i = 0; i < size; ++i) {
            if (!std::isfinite(args[1][i]) || !std::isfinite(args[2][i]) ||
                !std::isfinite(args[3][i]))
                continue;
            const int offset = static_cast<int>(args[1][i]);
            const int requested_length = static_cast<int>(args[2][i]);
            const int requested_rank = static_cast<int>(args[3][i]);
            if (offset < 0 || requested_length < 1 || requested_rank < 1 ||
                static_cast<std::size_t>(offset) > i)
                continue;
            const auto end = i - static_cast<std::size_t>(offset);
            const auto length = std::min<std::size_t>(
                static_cast<std::size_t>(requested_length), end + 1);
            const auto begin = end + 1 - length;
            std::vector<std::pair<double, std::size_t>> candidates;
            candidates.reserve(length);
            for (std::size_t at = begin; at <= end; ++at)
                if (std::isfinite(args[0][at]))
                    candidates.emplace_back(native_float(args[0][at]), at);
            if (candidates.empty()) continue;
            std::stable_sort(candidates.begin(), candidates.end(),
                [&](const auto& left, const auto& right) {
                    if (left.first != right.first)
                        return lowest ? left.first < right.first
                                      : left.first > right.first;
                    // The native high heap retains the first equal value;
                    // its low heap replaces it with the later equal value.
                    return lowest ? left.second > right.second
                                  : left.second < right.second;
                });
            const auto rank = std::min<std::size_t>(
                static_cast<std::size_t>(requested_rank), candidates.size());
            const auto& selected = candidates[rank - 1];
            out[i] = bars ? static_cast<double>(i - selected.second)
                          : selected.first;
        }
        return out;
    }
    if (name == "LOWRANGE") {
        require_arity(name, args, 1, 1);
        return tcalc_low_range(args[0]);
    }
    if (name == "TOPRANGE") {
        require_arity(name, args, 1, 1);
        return tcalc_top_range(args[0]);
    }
    if (name == "SUMBARS") {
        require_arity(name, args, 2, 2);
        return tcalc_sum_bars(args[0], args[1]);
    }
    if (name == "SUMBARSX") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        std::size_t first = 0;
        while (first < size && !std::isfinite(args[0][first])) ++first;
        for (std::size_t i = first; i < size; ++i) {
            if (!std::isfinite(args[0][i]) || !std::isfinite(args[1][i])) continue;
            double total = native_float(args[0][i]);
            const double target = native_float(args[1][i]);
            const double lower = total - std::abs(total) * relative_epsilon -
                                 native_epsilon;
            if (target <= lower) {
                out[i] = -1.0;
                continue;
            }
            const auto reached = [&](double value) {
                return target < value + std::abs(value) * relative_epsilon +
                                      native_epsilon;
            };
            if (reached(total)) {
                out[i] = 0.0;
                continue;
            }
            for (std::size_t at = i; at > first;) {
                --at;
                if (!std::isfinite(args[0][at])) break;
                total = native_float(total + native_float(args[0][at]));
                if (!reached(total)) continue;
                out[i] = static_cast<double>(i - at);
                break;
            }
        }
        return out;
    }
    if (name == "SLOPE") {
        require_arity(name, args, 2, 2);
        return tcalc_slope(args[0], args[1]);
    }
    if (name == "WMA" || name == "FORCAST") {
        require_arity(name, args, 2, 2); Series out(size, missing);
        for (std::size_t i = 0; i < size; ++i) {
            const int n = period_at(args[1], i, 1); if (i + 1 < static_cast<std::size_t>(n)) continue;
            double sum_y = 0.0, sum_xy = 0.0, weighted = 0.0, weights = 0.0; bool valid = true;
            for (int x = 0; x < n; ++x) { const double y = args[0][i + 1 - n + x]; if (!std::isfinite(y)) { valid = false; break; } sum_y += y; sum_xy += x * y; weighted += (x + 1) * y; weights += x + 1; }
            if (!valid) continue;
            if (name == "WMA") out[i] = weighted / weights;
            else { const double sum_x = n * (n - 1.0) / 2.0, sum_x2 = n * (n - 1.0) * (2.0 * n - 1.0) / 6.0; const double denominator = n * sum_x2 - sum_x * sum_x; const double slope = std::abs(denominator) > 1e-15 ? (n * sum_xy - sum_x * sum_y) / denominator : 0.0; out[i] = sum_y / n + slope * ((n - 1.0) / 2.0); }
        }
        return out;
    }
    if (name == "REVERSE") { require_arity(name, args, 1, 1); auto out = args[0]; for (auto& x : out) if (std::isfinite(x)) x = -x; return out; }
    if (name == "VALUEWHEN") {
        require_arity(name, args, 2, 2);
        return tcalc_value_when(args[0], args[1]);
    }
    if (name == "LONGCROSS") {
        require_arity(name, args, 3, 3); Series out(size, 0.0);
        for (std::size_t i = 1; i < size; ++i) { const int n = period_at(args[2], i, 1); if (i < static_cast<std::size_t>(n)) continue; bool below = true; for (std::size_t j = i - n; j < i; ++j) if (!std::isfinite(args[0][j]) || !std::isfinite(args[1][j]) || args[0][j] >= args[1][j]) { below = false; break; } if (below && args[0][i] > args[1][i]) out[i] = 1.0; }
        return out;
    }
    if (name == "ZTPRICE" || name == "DTPRICE") {
        require_arity(name, args, 2, 2);
        return tcalc_limit_price(args[0], args[1], env, name == "ZTPRICE");
    }
    if (name == "MTM" || name == "ROC" || name == "PSY" || name == "WR" || name == "CCI") {
        require_arity(name, args, 1, name == "CCI" ? 1 : 2);
        const auto& close = env.at("CLOSE"); Series out(size, missing);
        for (std::size_t i = 0; i < size; ++i) {
            const int n = period_at(args[0], i, 1);
            if (name == "MTM" || name == "ROC") { if (i < static_cast<std::size_t>(n) || !std::isfinite(close[i - n])) continue; out[i] = close[i] - close[i - n]; if (name == "ROC") out[i] = std::abs(close[i - n]) > 1e-15 ? out[i] * 100.0 / close[i - n] : missing; continue; }
            if (i + 1 < static_cast<std::size_t>(n)) continue;
            if (name == "PSY") { int rises = 0; bool valid = i + 1 >= static_cast<std::size_t>(n + 1); if (!valid) continue; for (std::size_t j = i + 1 - n; j <= i; ++j) if (close[j] > close[j - 1]) ++rises; out[i] = rises * 100.0 / n; continue; }
            const auto& high = env.at("HIGH"); const auto& low = env.at("LOW"); double highest = high[i + 1 - n], lowest = low[i + 1 - n], sum = 0.0;
            for (std::size_t j = i + 1 - n; j <= i; ++j) { highest = std::max(highest, high[j]); lowest = std::min(lowest, low[j]); if (name == "CCI") sum += (high[j] + low[j] + close[j]) / 3.0; }
            if (name == "WR") out[i] = highest > lowest ? 100.0 * (highest - close[i]) / (highest - lowest) : 0.0;
            else { const double mean = sum / n, typ = (high[i] + low[i] + close[i]) / 3.0; double dev = 0.0; for (std::size_t j = i + 1 - n; j <= i; ++j) dev += std::abs((high[j] + low[j] + close[j]) / 3.0 - mean); dev /= n; out[i] = dev > 1e-15 ? (typ - mean) / (0.015 * dev) : 0.0; }
        }
        return out;
    }
    if (name == "TR") {
        require_arity(name, args, 0, 0); const auto& high = env.at("HIGH"), low = env.at("LOW"), close = env.at("CLOSE"); Series out(size, missing);
        if (size) out[0] = high[0] - low[0];
        for (std::size_t i = 1; i < size; ++i)
            out[i] = std::max({high[i] - low[i], std::abs(high[i] - close[i - 1]),
                               std::abs(low[i] - close[i - 1])});
        return out;
    }
    return std::nullopt;
}

}  // namespace tdx::formula_engine_detail
