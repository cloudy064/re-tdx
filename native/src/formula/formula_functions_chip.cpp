#include "formula_function_dispatch_internal.hpp"
#include "formula_native_constants.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace tdx::formula_engine_detail {
namespace {

using FunctionHandler = Series (*)(const std::string&,
                                   const std::vector<Series>&,
                                   const Environment&, std::size_t);

Series chip_distribution_call(const std::string& name,
                              const std::vector<Series>& args,
                              const Environment& env, std::size_t size) {
    const bool two_arguments =
        name == "COSTEX" || name == "PWINNER" || name == "LWINNER";
    require_arity(name, args, two_arguments ? 2 : 1,
                  two_arguments ? 2 : 1);
    Series out(size, 0.0);
    if (size == 0) return out;

    const auto high = env.find("HIGH");
    const auto low = env.find("LOW");
    const auto volume = env.find("VOL");
    const auto capital = env.find("CAPITAL");
    if (high == env.end() || low == env.end() || volume == env.end())
        throw Error(name + " requires HIGH, LOW and VOL series");
    if (capital == env.end())
        throw Error(name + " requires the historical CAPITAL context series");
    if (const auto period = env.find("PERIOD");
        period != env.end() && !period->second.empty() &&
        static_cast<int>(period->second.back()) != 5)
        throw Error(name + " is only valid for the daily analysis period");

    const auto supported = env.find("__CHIP_SUPPORTED");
    if (supported != env.end() && !supported->second.empty() &&
        !truth(supported->second.back()))
        return out;

    constexpr double native_rounding = tdx::formula_engine_detail::tcalc_constants::integer_bias;
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr std::size_t native_work_limit = 50000000;

    double global_low = native_float(low->second.front());
    double global_high = native_float(high->second.front());
    for (std::size_t i = 1; i < size; ++i) {
        const auto bar_low = native_float(low->second[i]);
        const auto bar_high = native_float(high->second[i]);
        if (bar_low <= global_low) global_low = bar_low;
        if (bar_high >= global_high) global_high = bar_high;
    }
    if (!std::isfinite(global_low) || !std::isfinite(global_high) ||
        global_low > 10000.0 || global_high > 10000.0)
        return out;

    const int lower_price = static_cast<int>(
        global_low * 100.0 + 1.0 + native_rounding);
    const int upper_price = static_cast<int>(
        global_high * 100.0 - 1.0 + native_rounding);
    const long long signed_bins =
        static_cast<long long>(upper_price) - lower_price + 1;
    if (signed_bins < 1) return out;
    const auto bins = static_cast<std::size_t>(signed_bins);
    std::size_t begin = 0;
    if (bins > native_work_limit / size)
        begin = size - std::min(size, native_work_limit / bins);

    double percentile = 0.0;
    if (name == "COST") {
        percentile = native_float(args.front().back()) / 100.0;
        if (percentile < native_epsilon ||
            percentile > 1.000010013580322)
            return out;
    }

    const auto apply_bar = [&](std::vector<double>& distribution,
                               std::size_t i) {
        double divisor = 1.0;
        if (const auto found = env.find("__CHIP_CAPITAL_DIVISOR");
            found != env.end() && i < found->second.size() &&
            std::isfinite(found->second[i]) && found->second[i] > 0.0)
            divisor = found->second[i];
        const double bar_capital = native_float(capital->second[i]);
        if (!(bar_capital >= 0.0000099999997)) return false;
        const double bar_volume = native_float(volume->second[i]);
        const double turnover = bar_volume / (bar_capital / divisor);
        const double retained = 1.0 - turnover;
        for (auto& weight : distribution) weight *= retained;

        const auto clamp_price = [&](double price) {
            const int rounded = static_cast<int>(
                native_float(price) * 100.0 + native_rounding);
            return std::max(lower_price, std::min(upper_price, rounded)) -
                   lower_price;
        };
        const int bar_low = clamp_price(low->second[i]);
        const int bar_high = clamp_price(high->second[i]);
        if (bar_low >= 0 && bar_high >= bar_low &&
            static_cast<std::size_t>(bar_high) < bins) {
            const int middle = static_cast<int>(
                (bar_low + bar_high) * 0.5 + native_rounding);
            if (middle == bar_low || middle == bar_high) {
                distribution[static_cast<std::size_t>(middle)] += bar_volume;
            } else {
                const double left_width =
                    static_cast<double>(middle - bar_low);
                const double peak = bar_volume / left_width;
                const double rising_step = peak / left_width;
                for (int price = bar_low; price < middle; ++price)
                    distribution[static_cast<std::size_t>(price)] +=
                        static_cast<double>(price - bar_low) * rising_step;
                const double falling_step =
                    peak / static_cast<double>(middle - bar_high);
                for (int price = middle; price <= bar_high; ++price)
                    distribution[static_cast<std::size_t>(price)] +=
                        static_cast<double>(price - bar_high) * falling_step;
            }
        }
        return true;
    };

    const auto winner_value = [&](const std::vector<double>& distribution,
                                  double target, double fallback) {
        double total = 0.0;
        for (const auto weight : distribution) total += weight;
        if (total <= native_epsilon) return fallback;
        const int target_bin = std::min<int>(
            static_cast<int>(native_float(target) * 100.0 + native_rounding) -
                lower_price,
            static_cast<int>(bins) - 1);
        double profitable = 0.0;
        for (int price = 0; price <= target_bin; ++price)
            if (price >= 0)
                profitable += distribution[static_cast<std::size_t>(price)];
        return native_float(profitable / total);
    };

    const auto cumulative_cost =
        [&](const std::vector<double>& distribution, double target) {
            const int target_bin = std::min<int>(
                static_cast<int>(native_float(target) * 100.0 +
                                 native_rounding) -
                    lower_price,
                static_cast<int>(bins) - 1);
            double weight = 0.0;
            double cost = 0.0;
            for (int price = 0; price <= target_bin; ++price) {
                if (price < 0) continue;
                const double item =
                    distribution[static_cast<std::size_t>(price)];
                weight += item;
                cost += static_cast<double>(lower_price + price) * 0.01 * item;
            }
            return std::pair<double, double>{weight, cost};
        };

    if (name == "LWINNER") {
        const int window = static_cast<int>(native_float(args.front().back()));
        if (window <= 0 || static_cast<std::size_t>(window) > size) return out;
        const std::size_t first_output =
            std::max(begin, static_cast<std::size_t>(window - 1));
        std::vector<double> distribution(bins, 0.0);
        for (std::size_t i = first_output; i < size; ++i) {
            std::fill(distribution.begin(), distribution.end(), 0.0);
            const auto first_bar =
                i + 1 - static_cast<std::size_t>(window);
            for (std::size_t bar = first_bar; bar <= i; ++bar)
                apply_bar(distribution, bar);
            out[i] = winner_value(
                distribution, args[1][i], i ? out[i - 1] : 0.0);
        }
        return out;
    }

    int lag = 0;
    std::size_t processing_end = size;
    if (name == "PWINNER") {
        lag = static_cast<int>(native_float(args.front().back()));
        if (lag < 0 || static_cast<std::size_t>(lag) >= size) return out;
        processing_end = size - static_cast<std::size_t>(lag);
    }

    std::vector<double> distribution(bins, 0.0);
    for (std::size_t i = begin; i < processing_end; ++i) {
        if (!apply_bar(distribution, i)) continue;
        if (name == "WINNER") {
            out[i] = winner_value(
                distribution, args.front()[i], i ? out[i - 1] : 0.0);
            continue;
        }
        if (name == "PWINNER") {
            const std::size_t output_index =
                i + static_cast<std::size_t>(lag);
            out[output_index] = winner_value(
                distribution, args[1][output_index],
                output_index ? out[output_index - 1] : 0.0);
            continue;
        }
        if (name == "COSTEX") {
            const auto first = cumulative_cost(distribution, args[0][i]);
            const auto second = cumulative_cost(distribution, args[1][i]);
            const double cost_difference = first.second - second.second;
            if (std::abs(cost_difference) <= native_epsilon) {
                if (i) out[i] = out[i - 1];
            } else {
                out[i] = native_float(
                    cost_difference / (first.first - second.first));
            }
            continue;
        }

        double total = 0.0;
        for (const auto weight : distribution) total += weight;
        const double threshold = total * percentile - native_epsilon;
        double cumulative = 0.0;
        for (std::size_t price = 0; price < bins; ++price) {
            cumulative += distribution[price];
            if (threshold < cumulative) {
                out[i] = native_float(
                    (global_high - global_low) *
                        static_cast<double>(price) /
                        static_cast<double>(bins) +
                    global_low);
                break;
            }
        }
    }
    return out;
}

Series chip_retained_part_call(const std::string& name,
                               const std::vector<Series>& args,
                               const Environment& env, std::size_t size) {
    require_arity(name, args, 1, 1);
    Series out(size, 0.0);
    if (size == 0) return out;
    const auto volume = env.find("VOL");
    const auto capital = env.find("CAPITAL");
    if (volume == env.end()) throw Error("PPART requires the VOL series");
    if (capital == env.end())
        throw Error("PPART requires the historical CAPITAL context series");
    const auto supported = env.find("__CHIP_SUPPORTED");
    if (supported != env.end() && !supported->second.empty() &&
        !truth(supported->second.back()))
        return out;
    const int periods =
        static_cast<int>(native_float(args.front().back()));
    if (periods < 0 || static_cast<std::size_t>(periods) >= size) return out;
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    for (std::size_t i = static_cast<std::size_t>(periods); i < size; ++i) {
        double retained = 1.0;
        bool valid = true;
        const std::size_t first =
            i - static_cast<std::size_t>(periods);
        for (std::size_t bar = first; bar < i; ++bar) {
            double divisor = 1.0;
            if (const auto found = env.find("__CHIP_CAPITAL_DIVISOR");
                found != env.end() && bar < found->second.size() &&
                std::isfinite(found->second[bar]) &&
                found->second[bar] > 0.0)
                divisor = found->second[bar];
            const double bar_capital = native_float(capital->second[bar]);
            if (!(bar_capital >= native_epsilon)) {
                valid = false;
                break;
            }
            const double bar_volume = native_float(volume->second[bar]);
            retained = native_float(
                (1.0 - bar_volume / (bar_capital / divisor)) * retained);
        }
        const double current_capital = native_float(capital->second[i]);
        if (valid && current_capital >= native_epsilon) out[i] = retained;
    }
    return out;
}

const std::unordered_map<std::string_view, FunctionHandler>& chip_registry() {
    static const std::unordered_map<std::string_view, FunctionHandler> registry{
        {"COST", chip_distribution_call},
        {"COSTEX", chip_distribution_call},
        {"LWINNER", chip_distribution_call},
        {"PWINNER", chip_distribution_call},
        {"WINNER", chip_distribution_call},
        {"PPART", chip_retained_part_call},
    };
    return registry;
}

}  // namespace

std::optional<Series> evaluate_chip_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    const auto found = chip_registry().find(name);
    if (found == chip_registry().end()) return std::nullopt;
    return found->second(name, args, env, size);
}

}  // namespace tdx::formula_engine_detail
