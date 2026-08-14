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
#include <vector>

namespace tdx::formula_engine_detail {

namespace {

constexpr float tcalc_missing_sentinel = -4.0398103e34F;
constexpr float tcalc_backset_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon_f;

float tcalc_backset_operand(double value) {
    return std::isfinite(value) ? static_cast<float>(value)
                                : tcalc_missing_sentinel;
}

Series tcalc_backset(const Series& source, const Series& periods) {
    Series out(source.size(), missing);

    // TCalc!sub_10017550 uses sub_10001280 to skip only the leading native
    // missing sentinel.  Once evaluation starts, a later sentinel is an
    // ordinary, large non-zero float and therefore fires BACKSET.
    std::size_t first = 0;
    while (first < source.size() &&
           tcalc_backset_operand(source[first]) == tcalc_missing_sentinel)
        ++first;
    if (first == source.size()) return out;
    std::fill(out.begin() + static_cast<std::ptrdiff_t>(first), out.end(), 0.0);

    // The native handler walks backward, converts each N through float and
    // __ftol2_sse, clamps it to at least one, and clips the write at `first`.
    // tcalc_external_integer makes the out-of-range/sentinel result explicit
    // instead of relying on an undefined C++ floating-to-integer conversion.
    for (std::size_t next = source.size(); next > first; --next) {
        const std::size_t index = next - 1;
        if (!(std::fabs(tcalc_backset_operand(source[index])) >
              tcalc_backset_epsilon))
            continue;

        const int converted = tcalc_external_integer(
            static_cast<double>(tcalc_backset_operand(periods[index])));
        const int native_period = std::max(1, converted);
        const auto count = std::min<std::size_t>(
            static_cast<std::size_t>(native_period), index - first + 1);
        const auto begin = index + 1 - count;
        std::fill(out.begin() + static_cast<std::ptrdiff_t>(begin),
                  out.begin() + static_cast<std::ptrdiff_t>(index + 1), 1.0);
    }
    return out;
}

enum class TcalcForwardReferenceKind { refx, refxv };

float tcalc_forward_reference_operand(double value) {
    return std::isfinite(value) ? static_cast<float>(value)
                                : tcalc_missing_sentinel;
}

double tcalc_forward_reference_output(float value) {
    return value == tcalc_missing_sentinel || !std::isfinite(value)
        ? missing : static_cast<double>(value);
}

float tcalc_reference_operand(double value) {
    if (!std::isfinite(value)) return tcalc_missing_sentinel;
    const float narrowed = static_cast<float>(value);
    return std::isfinite(narrowed) ? narrowed : tcalc_missing_sentinel;
}

Series tcalc_reference(const Series& source, const Series& offsets) {
    const auto size = std::min(source.size(), offsets.size());
    Series out(size, missing);
    std::vector<float> native_out(size, tcalc_missing_sentinel);
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

    // TCalc!sub_10019DF0 starts only when both raw-f32 operands differ from
    // the canonical sentinel.  A selected sentinel remains native missing.
    std::size_t first = 0;
    while (first < size &&
           (tcalc_reference_operand(source[first]) ==
                tcalc_missing_sentinel ||
            tcalc_reference_operand(offsets[first]) ==
                tcalc_missing_sentinel))
        ++first;

    for (std::size_t index = first; index < size; ++index) {
        const float offset = tcalc_reference_operand(offsets[index]);
        const float magnitude = static_cast<float>(std::fabs(offset));
        const double tolerance = static_cast<double>(magnitude) *
                                     relative_epsilon +
                                 absolute_epsilon;
        const bool invalid = static_cast<double>(offset) + tolerance <= 0.0 ||
            static_cast<double>(index) <=
                static_cast<double>(offset) - tolerance;

        float selected = tcalc_missing_sentinel;
        if (invalid) {
            if (index > 0) selected = native_out[index - 1];
        } else {
            const int displacement = tcalc_external_integer(offset);
            if (displacement >= 0 &&
                static_cast<std::size_t>(displacement) <= index)
                selected = tcalc_reference_operand(
                    source[index - static_cast<std::size_t>(displacement)]);
        }
        native_out[index] = selected;
        out[index] = tcalc_forward_reference_output(selected);
    }
    return out;
}

Series tcalc_forward_reference(const Series& source, const Series& offsets,
                               TcalcForwardReferenceKind kind) {
    const auto size = std::min(source.size(), offsets.size());
    Series out(size, missing);
    std::vector<float> native_out(size, tcalc_missing_sentinel);
    constexpr double absolute_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;

    // TCalc!sub_10019F90/sub_1001A300 start only when source and offset are
    // both non-sentinel. Later sentinels remain ordinary raw float operands.
    std::size_t first = 0;
    while (first < size &&
           (tcalc_forward_reference_operand(source[first]) ==
                tcalc_missing_sentinel ||
            tcalc_forward_reference_operand(offsets[first]) ==
                tcalc_missing_sentinel))
        ++first;

    for (std::size_t index = first; index < size; ++index) {
        const float offset = tcalc_forward_reference_operand(offsets[index]);
        const float offset_magnitude =
            static_cast<float>(std::fabs(offset));
        bool invalid = static_cast<double>(offset_magnitude) *
                           relative_epsilon +
                           static_cast<double>(offset) + absolute_epsilon <=
                       0.0;

        double wide_target = 0.0;
        if (!invalid) {
            wide_target = static_cast<double>(index) +
                          static_cast<double>(offset);
            const float target = static_cast<float>(wide_target);
            const float target_magnitude =
                static_cast<float>(std::fabs(target));
            invalid = static_cast<double>(size) <
                wide_target +
                    static_cast<double>(target_magnitude) *
                        relative_epsilon +
                    absolute_epsilon;
        }

        float selected = tcalc_missing_sentinel;
        if (!invalid) {
            const int displacement = tcalc_external_integer(offset);
            const auto target = static_cast<std::ptrdiff_t>(index) +
                                static_cast<std::ptrdiff_t>(displacement);
            if (target >= 0 && target < static_cast<std::ptrdiff_t>(size))
                selected = tcalc_forward_reference_operand(
                    source[static_cast<std::size_t>(target)]);
        } else if (kind == TcalcForwardReferenceKind::refxv) {
            selected = index == 0
                ? tcalc_forward_reference_operand(source[index])
                : native_out[index - 1];
        }

        native_out[index] = selected;
        out[index] = tcalc_forward_reference_output(selected);
    }
    return out;
}

float tcalc_bars_next_operand(double value) {
    if (!std::isfinite(value)) return tcalc_missing_sentinel;
    const float narrowed = static_cast<float>(value);
    return std::isfinite(narrowed) ? narrowed : tcalc_missing_sentinel;
}

bool tcalc_bars_next_true(double value) {
    constexpr double epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    const float operand = tcalc_bars_next_operand(value);
    return operand != tcalc_missing_sentinel &&
           (static_cast<double>(operand) >= epsilon ||
            static_cast<double>(operand) <= -epsilon);
}

Series tcalc_bars_next(const Series& source) {
    Series out(source.size(), missing);

    // TCalc!sub_10006CB0 first finds the rightmost native-true source bar.
    // The preinitialized output after it is untouched and remains missing.
    std::size_t last_true = source.size();
    for (std::size_t cursor = source.size(); cursor-- > 0;) {
        if (tcalc_bars_next_true(source[cursor])) {
            last_true = cursor;
            break;
        }
    }
    if (last_true == source.size()) return out;

    // From that bar backward, every true bar resets the distance to zero;
    // sentinel and values strictly inside the epsilon band count as false.
    int distance = 0;
    for (std::size_t cursor = last_true + 1; cursor-- > 0;) {
        if (tcalc_bars_next_true(source[cursor])) distance = 0;
        out[cursor] = native_float(static_cast<double>(distance));
        ++distance;
    }
    return out;
}

float tcalc_refdate_operand(double value) {
    if (!std::isfinite(value)) return tcalc_missing_sentinel;
    const float narrowed = static_cast<float>(value);
    return std::isfinite(narrowed) ? narrowed : tcalc_missing_sentinel;
}

float tcalc_bars_since_operand(double value) {
    if (!std::isfinite(value)) return tcalc_missing_sentinel;
    const float narrowed = static_cast<float>(value);
    return std::isfinite(narrowed) ? narrowed : tcalc_missing_sentinel;
}

double tcalc_refdate_output(float value) {
    return value == tcalc_missing_sentinel || !std::isfinite(value)
        ? missing : static_cast<double>(value);
}

Series tcalc_refdate(const Series& source, const Series& targets,
                     const Environment& env, std::size_t size) {
    Series out(size, missing);
    if (!size) return out;
    const auto dates = env.find("DATE");
    if (dates == env.end() || dates->second.size() != size) return out;

    // TCalc!sub_100074F0 reads only the final target through raw float and
    // __ftol2_sse.  The subsequent comparison is unsigned, so a missing or
    // out-of-range target becomes INT_MIN/0x80000000 and selects the latest
    // ordinary date instead of leaving the output missing.
    const auto target = static_cast<std::uint32_t>(
        tcalc_external_integer(tcalc_refdate_operand(targets.back())));
    for (std::size_t cursor = size; cursor-- > 0;) {
        const double date = dates->second[cursor];
        if (!std::isfinite(date) || date < 0.0 ||
            date > static_cast<double>(
                       std::numeric_limits<std::uint32_t>::max()))
            continue;
        if (target < static_cast<std::uint32_t>(date)) continue;

        // The selected source is loaded and stored as a raw dword float, then
        // broadcast to every output bar.  Preserve that landing point and map
        // only the canonical sentinel/non-finite result back to missing.
        const double selected = tcalc_refdate_output(
            tcalc_refdate_operand(source[cursor]));
        std::fill(out.begin(), out.end(), selected);
        break;
    }
    return out;
}

}  // namespace

std::optional<Series> evaluate_reference_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    if (name == "XMA") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        // TCalc!sub_1000B370 uses offsets [-N/2, ceil(N/2)) and averages
        // only the available valid portion at the two data boundaries.
        for (std::size_t i = 0; i < size; ++i) {
            const int window = period_at(args[1], i);
            if (window < 1 || !std::isfinite(args[0][i])) continue;
            const int first_offset = -(window / 2);
            const int end_offset = window % 2 ? window / 2 + 1 : window / 2;
            double sum = 0.0;
            int count = 0;
            for (int offset = first_offset; offset < end_offset; ++offset) {
                const auto at = static_cast<std::ptrdiff_t>(i) + offset;
                if (at < 0 || at >= static_cast<std::ptrdiff_t>(size) ||
                    !std::isfinite(args[0][static_cast<std::size_t>(at)]))
                    continue;
                sum = native_float(sum +
                    native_float(args[0][static_cast<std::size_t>(at)]));
                ++count;
            }
            if (count) out[i] = native_float(sum / static_cast<double>(count));
        }
        return out;
    }
    if (name == "REF") {
        require_arity(name, args, 2, 2);
        return tcalc_reference(args[0], args[1]);
    }
    if (name == "REFV") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        for (std::size_t i = 0; i < size; ++i) {
            if (!std::isfinite(args[0][i]) || !std::isfinite(args[1][i]))
                continue;
            const double offset = native_float(args[1][i]);
            const double tolerance = std::abs(offset) * relative_epsilon +
                                     native_epsilon;
            if (offset + tolerance <= 0.0 ||
                static_cast<double>(i) <= offset - tolerance) {
                out[i] = i ? out[i - 1] : native_float(args[0][i]);
                continue;
            }
            const int bars = static_cast<int>(offset);
            if (bars >= 0 && static_cast<std::size_t>(bars) <= i)
                out[i] = native_float(args[0][i - static_cast<std::size_t>(bars)]);
        }
        return out;
    }
    if (name == "BACKSET") {
        require_arity(name, args, 2, 2);
        return tcalc_backset(args[0], args[1]);
    }
    if (name == "REFX" || name == "REFXV") {
        require_arity(name, args, 2, 2);
        return tcalc_forward_reference(
            args[0], args[1], name == "REFX"
                ? TcalcForwardReferenceKind::refx
                : TcalcForwardReferenceKind::refxv);
    }
    if (name == "BARSNEXT") {
        require_arity(name, args, 1, 1);
        return tcalc_bars_next(args[0]);
    }
    if (name == "REFDATE") {
        require_arity(name, args, 2, 2);
        return tcalc_refdate(args[0], args[1], env, size);
    }
    if (name == "BARSSINCE") {
        require_arity(name, args, 1, 1); Series out(size, missing);
        std::size_t first = size;
        for (std::size_t i = 0; i < size; ++i) {
            const float operand = tcalc_bars_since_operand(args[0][i]);
            if (first == size && operand != tcalc_missing_sentinel &&
                operand != 0.0F)
                first = i;
            if (first != size) out[i] = static_cast<double>(i - first);
        }
        return out;
    }
    if (name == "BARSLASTS") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        for (std::size_t i = 0; i < size; ++i) {
            if (!std::isfinite(args[1][i])) continue;
            const double target = args[1][i];
            int occurrences = 0;
            std::size_t found = size;
            for (std::size_t cursor = i + 1; cursor-- > 0;) {
                if (!std::isfinite(args[0][cursor]) || args[0][cursor] == 0.0)
                    continue;
                ++occurrences;
                found = cursor;
                if (target == static_cast<double>(occurrences)) break;
            }
            if (found != size) out[i] = static_cast<double>(i - found);
        }
        return out;
    }
    if (name == "BARSSINCEN") {
        require_arity(name, args, 2, 2);
        Series out(size, missing);
        if (!size || !std::isfinite(args[1].back())) return out;
        const int window = static_cast<int>(args[1].back());
        if (window < 1) return out;
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        for (std::size_t i = 0; i < size; ++i) {
            const auto count = static_cast<std::size_t>(window);
            const auto begin = i + 1 > count ? i + 1 - count : 0;
            for (std::size_t cursor = begin; cursor <= i; ++cursor) {
                if (std::isfinite(args[0][cursor]) &&
                    (args[0][cursor] >= native_epsilon ||
                     args[0][cursor] <= -native_epsilon)) {
                    out[i] = static_cast<double>(i - cursor);
                    break;
                }
            }
        }
        return out;
    }
    return std::nullopt;
}
}  // namespace tdx::formula_engine_detail
