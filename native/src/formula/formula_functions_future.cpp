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

enum class TcalcIncludedDirection {
    backward,
    forward,
};

struct TcalcIncludedRange {
    float upper;
    float lower;
};

constexpr float tcalc_included_missing_sentinel = -4.0398103e34F;

float tcalc_included_operand(double value) {
    if (!std::isfinite(value)) return tcalc_included_missing_sentinel;
    const float narrowed = static_cast<float>(value);
    return std::isfinite(narrowed)
        ? narrowed : tcalc_included_missing_sentinel;
}

int tcalc_included_integer(double value) {
    const float narrowed = tcalc_included_operand(value);
    if (!std::isfinite(narrowed) || narrowed >= 2147483648.0F ||
        narrowed < -2147483648.0F)
        return std::numeric_limits<int>::min();
    return static_cast<int>(std::trunc(narrowed));
}

Series tcalc_included(
    const Series& selectors, const Series& limits,
    const Environment& env, std::size_t size,
    TcalcIncludedDirection direction) {
    Series out(size, 0.0);
    if (!size) return out;

    const int selector = tcalc_included_integer(selectors.back());
    const int limit = tcalc_included_integer(limits.back());
    const auto& high = env.at("HIGH");
    const auto& low = env.at("LOW");
    const auto& open = env.at("OPEN");
    const auto& close = env.at("CLOSE");
    constexpr double tolerance = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;

    const auto range = [&](std::size_t index) {
        if (selector == 0) {
            return TcalcIncludedRange{
                tcalc_included_operand(high[index]),
                tcalc_included_operand(low[index])};
        }
        return TcalcIncludedRange{
            std::max(tcalc_included_operand(open[index]),
                     tcalc_included_operand(close[index])),
            std::min(tcalc_included_operand(open[index]),
                     tcalc_included_operand(close[index]))};
    };
    const auto is_contained_by = [&](std::size_t current,
                                     std::size_t candidate) {
        const auto inner = range(current);
        const auto outer = range(candidate);
        return static_cast<double>(inner.upper) <
                   static_cast<double>(outer.upper) + tolerance &&
               static_cast<double>(inner.lower) + tolerance >
                   static_cast<double>(outer.lower);
    };

    if (selector != 0 && selector != 1) return out;
    if (direction == TcalcIncludedDirection::backward) {
        for (std::size_t current = 1; current < size; ++current) {
            int distance = 0;
            for (std::size_t candidate = current; candidate-- > 0;) {
                ++distance;
                if (out[candidate] != 0.0) {
                    if (limit > 0 && distance >= limit) break;
                    continue;
                }
                if (is_contained_by(current, candidate)) out[current] = 1.0;
                break;
            }
        }
    } else {
        for (std::size_t current = size; current-- > 0;) {
            int distance = 0;
            for (std::size_t candidate = current + 1;
                 candidate < size; ++candidate) {
                ++distance;
                if (out[candidate] != 0.0) {
                    if (limit > 0 && distance >= limit) break;
                    continue;
                }
                if (is_contained_by(current, candidate)) out[current] = 1.0;
                break;
            }
        }
    }
    return out;
}

constexpr float tcalc_zig_missing_sentinel = -4.0398103e34F;
constexpr double tcalc_zig_relative_tolerance =
    tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
constexpr double tcalc_zig_absolute_tolerance =
    tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;

float tcalc_zig_operand(double value) {
    if (!std::isfinite(value)) return tcalc_zig_missing_sentinel;
    const float narrowed = static_cast<float>(value);
    return std::isfinite(narrowed) ? narrowed : tcalc_zig_missing_sentinel;
}

