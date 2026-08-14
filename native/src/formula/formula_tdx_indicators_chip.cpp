#include "formula_tdx_indicators_internal.hpp"
#include "formula_native_constants.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tdx::formula_engine_detail {
namespace {

Series tdx_ssrp_call(const std::vector<Series>& args,
                     const Environment& env, std::size_t size) {
    require_arity("TDXSSRP", args, 3, 3);
    const int first_average_period = size ? period_at(args[0], size - 1, 1) : 1;
    const int second_average_period = size ? period_at(args[1], size - 1, 1) : 1;
    const int selector = size ? period_at(args[2], size - 1) : 0;
    if (selector < 0 || selector > 2)
        throw Error("TDXSSRP selector must be 0..2");
    Series raw(size, missing);
    if (!size) return raw;
    const auto high = env.find("HIGH"), low = env.find("LOW"),
               raw_volume = env.find("__RAW_VOLUME"), capital = env.find("CAPITAL");
    if (high == env.end() || low == env.end() || raw_volume == env.end())
        throw Error("TDXSSRP requires HIGH, LOW and raw VOL series");
    if (capital == env.end())
        throw Error("TDXSSRP requires the historical CAPITAL context series");

    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    double global_low = native_float(low->second.front());
    double global_high = native_float(high->second.front());
    for (std::size_t i = 1; i < size; ++i) {
        const double bar_low = native_float(low->second[i]);
        const double bar_high = native_float(high->second[i]);
        if (bar_low <= global_low) global_low = bar_low;
        if (bar_high >= global_high) global_high = bar_high;
    }
    if (!std::isfinite(global_low) || !std::isfinite(global_high) ||
        global_low < native_epsilon || global_high < native_epsilon ||
        global_low > 10000.0 || global_high > 10000.0) return raw;
    const int lower_price = static_cast<int>(global_low * 100.0 - 1.0);
    const int upper_price = static_cast<int>(global_high * 100.0 + 1.0);
    const long long signed_bins = static_cast<long long>(upper_price) - lower_price + 1;
    if (signed_bins < 1) return raw;
    const auto bins = static_cast<std::size_t>(signed_bins);
    std::vector<double> distribution(bins, 0.0);
    const std::size_t begin = size > 720 ? size - 720 : 0;
    for (std::size_t i = begin; i < size; ++i) {
        const double bar_capital = native_float(capital->second[i] * 100.0);
        if (!(bar_capital >= native_epsilon)) {
            if (i > begin) raw[i] = raw[i - 1];
            continue;
        }
        const double bar_volume = native_float(raw_volume->second[i]);
        const double turnover = native_float(bar_volume / bar_capital);
        const double retained = 1.0 - turnover;
        for (auto& weight : distribution)
            weight = native_float(weight * retained);

        const auto price_bin = [&](double price) {
            const double scaled = native_float(price) * 100.0;
            const double bounded = std::max<double>(
                lower_price, std::min<double>(upper_price, scaled));
            return static_cast<int>(bounded) - lower_price;
        };
        const int bar_low = price_bin(low->second[i]);
        const int bar_high = price_bin(high->second[i]);
        if (bar_low <= bar_high && bar_low >= 0 &&
            static_cast<std::size_t>(bar_high) < bins) {
            const double addition = native_float(
                bar_volume / static_cast<double>(bar_high - bar_low + 1));
            for (int price = bar_low; price <= bar_high; ++price)
                distribution[static_cast<std::size_t>(price)] = native_float(
                    distribution[static_cast<std::size_t>(price)] + addition);
        }

        double total = 0.0;
        for (const double weight : distribution)
            total = native_float(total + weight / 10000.0);
        double average = 0.0;
        for (std::size_t price = 0; price < bins; ++price) {
            const double scaled_weight = distribution[price] / 10000.0;
            const double term = scaled_weight / total *
                static_cast<double>(lower_price + static_cast<int>(price)) / 100.0;
            average = native_float(average + term);
        }
        raw[i] = average;
    }

    if (selector == 0) return raw;
    Series average = raw;
    const int period = selector == 1 ? first_average_period : second_average_period;
    const auto first = std::find_if(average.begin(), average.end(), [](double value) {
        return std::isfinite(value);
    });
    if (first == average.end()) return average;
    const auto first_index = static_cast<std::size_t>(first - average.begin());
    if (period < 1 || first_index + static_cast<std::size_t>(period) > size)
        return average;
    for (std::size_t i = first_index + 1; i < size; ++i) {
        if (!std::isfinite(average[i]) || !std::isfinite(average[i - 1])) continue;
        average[i] = native_float(
            (average[i - 1] * static_cast<double>(period - 1) + average[i]) /
            static_cast<double>(period));
    }
    return average;
}


Series tdx_pav_call(const std::string& name, const std::vector<Series>& args,
                    const Environment& env, std::size_t size) {
    require_arity(name, args, 3, 3);
    const int first_average_period = size ? period_at(args[0], size - 1, 1) : 1;
    const int second_average_period = size ? period_at(args[1], size - 1, 1) : 1;
    const int selector = size ? period_at(args[2], size - 1) : 0;
    const int maximum_selector = name == "TDXPAV" ? 4 : 2;
    if (selector < 0 || selector > maximum_selector)
        throw Error(name + " selector must be 0.." + std::to_string(maximum_selector));

    Series positive(size, missing), negative(size, missing);
    if (!size) return positive;
    const auto high = env.find("HIGH"), low = env.find("LOW"), close = env.find("CLOSE"),
               raw_volume = env.find("__RAW_VOLUME"), capital = env.find("CAPITAL");
    if (high == env.end() || low == env.end() || close == env.end() ||
        raw_volume == env.end())
        throw Error(name + " requires HIGH, LOW, CLOSE and raw VOL series");
    if (capital == env.end())
        throw Error(name + " requires the circulating CAPITAL context series");

    // TCalc sub_1002DCD0/sub_10032DE0 request host type 105 once and use
    // response +49 for every bar. TdxW fills +49 from the current circulating
    // shares field (security record +86, stored in ten-thousand-share units)
    // multiplied by 10000. Formula context exposes CAPITAL in hands.
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    const double current_capital = native_float(capital->second.back() * 100.0);
    if (!(current_capital >= native_epsilon)) return positive;

    auto native_dword = [](double value) -> std::uint32_t {
        if (!std::isfinite(value)) return 0;
        const auto narrowed = static_cast<long long>(value);
        return static_cast<std::uint32_t>(narrowed);
    };
    std::vector<std::uint32_t> distribution(200, 0), old_distribution(200, 0);
    double global_high = native_float(high->second.front());
    double global_low = native_float(low->second.front());
    double width = native_float((global_high - global_low) / 200.0);
    double accumulated_turnover = 0.0;

    for (std::size_t bar = 0; bar < size; ++bar) {
        const double old_high = global_high;
        const double old_low = global_low;
        const double old_width = width;
        const double bar_low = native_float(low->second[bar]);
        const double bar_high = native_float(high->second[bar]);
        const double bar_close = native_float(close->second[bar]);
        const double bar_volume = native_float(raw_volume->second[bar]);
        if (!std::isfinite(bar_low) || !std::isfinite(bar_high) ||
            !std::isfinite(bar_close) || !std::isfinite(bar_volume)) continue;
        if (bar_low <= global_low) global_low = bar_low;
        if (bar_high >= global_high) global_high = bar_high;
        width = native_float((global_high - global_low) / 200.0);
        if (!(width >= native_epsilon)) continue;

        if (old_low - global_low > native_epsilon ||
            global_high - old_high > native_epsilon) {
            old_distribution = distribution;
            std::fill(distribution.begin(), distribution.end(), 0);
            for (int bin = 0; bin < 200; ++bin) {
                const double position = static_cast<double>(bin) * old_width / width;
                const int target = static_cast<int>(position);
                if (target < 0 || target >= 200) continue;
                const double target_float = native_float(static_cast<double>(target));
                const double fraction = position - target_float;
                const double remaining_width = width - fraction * width;
                const double tolerance = std::abs(native_float(remaining_width)) *
                                             relative_epsilon +
                                         native_epsilon;
                if (remaining_width + tolerance > old_width - native_epsilon) {
                    distribution[static_cast<std::size_t>(target)] +=
                        old_distribution[static_cast<std::size_t>(bin)];
                } else {
                    const double old_weight = native_float(
                        static_cast<double>(old_distribution[static_cast<std::size_t>(bin)]));
                    distribution[static_cast<std::size_t>(target)] +=
                        native_dword(fraction * old_weight);
                    if (target + 1 < 200)
                        distribution[static_cast<std::size_t>(target + 1)] += native_dword(
                            old_weight * (target_float + 1.0 - position));
                }
            }
        }

        const double low_index = native_float((global_high - bar_low) / width);
        const double high_index = native_float((global_high - bar_high) / width);
        const double turnover = native_float(bar_volume / current_capital);
        const double retained = 1.0 - turnover;
        accumulated_turnover = std::max(
            1.0, turnover + accumulated_turnover * retained);
        for (auto& weight : distribution)
            weight = native_dword(static_cast<double>(weight) * retained);

        int first_bin = static_cast<int>(high_index);
        const int last_bin = static_cast<int>(low_index);
        if (first_bin <= last_bin) {
            const double addition = bar_volume / (low_index - high_index + 1.0);
            for (int bin = first_bin; bin <= last_bin && bin <= 199; ++bin)
                if (bin >= 0)
                    distribution[static_cast<std::size_t>(bin)] += native_dword(addition);
        }

        double below_or_equal = 0.0;
        double above = 0.0;
        for (int bin = 0; bin < 200; ++bin) {
            const double price = native_float(
                global_high - static_cast<double>(bin) * width);
            const double threshold = price + std::abs(price) * relative_epsilon +
                                     native_epsilon;
            const double weight = static_cast<double>(
                distribution[static_cast<std::size_t>(bin)] / 1000u);
            if (bar_close < threshold) above -= weight;
            else below_or_equal += weight;
        }
        const double total = below_or_equal - above;
        if (total > native_epsilon) {
            positive[bar] = native_float(below_or_equal / total * 100.0);
            negative[bar] = native_float(above / total * 100.0);
        }
    }

    if (name == "TDXPAV") {
        if (selector == 0) return positive;
        if (selector == 1) return negative;
        const auto average_positive = native_recursive_average(
            positive, first_average_period);
        const auto average_negative = native_recursive_average(
            negative, second_average_period);
        if (selector == 2) return average_positive;
        if (selector == 3) return average_negative;
        Series difference(size, missing);
        for (std::size_t i = 0; i < size; ++i)
            if (std::isfinite(average_positive[i]) && std::isfinite(average_negative[i]))
                difference[i] = native_float(average_positive[i] + average_negative[i]);
        return difference;
    }

    const auto cv = native_recursive_average(negative, 2);
    const auto mcv = native_recursive_average(negative, second_average_period);
    if (selector == 0) return cv;
    if (selector == 1) return mcv;
    const auto average_positive = native_recursive_average(positive, first_average_period);
    Series difference(size, missing);
    for (std::size_t i = 0; i < size; ++i)
        if (std::isfinite(average_positive[i]) && std::isfinite(mcv[i]))
            difference[i] = native_float(average_positive[i] + mcv[i]);
    return difference;
}


}  // namespace

