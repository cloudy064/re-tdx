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

std::optional<int> tcalc_period_at(const Series& periods, std::size_t index) {
    if (index >= periods.size() || !std::isfinite(periods[index]))
        return std::nullopt;
    return tcalc_external_integer(periods[index]);
}

std::size_t first_valid_source(const Series& input) {
    const auto first = std::find_if(input.begin(), input.end(), [](double value) {
        return std::isfinite(value);
    });
    return static_cast<std::size_t>(first - input.begin());
}

std::optional<int> tcalc_extreme_period_at(const Series& periods,
                                           std::size_t index) {
    auto period = tcalc_period_at(periods, index);
    if (!period) return std::nullopt;
    const auto available = index + 1;
    if (*period < 1 || static_cast<std::size_t>(*period) > available)
        *period = static_cast<int>(available);
    return period;
}

constexpr double tcalc_absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
constexpr double tcalc_relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

}  // namespace

Series tcalc_sum(const Series& input, const Series& periods) {
    Series out(input.size(), missing);
    const auto first = first_valid_source(input);
    if (first == input.size()) return out;

    // TCalc!sub_10007720 first materializes a float prefix series.  Missing
    // source bars carry that prefix; every accepted add is rounded to float.
    float prefix = 0.0F;
    for (std::size_t index = first; index < input.size(); ++index) {
        if (std::isfinite(input[index]))
            prefix = static_cast<float>(
                prefix + static_cast<float>(input[index]));
        out[index] = static_cast<double>(prefix);
    }

    // The native second pass walks bars backwards.  A positive period only
    // replaces the prefix when the complete window lies at/after the first
    // source value and begins strictly after bar zero.  This makes zero,
    // negative, missing, and insufficient-history periods retain the prefix.
    for (std::size_t reverse = input.size(); reverse > first;) {
        --reverse;
        const auto period = tcalc_period_at(periods, reverse);
        if (!period || *period < 1 ||
            static_cast<std::size_t>(*period) > reverse + 1)
            continue;
        const auto begin = reverse + 1 - static_cast<std::size_t>(*period);
        if (begin < first || begin == 0) continue;

        float aggregate = 0.0F;
        for (std::size_t at = reverse + 1; at-- > begin;) {
            if (std::isfinite(input[at]))
                aggregate = static_cast<float>(
                    static_cast<float>(input[at]) + aggregate);
        }
        out[reverse] = static_cast<double>(aggregate);
    }
    return out;
}

Series tcalc_highest_value(const Series& input, const Series& periods) {
    Series out(input.size(), missing);
    const auto first = first_valid_source(input);
    if (first == input.size()) return out;

    for (std::size_t index = first; index < input.size(); ++index) {
        const auto period = tcalc_extreme_period_at(periods, index);
        if (!period) continue;
        const auto begin = index + 1 - static_cast<std::size_t>(*period);

        bool has_selected = false;
        float selected = 0.0F;
        for (std::size_t at = begin; at <= index; ++at) {
            // In TCalc!sub_10018590 the finite missing sentinel can never
            // replace a normal maximum, while a later normal value replaces
            // an initially missing selection.
            if (!std::isfinite(input[at])) continue;
            const float candidate = static_cast<float>(input[at]);
            const double tolerant_candidate =
                static_cast<double>(candidate) +
                std::abs(static_cast<double>(candidate)) *
                    tcalc_relative_epsilon +
                tcalc_absolute_epsilon;
            if (!has_selected ||
                static_cast<double>(selected) < tolerant_candidate) {
                selected = candidate;
                has_selected = true;
            }
        }
        if (has_selected) out[index] = static_cast<double>(selected);
    }
    return out;
}