Series tcalc_percentage_zig(
    const Series& source, const Series& thresholds,
    const Environment& env, std::size_t size) {
    Series out(size, 0.0);
    if (!size) return out;

    const float threshold = tcalc_zig_operand(thresholds.back());
    const float threshold_abs = std::fabs(threshold);
    if (static_cast<double>(threshold) -
            static_cast<double>(threshold_abs) * tcalc_zig_relative_tolerance -
            tcalc_zig_absolute_tolerance <
        0.0)
        return out;

    // The first argument can be a literal 0..3 OHLC selector.  TCalc only
    // inspects the final ten adjacent raw-f32 pairs before treating it as a
    // selector; older differences do not change this decision.
    const Series* selected = &source;
    int selector = tcalc_external_integer(tcalc_zig_operand(source.back()));
    std::size_t compared = 0;
    for (std::size_t index = size - 1; index > 0 && compared < 10;
         --index, ++compared) {
        const float prior = tcalc_zig_operand(source[index - 1]);
        const float current = tcalc_zig_operand(source[index]);
        const double difference =
            static_cast<double>(prior) - static_cast<double>(current);
        if (prior == tcalc_zig_missing_sentinel ||
            difference >= tcalc_zig_absolute_tolerance ||
            difference <= -tcalc_zig_absolute_tolerance) {
            selector = 4;
            break;
        }
    }
    const char* selected_name = selector == 0 ? "OPEN" : selector == 1 ? "HIGH" :
                                selector == 2 ? "LOW" : selector == 3 ? "CLOSE" : nullptr;
    if (selected_name) {
        const auto found = env.find(selected_name);
        if (found != env.end()) selected = &found->second;
    }

    std::vector<float> values(size, tcalc_zig_missing_sentinel);
    for (std::size_t index = 0; index < size; ++index)
        values[index] = tcalc_zig_operand((*selected)[index]);
    std::size_t first = 0;
    while (first < size && values[first] == tcalc_zig_missing_sentinel) ++first;
    if (first == size) return out;

    const std::size_t last = size - 1;
    std::vector<std::size_t> pivots{first};
    int candidate = static_cast<int>(first);
    std::size_t index = first + 1;
    while (index < last && candidate == static_cast<int>(first)) {
        const float difference =
            static_cast<float>(values[index] - values[first]);
        if (static_cast<double>(values[first]) *
                static_cast<double>(threshold) <=
            static_cast<double>(std::fabs(difference)) * 100.0)
            candidate = values[first] >= values[index]
                ? -static_cast<int>(index) : static_cast<int>(index);
        ++index;
    }

    const auto percentage_reversal = [&](float pivot,
                                         double scaled_difference) {
        const float narrowed_difference =
            static_cast<float>(scaled_difference);
        return static_cast<double>(pivot) * static_cast<double>(threshold) <
            scaled_difference +
                static_cast<double>(std::fabs(narrowed_difference)) *
                    tcalc_zig_relative_tolerance +
                tcalc_zig_absolute_tolerance;
    };

    for (; index < last; ++index) {
        const float current = values[index];
        const double tolerance =
            static_cast<double>(std::fabs(current)) *
                tcalc_zig_relative_tolerance +
            tcalc_zig_absolute_tolerance;
        const bool higher_neighbor =
            values[index - 1] >= static_cast<double>(current) + tolerance ||
            values[index + 1] >= static_cast<double>(current) + tolerance;
        if (higher_neighbor) {
            if (values[index - 1] > static_cast<double>(current) - tolerance &&
                values[index + 1] > static_cast<double>(current) - tolerance) {
                if (candidate > 0) {
                    const double scaled_difference =
                        (static_cast<double>(values[candidate]) -
                         static_cast<double>(current)) * 100.0;
                    if (percentage_reversal(values[candidate],
                                            scaled_difference)) {
                        const auto pivot = static_cast<std::size_t>(candidate);
                        if (pivot != pivots.back()) pivots.push_back(pivot);
                        candidate = -static_cast<int>(index);
                    }
                } else if (values[-candidate] >=
                           static_cast<double>(current) + tolerance) {
                    candidate = -static_cast<int>(index);
                }
            }
        } else if (candidate >= 0) {
            if (values[candidate] <= static_cast<double>(current) - tolerance)
                candidate = static_cast<int>(index);
        } else {
            const float prior = values[-candidate];
            const double scaled_difference =
                (static_cast<double>(current) - static_cast<double>(prior)) * 100.0;
            if (percentage_reversal(prior, scaled_difference)) {
                const auto pivot = static_cast<std::size_t>(-candidate);
                if (pivot != pivots.back()) pivots.push_back(pivot);
                candidate = static_cast<int>(index);
            }
        }
    }

    // Preserve the native repaint boundary: a candidate already in the final
    // two bars can fold into the last bar; an older unconfirmed extremum is
    // retained and followed by a separately painted final segment.
    if (std::abs(candidate) >= static_cast<int>(size) - 2) {
        const double tolerance =
            static_cast<double>(std::fabs(values[last])) *
                tcalc_zig_relative_tolerance +
            tcalc_zig_absolute_tolerance;
        if (candidate > 0 &&
            values[candidate] < static_cast<double>(values[last]) + tolerance)
            candidate = static_cast<int>(last);
        else if (candidate < 0 &&
                 values[-candidate] > static_cast<double>(values[last]) - tolerance)
            candidate = -static_cast<int>(last);
    }
    const auto candidate_index =
        static_cast<std::size_t>(std::abs(candidate));
    if (candidate_index != pivots.back()) pivots.push_back(candidate_index);
    if (last != pivots.back()) pivots.push_back(last);

    for (std::size_t pivot = 1; pivot < pivots.size(); ++pivot) {
        const auto begin = pivots[pivot - 1], end = pivots[pivot];
        if (end <= begin) continue;
        const float slope = static_cast<float>(
            (values[end] - values[begin]) / static_cast<double>(end - begin));
        for (std::size_t point = begin; point <= end; ++point) {
            const float value = static_cast<float>(
                values[begin] + static_cast<double>(point - begin) * slope);
            out[point] = value == tcalc_zig_missing_sentinel ||
                    !std::isfinite(value)
                ? missing : static_cast<double>(value);
        }
    }
    if (pivots.size() == 1)
        out[first] = values[first] == tcalc_zig_missing_sentinel
            ? missing : static_cast<double>(values[first]);
    return out;
}

