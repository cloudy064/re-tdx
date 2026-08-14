#include "formula_tdx_indicators_internal.hpp"
#include "formula_native_constants.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>

namespace tdx::formula_engine_detail {

Series evaluate_tdx_band_volume_indicator(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    if (name == "TDXBB" || name == "TDXWIDTH") {
        require_arity(name, args, 2, 2);
        const auto close = env.find("CLOSE");
        if (close == env.end()) throw Error(name + " requires the CLOSE series");
        const int smoothing = period_at(args[0], size - 1, 1);
        const int selector = period_at(args[1], size - 1);
        if (selector < 0 || selector > 1) throw Error(name + " selector must be 0 or 1");
        const auto twenty = constant(20.0, size);
        const auto middle = evaluate_call("TDXBOLLM", {twenty, constant(0.0, size)}, env, size);
        const auto deviation = evaluate_call("TDXBOLLM", {twenty, constant(3.0, size)}, env, size);
        Series value(size, 0.0), average(size, 0.0);
        for (std::size_t i = 39; i < size; ++i) {
            if (!std::isfinite(deviation[i]) || std::abs(deviation[i]) <= 1e-30 ||
                (name == "TDXWIDTH" && std::abs(middle[i]) <= 1e-30)) {
                value[i] = missing;
                continue;
            }
            value[i] = name == "TDXBB"
                ? native_float((native_float(close->second[i]) - middle[i] + deviation[i]) *
                               100.0 / deviation[i] * 0.25)
                : native_float(deviation[i] * 4.0 / middle[i]);
        }
        if (size) average[0] = value[0];
        const double divisor = static_cast<double>(smoothing);
        const double retained = static_cast<double>(smoothing - 1);
        for (std::size_t i = 1; i < size; ++i)
            average[i] = native_float((average[i - 1] * retained + value[i]) / divisor);
        return selector == 0 ? value : average;
    }
    if (name == "TDXBOLLM") {
        require_arity(name, args, 2, 2);
        const auto close = env.find("CLOSE");
        if (close == env.end()) throw Error("TDXBOLLM requires the CLOSE series");
        const int window = period_at(args[0], size - 1, 1);
        const int selector = period_at(args[1], size - 1);
        if (selector < 0 || selector > 3) throw Error("TDXBOLLM selector must be 0..3");
        Series middle(size, 0.0), upper(size, 0.0), lower(size, 0.0), deviation(size, 0.0);
        for (std::size_t i = 0; i < size; ++i) middle[i] = native_float(close->second[i]);
        if (window <= static_cast<int>(size)) {
            for (std::size_t i = static_cast<std::size_t>(window - 1); i < size; ++i) {
                double sum = 0.0;
                const auto begin = i + 1 - static_cast<std::size_t>(window);
                for (std::size_t at = begin; at <= i; ++at)
                    sum += native_float(close->second[at]);
                middle[i] = native_float(sum / static_cast<double>(window));
            }
            for (std::size_t i = size; i-- > 1;) middle[i] = middle[i - 1];
            const auto first = static_cast<std::size_t>(2 * window - 1);
            for (std::size_t i = first; i < size; ++i) {
                double square_sum = 0.0;
                const auto begin = i - static_cast<std::size_t>(window);
                for (std::size_t at = begin; at < i; ++at) {
                    const double difference = native_float(close->second[at]) - middle[at + 1];
                    square_sum = native_float(difference * difference + square_sum);
                }
                deviation[i] = native_float(std::sqrt(native_float(
                    square_sum / static_cast<double>(window))));
                upper[i] = native_float(middle[i] + 2.0 * deviation[i]);
                lower[i] = native_float(middle[i] - 2.0 * deviation[i]);
            }
        }
        return selector == 0 ? middle : selector == 1 ? upper :
               selector == 2 ? lower : deviation;
    }
    if (name == "TDXNVI" || name == "TDXPVI") {
        require_arity(name, args, 2, 2);
        const auto close = env.find("CLOSE"), volume = env.find("VOL");
        if (close == env.end() || volume == env.end())
            throw Error(name + " requires CLOSE and VOL series");
        const int average_period = period_at(args[0], size - 1, 1);
        const int selector = period_at(args[1], size - 1);
        if (selector < 0 || selector > 1) throw Error(name + " selector must be 0 or 1");
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        Series index(size, 0.0), average(size, 0.0);
        if (!size) return selector == 0 ? index : average;
        index[0] = 100.0;
        for (std::size_t i = 1; i < size; ++i) {
            const double previous_volume = native_float(volume->second[i - 1]);
            const double current_volume = native_float(volume->second[i]);
            const double tolerance = std::abs(current_volume) * relative_epsilon + native_epsilon;
            const bool active = name == "TDXNVI"
                ? previous_volume >= current_volume + tolerance
                : previous_volume <= current_volume - tolerance;
            const double previous_close = native_float(close->second[i - 1]);
            if (active && (previous_close >= native_epsilon || previous_close <= -native_epsilon))
                index[i] = native_float(index[i - 1] * native_float(close->second[i]) /
                                        previous_close);
            else
                index[i] = index[i - 1];
        }
        average[0] = index[0];
        const double divisor = static_cast<double>(average_period);
        const double retained = static_cast<double>(average_period - 1);
        for (std::size_t i = 1; i < size; ++i)
            average[i] = native_float((average[i - 1] * retained + index[i]) / divisor);
        return selector == 0 ? index : average;
    }
    if (name == "TDXKDJ") {
        require_arity(name, args, 3, 3);
        const auto high = env.find("HIGH"), low = env.find("LOW"), close = env.find("CLOSE");
        if (high == env.end() || low == env.end() || close == env.end())
            throw Error("TDXKDJ requires HIGH, LOW and CLOSE series");
        const int window = period_at(args[0], size - 1, 2);
        const int smoothing = period_at(args[1], size - 1, 1);
        const int selector = period_at(args[2], size - 1);
        if (selector < 0 || selector > 2) throw Error("TDXKDJ selector must be 0, 1 or 2");
        Series k(size, 0.0), d(size, 0.0), j(size, 0.0), rsv(size, 50.0);
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        for (std::size_t i = 0; i < size; ++i) {
            const std::size_t begin = i + 1 > static_cast<std::size_t>(window)
                ? i + 1 - static_cast<std::size_t>(window) : 0;
            double highest = native_float(high->second[begin]);
            double lowest = native_float(low->second[begin]);
            for (std::size_t at = begin + 1; at <= i; ++at) {
                const double candidate_high = native_float(high->second[at]);
                const double candidate_low = native_float(low->second[at]);
                if (candidate_high >= highest + std::abs(candidate_high) * relative_epsilon +
                                          native_epsilon)
                    highest = candidate_high;
                if (candidate_low <= lowest - std::abs(candidate_low) * relative_epsilon -
                                        native_epsilon)
                    lowest = candidate_low;
            }
            double range = highest - lowest;
            if (range >= native_epsilon || range <= -native_epsilon) {
                range = std::max(range, native_epsilon);
                rsv[i] = native_float((native_float(close->second[i]) - lowest) /
                                      range * 100.0);
            }
        }
        if (window <= static_cast<int>(size) + 1) {
            const auto seed = static_cast<std::size_t>(window - 2);
            if (seed < size) {
                k[seed] = d[seed] = j[seed] = 50.0;
                const double divisor = static_cast<double>(smoothing);
                const double retained = static_cast<double>(smoothing - 1);
                for (std::size_t i = seed + 1; i < size; ++i) {
                    k[i] = native_float((k[i - 1] * retained + rsv[i]) / divisor);
                    d[i] = native_float((k[i] + d[i - 1] * retained) / divisor);
                    j[i] = native_float(divisor * k[i] - retained * d[i]);
                }
            }
        }
        return selector == 0 ? k : selector == 1 ? d : j;
    }
    throw Error("unregistered TDX band/volume indicator: " + name);
}

}  // namespace tdx::formula_engine_detail