Series tcalc_lowest_value(const Series& input, const Series& periods) {
    Series out(input.size(), missing);
    const auto first = first_valid_source(input);
    if (first == input.size()) return out;

    for (std::size_t index = first; index < input.size(); ++index) {
        const auto period = tcalc_extreme_period_at(periods, index);
        if (!period) continue;
        const auto begin = index + 1 - static_cast<std::size_t>(*period);

        bool has_selected = false;
        float selected = 0.0F;
        for (std::size_t at = begin; at <= index; ++at) {
            // TCalc!sub_100194A0 is intentionally asymmetric with HHV: it
            // scans past leading missing values, but a later missing sentinel
            // becomes the selected minimum.  A following finite value restores
            // the selection through the native "selected is missing" branch.
            if (!std::isfinite(input[at])) {
                has_selected = false;
                continue;
            }
            const float candidate = static_cast<float>(input[at]);
            const double tolerant_candidate =
                static_cast<double>(candidate) -
                std::abs(static_cast<double>(candidate)) *
                    tcalc_relative_epsilon -
                tcalc_absolute_epsilon;
            if (!has_selected ||
                static_cast<double>(selected) > tolerant_candidate) {
                selected = candidate;
                has_selected = true;
            }
        }
        if (has_selected) out[index] = static_cast<double>(selected);
    }
    return out;
}

Series tcalc_moving_average(const Series& input, const Series& periods) {
    constexpr float tcalc_missing_sentinel = -4.0398103e34F;
    const auto native_operand = [=](double value) {
        const float narrowed = static_cast<float>(value);
        return std::isfinite(narrowed) ? narrowed : tcalc_missing_sentinel;
    };

    Series out(input.size(), missing);
    const auto first = std::find_if(input.begin(), input.end(), [=](double value) {
        const float narrowed = static_cast<float>(value);
        return std::isfinite(narrowed) && narrowed != tcalc_missing_sentinel;
    });
    if (first == input.end()) return out;

    const auto first_index = static_cast<std::size_t>(first - input.begin());
    for (std::size_t i = first_index; i < input.size(); ++i) {
        const int n = i < periods.size()
            ? tcalc_external_integer(periods[i])
            : std::numeric_limits<int>::min();
        if (n < 1 || i - first_index + 1 < static_cast<std::size_t>(n))
            continue;

        float aggregate = 0.0f;
        const auto begin = i + 1 - static_cast<std::size_t>(n);
        for (std::size_t at = i + 1; at-- > begin;) {
            const float source = native_operand(input[at]);
            if (source != tcalc_missing_sentinel)
                aggregate = static_cast<float>(
                    aggregate + source);
        }
        const float result = static_cast<float>(
            static_cast<double>(aggregate) / static_cast<double>(n));
        out[i] = result == tcalc_missing_sentinel || !std::isfinite(result)
            ? missing : static_cast<double>(result);
    }
    return out;
}

Series tcalc_exponential_moving_average(const Series& input,
                                        const Series& periods) {
    Series out(input.size(), missing);
    std::size_t first = 0;
    while (first < input.size() && !std::isfinite(input[first])) ++first;
    if (first == input.size()) return out;

    float previous = static_cast<float>(input[first]);
    out[first] = static_cast<double>(previous);
    for (std::size_t i = first + 1; i < input.size(); ++i) {
        // TCalc!sub_1000B0C0 converts the current period to a signed integer
        // on every bar, clamps it to at least one, and never re-seeds merely
        // because that period changes.
        int period = 1;
        if (i < periods.size() && std::isfinite(periods[i])) {
            const float native_period = static_cast<float>(periods[i]);
            if (std::isfinite(native_period) &&
                static_cast<double>(native_period) >=
                    static_cast<double>(std::numeric_limits<int>::min()) &&
                static_cast<double>(native_period) < 2147483648.0) {
                period = static_cast<int>(native_period);
            }
            if (period < 1) period = 1;
        }

        // After the leading validity scan, a missing source carries the
        // preceding valid EMA output instead of opening a new missing gap.
        if (!std::isfinite(input[i]) || !std::isfinite(previous)) {
            if (std::isfinite(previous)) out[i] = static_cast<double>(previous);
            continue;
        }

        const float source = static_cast<float>(input[i]);
        previous = static_cast<float>(
            (static_cast<double>(period - 1) * previous +
             static_cast<double>(source) * 2.0) /
            (static_cast<double>(period) + 1.0));
        out[i] = static_cast<double>(previous);
    }
    return out;
}