enum class TcalcTurningKind {
    peak,
    trough,
};

enum class TcalcTurningOutput {
    value,
    bars,
};

Series tcalc_turning_projection(
    const Series& source, const Series& thresholds, const Series& orders,
    const Environment& env, std::size_t size, TcalcTurningKind kind,
    TcalcTurningOutput output_kind) {
    Series out(size, 0.0);
    if (!size) return out;

    const int order = tcalc_external_integer(orders.back());
    if (order < 1 || static_cast<std::size_t>(order) > size) return out;

    const auto projected = tcalc_percentage_zig(source, thresholds, env, size);
    std::vector<float> zig(size, tcalc_zig_missing_sentinel);
    for (std::size_t index = 0; index < size; ++index)
        zig[index] = tcalc_zig_operand(projected[index]);

    std::size_t first = 0;
    while (first < size && zig[first] == tcalc_zig_missing_sentinel) ++first;
    if (first == size) return out;

    std::size_t cursor = first + 1;
    if (kind == TcalcTurningKind::peak) {
        while (cursor < size) {
            const double tolerance =
                static_cast<double>(std::fabs(zig[cursor])) *
                    tcalc_zig_relative_tolerance +
                tcalc_zig_absolute_tolerance;
            if (static_cast<double>(zig[cursor - 1]) <
                static_cast<double>(zig[cursor]) + tolerance)
                break;
            ++cursor;
        }
        while (cursor < size) {
            const double tolerance =
                static_cast<double>(std::fabs(zig[cursor])) *
                    tcalc_zig_relative_tolerance +
                tcalc_zig_absolute_tolerance;
            if (static_cast<double>(zig[cursor - 1]) >
                static_cast<double>(zig[cursor]) - tolerance)
                break;
            ++cursor;
        }
    } else {
        while (cursor < size) {
            const double tolerance =
                static_cast<double>(std::fabs(zig[cursor])) *
                    tcalc_zig_relative_tolerance +
                tcalc_zig_absolute_tolerance;
            if (static_cast<double>(zig[cursor - 1]) >
                static_cast<double>(zig[cursor]) - tolerance)
                break;
            ++cursor;
        }
        while (cursor < size) {
            const double tolerance =
                static_cast<double>(std::fabs(zig[cursor])) *
                    tcalc_zig_relative_tolerance +
                tcalc_zig_absolute_tolerance;
            if (static_cast<double>(zig[cursor - 1]) <
                static_cast<double>(zig[cursor]) + tolerance)
                break;
            ++cursor;
        }
    }

    std::vector<int> turns(static_cast<std::size_t>(order), 0);
    const auto initial_turn = cursor - 1;
    turns[0] = static_cast<int>(initial_turn);
    bool moving_toward_turn = false;

    const auto emit = [&](std::size_t index) {
        const int selected = turns.back();
        if (!selected) return;
        if (output_kind == TcalcTurningOutput::bars) {
            out[index] = static_cast<double>(static_cast<float>(
                static_cast<int>(index) - selected));
            return;
        }
        const float value = zig[static_cast<std::size_t>(selected)];
        out[index] = value == tcalc_zig_missing_sentinel || !std::isfinite(value)
            ? missing : static_cast<double>(value);
    };

    for (std::size_t index = initial_turn; index + 1 < size; ++index) {
        const double tolerance =
            static_cast<double>(std::fabs(zig[index])) *
                tcalc_zig_relative_tolerance +
            tcalc_zig_absolute_tolerance;
        const bool toward = kind == TcalcTurningKind::peak
            ? static_cast<double>(zig[index + 1]) >
                  static_cast<double>(zig[index]) - tolerance
            : static_cast<double>(zig[index + 1]) <
                  static_cast<double>(zig[index]) + tolerance;
        if (toward) {
            moving_toward_turn = true;
        } else if (moving_toward_turn) {
            if (turns.size() > 1)
                std::copy_backward(turns.begin(), turns.end() - 1, turns.end());
            turns[0] = static_cast<int>(index);
            moving_toward_turn = false;
        }
        emit(index);
    }
    emit(size - 1);
    return out;
}

}  // namespace