Series evaluate_tdx_chip_indicator(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    if (name == "TDXSSRP") return tdx_ssrp_call(args, env, size);
    if (name == "TDXPAV" || name == "TDXPAVE")
        return tdx_pav_call(name, args, env, size);
    if (name == "TDXMCST") {
        require_arity(name, args, 0, 0);
        const auto close = env.find("CLOSE"), amount = env.find("AMOUNT"),
                   raw_volume = env.find("__RAW_VOLUME"), capital = env.find("CAPITAL"),
                   scale_series = env.find("__MCST_VOLUME_SCALE");
        if (close == env.end() || amount == env.end() || raw_volume == env.end())
            throw Error("TDXMCST requires CLOSE, AMOUNT and raw VOL series");
        if (capital == env.end())
            throw Error("TDXMCST requires the circulating CAPITAL context series");
        if (scale_series == env.end())
            throw Error("TDXMCST requires the native market volume scale");
        Series result(size, 0.0);
        if (!size) return result;
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        // Type-103 CAPITAL is returned in shares.  Formula context uses hands,
        // so restore the native unit before reproducing sub_1002DB40.
        const double current_capital = native_float(capital->second.back() * 100.0);
        const double scale = native_float(scale_series->second.back());
        const double first_close = native_float(close->second[0]);
        const double first_amount = native_float(amount->second[0]);
        const double first_volume = native_float(raw_volume->second[0]);
        if (!std::isfinite(current_capital) || !std::isfinite(scale) ||
            !std::isfinite(first_close) || !std::isfinite(first_amount) ||
            !std::isfinite(first_volume)) {
            std::fill(result.begin(), result.end(), missing);
            return result;
        }
        double accumulated = native_float(current_capital * first_close + first_amount -
                                          first_volume * first_close * scale);
        result[0] = current_capital > 0.0
            ? native_float(accumulated / current_capital) : 0.0;
        const double capital_limit = current_capital +
            std::abs(current_capital) * relative_epsilon + native_epsilon;
        for (std::size_t i = 1; i < size; ++i) {
            const double volume = native_float(raw_volume->second[i]);
            const double turnover_limit = volume +
                std::abs(volume) * relative_epsilon + native_epsilon;
            if (!std::isfinite(volume) || !std::isfinite(amount->second[i])) {
                result[i] = missing;
                accumulated = missing;
            } else if (!std::isfinite(accumulated)) {
                result[i] = missing;
            } else if (1.0 >= capital_limit || turnover_limit <= 1.0) {
                result[i] = result[i - 1];
            } else {
                const double bar_amount = native_float(amount->second[i]);
                accumulated = native_float(accumulated + bar_amount -
                    result[i - 1] * volume * scale);
                result[i] = native_float(accumulated / current_capital);
            }
        }
        return result;
    }
    throw Error("unregistered TDX chip-distribution indicator: " + name);
}

}  // namespace tdx::formula_engine_detail