Series tcalc_smoothed_moving_average(const Series& input,
                                     const Series& periods,
                                     const Series& weights) {
    Series out(input.size(), missing);
    if (input.empty() || periods.empty() || weights.empty()) return out;

    // TCalc!sub_10007AE0 reads N and M only from the final bar, converts both
    // through native float-to-integer truncation, and rejects the complete
    // series unless N > M and N >= 1.
    const int period = tcalc_external_integer(periods.back());
    const int weight = tcalc_external_integer(weights.back());
    if (period <= weight || period < 1) return out;

    const auto first = first_valid_source(input);
    if (first == input.size()) return out;

    constexpr float tcalc_missing_sentinel = -4.0398103e34F;
    float previous = static_cast<float>(input[first]);
    out[first] = std::isfinite(previous)
        ? static_cast<double>(previous) : missing;

    // The subtraction is an x86 integer operation in TCalc.  Spell out its
    // two's-complement wrap so even an invalid M conversion is deterministic
    // rather than invoking signed-overflow undefined behaviour in C++.
    const std::uint32_t retained_bits =
        static_cast<std::uint32_t>(period) -
        static_cast<std::uint32_t>(weight);
    const std::int64_t retained_integer =
        retained_bits <= static_cast<std::uint32_t>(
                             std::numeric_limits<std::int32_t>::max())
        ? static_cast<std::int64_t>(retained_bits)
        : static_cast<std::int64_t>(retained_bits) - 0x100000000LL;
    const float retained_weight = static_cast<float>(retained_integer);
    const float source_weight = static_cast<float>(weight);
    const float divisor = static_cast<float>(period);

    for (std::size_t index = first + 1; index < input.size(); ++index) {
        // After the leading scan the native loop does not test for missing.
        // Its finite sentinel therefore participates in the recurrence, and
        // each output is narrowed to float before becoming the next state.
        const float source = std::isfinite(input[index])
            ? static_cast<float>(input[index]) : tcalc_missing_sentinel;
        previous = static_cast<float>(
            (static_cast<double>(source) * source_weight +
             static_cast<double>(previous) * retained_weight) /
            divisor);
        out[index] = previous == tcalc_missing_sentinel ||
                     !std::isfinite(previous)
            ? missing : static_cast<double>(previous);
    }
    return out;
}

Series tcalc_seeded_exponential_moving_average(
    const Series& input, const Series& periods,
    TcalcSeededExponentialKind kind) {
    Series out(input.size(), missing);
    if (input.empty() || periods.empty()) return out;

    // TCalc!sub_1000AEF0 (MEMA) and sub_1000B1A0 (EXPMEMA) read N only
    // from the final bar and convert it by truncating a native float.
    const int period = tcalc_external_integer(periods.back());
    if (period < 1) return out;

    const auto first = first_valid_source(input);
    if (first == input.size()) return out;
    const auto available = input.size() - first;
    const auto native_period = static_cast<std::size_t>(period);

    // MEMA requires a bar after its N-value seed; EXPMEMA permits the seed to
    // land on the final bar.  Leading output positions remain missing.
    const bool seed_fits = kind == TcalcSeededExponentialKind::mema
        ? native_period < available : native_period <= available;
    if (!seed_fits) return out;

    constexpr float tcalc_missing_sentinel = -4.0398103e34F;
    float carried_source = static_cast<float>(input[first]);
    float aggregate = 0.0F;
    for (std::size_t offset = 0; offset < native_period; ++offset) {
        const auto index = first + offset;
        if (std::isfinite(input[index]))
            carried_source = static_cast<float>(input[index]);
        // During native seed materialization an internal sentinel is replaced
        // by the preceding source.  Every running-sum assignment lands in f32.
        aggregate = static_cast<float>(
            static_cast<double>(carried_source) + aggregate);
    }

    const auto seed_index = first + native_period - 1;
    float previous = static_cast<float>(
        static_cast<double>(aggregate) / static_cast<double>(period));
    out[seed_index] = previous == tcalc_missing_sentinel ||
                      !std::isfinite(previous)
        ? missing : static_cast<double>(previous);

    for (std::size_t index = seed_index + 1; index < input.size(); ++index) {
        const float source = std::isfinite(input[index])
            ? static_cast<float>(input[index]) : tcalc_missing_sentinel;
        if (previous != tcalc_missing_sentinel &&
            source != tcalc_missing_sentinel) {
            const double retained =
                static_cast<double>(period - 1) * previous;
            const double incoming = kind == TcalcSeededExponentialKind::mema
                ? static_cast<double>(source)
                : static_cast<double>(source) * 2.0;
            const double divisor = kind == TcalcSeededExponentialKind::mema
                ? static_cast<double>(period)
                : static_cast<double>(period) + 1.0;
            previous = static_cast<float>((retained + incoming) / divisor);
        }
        // A post-seed source sentinel carries a valid previous output.  Once
        // the previous state itself is the sentinel, the native branch keeps
        // propagating it until the end of the series.
        out[index] = previous == tcalc_missing_sentinel ||
                     !std::isfinite(previous)
            ? missing : static_cast<double>(previous);
    }
    return out;
}