std::optional<Series> evaluate_future_shape_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    if (auto evaluated = evaluate_calendar_function(name, args, env, size))
        return std::move(*evaluated);

    if (name == "INCLUDED" || name == "INCLUDEDV") {
        require_arity(name, args, 2, 2);
        const auto high = env.find("HIGH"), low = env.find("LOW"),
                   open = env.find("OPEN"), close = env.find("CLOSE");
        if (high == env.end() || low == env.end() || open == env.end() || close == env.end())
            throw Error(name + " requires OHLC series");
        // Both handlers are read-only future primitives. INCLUDED scans
        // backward, while INCLUDEDV scans forward; their result buffer is
        // also the native skip mask for already-included candidates.
        return tcalc_included(
            args[0], args[1], env, size,
            name == "INCLUDED" ? TcalcIncludedDirection::backward
                               : TcalcIncludedDirection::forward);
    }
    if (name == "ZIG") {
        require_arity(name, args, 2, 2);
        return tcalc_percentage_zig(args[0], args[1], env, size);
    }
    if (name == "ZIGA") {
        require_arity(name, args, 2, 2);
        Series out(size, 0.0);
        if (!size) return out;

        constexpr float tcalc_missing_sentinel = -4.0398103e34f;
        const float threshold = static_cast<float>(args[1].back());
        if (!std::isfinite(threshold) ||
            threshold - std::fabs(threshold) * 1.0e-7f - 1.0e-5f < 0.0f)
            return out;

        // The first argument is either an explicit series or TCalc's 0..3
        // OHLC selector.  The DLL distinguishes a selector by checking that
        // the last ten adjacent values are equal within its fixed tolerance.
        const Series* selected = &args[0];
        int selector = 4;
        if (std::isfinite(args[0].back())) {
            selector = static_cast<int>(static_cast<float>(args[0].back()));
            std::size_t compared = 0;
            for (std::size_t index = size - 1; index > 0 && compared < 10;
                 --index, ++compared) {
                if (!std::isfinite(args[0][index - 1]) ||
                    std::fabs(static_cast<double>(static_cast<float>(args[0][index - 1])) -
                              static_cast<float>(args[0][index])) >= 1.0e-5) {
                    selector = 4;
                    break;
                }
            }
        }
        const char* selected_name = selector == 0 ? "OPEN" : selector == 1 ? "HIGH" :
                                    selector == 2 ? "LOW" : selector == 3 ? "CLOSE" : nullptr;
        if (selected_name) {
            const auto found = env.find(selected_name);
            if (found != env.end()) selected = &found->second;
        }

        std::vector<float> values(size, tcalc_missing_sentinel);
        for (std::size_t index = 0; index < size; ++index)
            if (std::isfinite((*selected)[index]))
                values[index] = static_cast<float>((*selected)[index]);
        std::size_t first = 0;
        while (first < size && values[first] == tcalc_missing_sentinel) ++first;
        if (first == size) return out;

        const std::size_t last = size - 1;
        std::vector<std::size_t> pivots{first};
        int candidate = static_cast<int>(first);
        std::size_t index = first + 1;
        while (index < last && candidate == static_cast<int>(first)) {
            const float difference = static_cast<float>(values[index] - values[first]);
            if (std::fabs(difference) >= threshold)
                candidate = values[first] >= values[index]
                    ? -static_cast<int>(index) : static_cast<int>(index);
            ++index;
        }
        for (; index < last; ++index) {
            const float current = values[index];
            const double tolerance =
                std::fabs(static_cast<double>(current)) * 1.0e-7 + 1.0e-5;
            const bool higher_neighbor =
                values[index - 1] >= current + tolerance ||
                values[index + 1] >= current + tolerance;
            if (higher_neighbor) {
                if (values[index - 1] > current - tolerance &&
                    values[index + 1] > current - tolerance) {
                    if (candidate > 0) {
                        const float difference =
                            static_cast<float>(values[candidate] - current);
                        if (difference + std::fabs(difference) * 1.0e-7f +
                                1.0e-5f > threshold) {
                            const auto pivot = static_cast<std::size_t>(candidate);
                            if (pivot != pivots.back()) pivots.push_back(pivot);
                            candidate = -static_cast<int>(index);
                        }
                    } else if (values[-candidate] >= current + tolerance) {
                        candidate = -static_cast<int>(index);
                    }
                }
            } else if (candidate >= 0) {
                if (values[candidate] <= current - tolerance)
                    candidate = static_cast<int>(index);
            } else {
                const float difference =
                    static_cast<float>(current - values[-candidate]);
                if (difference + std::fabs(difference) * 1.0e-7f +
                        1.0e-5f > threshold) {
                    const auto pivot = static_cast<std::size_t>(-candidate);
                    if (pivot != pivots.back()) pivots.push_back(pivot);
                    candidate = static_cast<int>(index);
                }
            }
        }

        // The DLL only folds the last bar into the active candidate when the
        // candidate is already one of the final two bars.  Otherwise it keeps
        // that unconfirmed local extremum and paints a second final segment.
        if (std::abs(candidate) >= static_cast<int>(size) - 2) {
            const double tolerance =
                std::fabs(static_cast<double>(values[last])) * 1.0e-7 + 1.0e-5;
            if (candidate > 0 && values[candidate] < values[last] + tolerance)
                candidate = static_cast<int>(last);
            else if (candidate < 0 && values[-candidate] > values[last] - tolerance)
                candidate = -static_cast<int>(last);
        }
        const auto candidate_index =
            static_cast<std::size_t>(std::abs(candidate));
        if (candidate_index != pivots.back()) pivots.push_back(candidate_index);
        if (last != pivots.back()) pivots.push_back(last);

        for (std::size_t pivot = 1; pivot < pivots.size(); ++pivot) {
            const auto begin = pivots[pivot - 1], end = pivots[pivot];
            if (end <= begin) continue;
            const float slope = static_cast<float>(
                (values[end] - values[begin]) / static_cast<double>(end - begin));
            for (std::size_t point = begin; point <= end; ++point) {
                const float value = static_cast<float>(
                    values[begin] + static_cast<double>(point - begin) * slope);
                out[point] = value == tcalc_missing_sentinel || !std::isfinite(value)
                    ? missing : static_cast<double>(value);
            }
        }
        if (pivots.size() == 1)
            out[first] = values[first] == tcalc_missing_sentinel
                ? missing : static_cast<double>(values[first]);
        return out;
    }
    if (name == "PEAK" || name == "TROUGH" || name == "PEAKBARS" ||
        name == "TROUGHBARS") {
        require_arity(name, args, 3, 3);
        const bool peak = name == "PEAK" || name == "PEAKBARS";
        const bool bars = name == "PEAKBARS" || name == "TROUGHBARS";
        return tcalc_turning_projection(
            args[0], args[1], args[2], env, size,
            peak ? TcalcTurningKind::peak : TcalcTurningKind::trough,
            bars ? TcalcTurningOutput::bars : TcalcTurningOutput::value);
    }
    return std::nullopt;
}
}  // namespace tdx::formula_engine_detail