DirectionalBarSeries tcalc_directional_bars(const Environment& env,
                                             std::size_t size) {
    DirectionalBarSeries result{
        Series(size, missing), Series(size, missing), Series(size, missing),
        Series(size, missing), Series(size, missing)};
    if (!size) return result;

    const auto require_field = [&](std::string_view name) -> const Series& {
        const auto found = env.find(name);
        if (found == env.end() || found->second.size() != size)
            throw Error("directional bars require " + std::string(name) + " series");
        return found->second;
    };
    const auto& open = require_field("OPEN");
    const auto& high = require_field("HIGH");
    const auto& low = require_field("LOW");
    const auto& close = require_field("CLOSE");
    const auto& volume = require_field("VOL");

    struct Segment {
        std::size_t begin{};
        std::size_t end{};
        double open{missing};
        double high{missing};
        double low{missing};
        double close{missing};
        double volume{missing};
    };
    const auto point = [&](std::size_t index) {
        return Segment{index, index, native_float(open[index]),
                       native_float(high[index]), native_float(low[index]),
                       native_float(close[index]), native_float(volume[index])};
    };
    const auto merge = [&](Segment& segment, std::size_t index) {
        const double bar_high = native_float(high[index]);
        const double bar_low = native_float(low[index]);
        segment.end = index;
        if (bar_high >= segment.high) segment.high = bar_high;
        if (bar_low <= segment.low) segment.low = bar_low;
        segment.close = native_float(close[index]);
        segment.volume = native_float(
            native_float(volume[index]) + segment.volume);
    };

    std::vector<Segment> segments;
    segments.reserve(size);
    Segment current = point(0);
    int direction = 0;
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    for (std::size_t index = 1; index < size; ++index) {
        const double current_close = native_float(close[index]);
        const double previous_close = native_float(close[index - 1]);
        const double tolerance = std::abs(current_close) * relative_epsilon +
                                 native_epsilon;
        int next_direction = 0;
        if (previous_close <= current_close - tolerance) next_direction = 1;
        else if (previous_close >= current_close + tolerance) next_direction = -1;

        if (next_direction != 0 && direction == -next_direction) {
            segments.push_back(current);
            current = point(index);
        } else {
            merge(current, index);
        }
        if (next_direction != 0) direction = next_direction;
    }
    segments.push_back(current);

    for (const auto& segment : segments) {
        for (std::size_t index = segment.begin; index <= segment.end; ++index) {
            result.open[index] = segment.open;
            result.high[index] = segment.high;
            result.low[index] = segment.low;
            result.close[index] = segment.close;
            result.volume[index] = segment.volume;
        }
    }
    return result;
}

}  // namespace tdx::formula_engine_detail
