#include "formula_tdx_indicators_internal.hpp"
#include "formula_native_constants.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tdx::formula_engine_detail {

Series evaluate_tdx_trend_indicator(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    if (name == "TDXMSI") {
        require_arity(name, args, 4, 4);
        const auto advance = env.find("ADVANCE"), decline = env.find("DECLINE");
        if (advance == env.end() || decline == env.end())
            throw Error("TDXMSI requires ADVANCE and DECLINE series");
        Series msi(size, 0.0);
        if (!size) return msi;
        const int fast_period = period_at(args[0], size - 1, 1);
        const int slow_period = period_at(args[1], size - 1, 1);
        const int average_period = period_at(args[2], size - 1, 1);
        const int selector = static_cast<int>(native_float(args[3][size - 1]));

        Series change(size, 0.0);
        for (std::size_t i = 1; i < size; ++i) {
            if (!std::isfinite(advance->second[i]) ||
                !std::isfinite(advance->second[i - 1]) ||
                !std::isfinite(decline->second[i]) ||
                !std::isfinite(decline->second[i - 1])) {
                change[i] = missing;
                continue;
            }
            change[i] = native_float(
                (advance->second[i] - advance->second[i - 1]) -
                (decline->second[i] - decline->second[i - 1]));
        }
        const auto smooth_native_ema = [&](Series values, int period, bool one_over_n) {
            if (period < 1 || static_cast<std::size_t>(period) > values.size())
                return values;
            for (std::size_t i = 1; i < values.size(); ++i) {
                if (!std::isfinite(values[i]) || !std::isfinite(values[i - 1])) {
                    values[i] = missing;
                    continue;
                }
                const double retained = static_cast<double>(period - 1);
                const double incoming = one_over_n ? values[i] : values[i] * 2.0;
                const double divisor = one_over_n ? static_cast<double>(period)
                                                  : static_cast<double>(period + 1);
                values[i] = native_float((values[i - 1] * retained + incoming) / divisor);
            }
            return values;
        };
        const auto fast = smooth_native_ema(change, fast_period, false);
        const auto slow = smooth_native_ema(change, slow_period, false);
        msi[0] = native_float(
            30.0 * (fast[0] + slow[0]) + fast[0] - slow[0] - 1000.0);
        for (std::size_t i = 1; i < size; ++i) {
            if (!std::isfinite(fast[i]) || !std::isfinite(slow[i]) ||
                !std::isfinite(msi[i - 1])) {
                msi[i] = missing;
                continue;
            }
            msi[i] = native_float(msi[i - 1] +
                                  30.0 * (fast[i] + slow[i]) +
                                  fast[i] - slow[i]);
        }
        if (selector == 0) return msi;
        if (selector == 1) return smooth_native_ema(std::move(msi), average_period, true);
        throw Error("TDXMSI selector must be 0 or 1");
    }
    if (name == "TDXVTY") {
        require_arity(name, args, 2, 2);
        const auto high = env.find("HIGH"), low = env.find("LOW"),
                   close = env.find("CLOSE");
        if (high == env.end() || low == env.end() || close == env.end())
            throw Error("TDXVTY requires HIGH, LOW and CLOSE series");
        Series result(size, 0.0);
        if (!size) return result;
        const int smoothing = period_at(args[0], size - 1, 1);
        const double reversal_pct = native_float(args[1][size - 1]);

        Series threshold(size, 0.0);
        threshold[0] = native_float(
            native_float(high->second[0]) - native_float(low->second[0]));
        for (std::size_t i = 1; i < size; ++i) {
            const double current_high = native_float(high->second[i]);
            const double current_low = native_float(low->second[i]);
            const double previous_close = native_float(close->second[i - 1]);
            const double high_low = native_float(current_high - current_low);
            const double high_previous = native_float(current_high - previous_close);
            const double previous_low = native_float(previous_close - current_low);
            threshold[i] = native_float(
                std::max(high_low, std::max(high_previous, previous_low)));
        }
        const double retained = static_cast<double>(smoothing - 1);
        const double divisor = static_cast<double>(smoothing + 1);
        for (std::size_t i = 1; i < size; ++i)
            threshold[i] = native_float(
                (threshold[i - 1] * retained + threshold[i] * 2.0) / divisor);
        for (auto& value : threshold)
            value = native_float(reversal_pct / 100.0 * value);
        if (size == 1) return result;

        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        bool stop_above = false;
        double extreme = native_float(close->second[0]);
        double next_stop = native_float(extreme - threshold[0]);
        for (std::size_t i = 1; i < size; ++i) {
            result[i] = next_stop;
            const double current_close = native_float(close->second[i]);
            const double tolerance = std::abs(current_close) * relative_epsilon +
                                     native_epsilon;
            if (stop_above) {
                if (current_close - tolerance >= result[i]) {
                    stop_above = false;
                    extreme = current_close;
                    next_stop = native_float(current_close - threshold[i]);
                } else {
                    if (current_close < extreme) extreme = current_close;
                    next_stop = native_float(extreme + threshold[i]);
                }
            } else {
                if (current_close + tolerance <= result[i]) {
                    stop_above = true;
                    extreme = current_close;
                    next_stop = native_float(current_close + threshold[i]);
                } else {
                    if (current_close > extreme) extreme = current_close;
                    next_stop = native_float(extreme - threshold[i]);
                }
            }
        }
        return result;
    }
    if (name == "TDXSAR") {
        require_arity(name, args, 4, 4);
        const auto high = env.find("HIGH"), low = env.find("LOW");
        if (high == env.end() || low == env.end())
            throw Error("TDXSAR requires HIGH and LOW series");
        const int initial_window = period_at(args[0], size - 1, 1);
        const double initial_acceleration = native_float(args[1][size - 1]) / 100.0;
        const double acceleration_increment = native_float(args[2][size - 1]) / 100.0;
        const double acceleration_maximum = native_float(args[3][size - 1]) / 100.0;
        Series result(size, 0.0);
        if (initial_window > static_cast<int>(size)) return result;
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        const auto tolerance = [&](double value) {
            return std::abs(value) * relative_epsilon + native_epsilon;
        };
        double seed_low = native_float(low->second.front());
        for (int i = 1; i < initial_window; ++i) {
            const double candidate = native_float(low->second[static_cast<std::size_t>(i)]);
            if (candidate <= seed_low - tolerance(candidate)) seed_low = candidate;
        }
        const auto seed = static_cast<std::size_t>(initial_window - 1);
        result[seed] = seed_low;
        bool falling = false;
        double extreme = native_float(high->second.front());
        double acceleration = initial_acceleration;
        for (std::size_t i = seed + 1; i < size; ++i) {
            const double current_high = native_float(high->second[i]);
            const double current_low = native_float(low->second[i]);
            const double previous_high = native_float(high->second[i - 1]);
            const double previous_low = native_float(low->second[i - 1]);
            const double previous_sar = result[i - 1];
            const double old_extreme = extreme;
            double output = previous_sar;
            if (!falling) {
                if (previous_sar >= current_low + tolerance(current_low)) {
                    falling = true;
                    extreme = current_low;
                    acceleration = initial_acceleration;
                    output = std::max(previous_high, current_high);
                    const double projected = previous_sar +
                        (extreme - old_extreme) * acceleration;
                    output = std::max(output, projected);
                } else {
                    if (current_high - tolerance(current_high) >= extreme) {
                        extreme = current_high;
                        acceleration = std::min(acceleration_maximum,
                                                acceleration + acceleration_increment);
                    }
                    output = std::min(previous_low, current_low);
                    const double projected = previous_sar +
                        (extreme - previous_sar) * acceleration;
                    output = std::min(output, projected);
                }
            } else {
                if (previous_sar <= current_high - tolerance(current_high)) {
                    falling = false;
                    extreme = current_high;
                    acceleration = initial_acceleration;
                    output = std::min(previous_low, current_low);
                    const double projected = previous_sar +
                        (extreme - old_extreme) * acceleration;
                    output = std::min(output, projected);
                } else {
                    if (current_low + tolerance(current_low) <= extreme) {
                        extreme = current_low;
                        acceleration = std::min(acceleration_maximum,
                                                acceleration + acceleration_increment);
                    }
                    output = std::max(previous_high, current_high);
                    const double projected = previous_sar +
                        (extreme - previous_sar) * acceleration;
                    output = std::max(output, projected);
                }
            }
            extreme = native_float(extreme);
            acceleration = native_float(acceleration);
            result[i] = native_float(output);
        }
        return result;
    }
    if (name == "TDXASI") {
        require_arity(name, args, 2, 2);
        const auto open = env.find("OPEN"), high = env.find("HIGH"),
                   low = env.find("LOW"), close = env.find("CLOSE");
        if (open == env.end() || high == env.end() || low == env.end() || close == env.end())
            throw Error("TDXASI requires OPEN, HIGH, LOW and CLOSE series");
        const int smoothing = period_at(args[0], size - 1, 1);
        const int selector = period_at(args[1], size - 1);
        if (selector < 0 || selector > 1) throw Error("TDXASI selector must be 0 or 1");
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        Series asi(size, 0.0), average(size, 0.0);
        for (std::size_t i = 1; i < size; ++i) {
            const double current_high = native_float(high->second[i]);
            const double current_low = native_float(low->second[i]);
            const double current_close = native_float(close->second[i]);
            const double previous_close = native_float(close->second[i - 1]);
            const double previous_open = native_float(open->second[i - 1]);
            const double previous_low = native_float(low->second[i - 1]);
            const double a = native_float(std::abs(native_float(current_high - previous_close)));
            const double b = native_float(std::abs(native_float(current_low - previous_close)));
            const double c = native_float(std::abs(native_float(current_high - previous_low)));
            const double d = native_float(std::abs(native_float(previous_close - previous_open)));
            const double maximum_ab = std::max(a, b);
            const double maximum = std::max(maximum_ab, c);
            double r = 0.0;
            if (std::abs(maximum - a) < native_epsilon)
                r = native_float(a + b * 0.5 + d * 0.25);
            else if (std::abs(maximum - b) < native_epsilon)
                r = native_float(b + a * 0.5 + d * 0.25);
            else
                r = native_float(c + d * 0.25);
            double increment = 0.0;
            if (r >= native_epsilon || r <= -native_epsilon) {
                const double x = (current_close - native_float(open->second[i])) * 0.5 +
                                 current_close - previous_close + previous_close - previous_open;
                increment = native_float(x * 50.0 / r * maximum_ab /
                                         static_cast<double>(smoothing));
            }
            asi[i] = native_float(asi[i - 1] + increment);
        }
        if (size) average[0] = asi[0];
        if (size > 1) average[1] = asi[1];
        const double divisor = static_cast<double>(smoothing);
        const double retained = static_cast<double>(smoothing - 1);
        for (std::size_t i = 2; i < size; ++i)
            average[i] = native_float((average[i - 1] * retained + asi[i]) / divisor);
        return selector == 0 ? asi : average;
    }
    throw Error("unregistered TDX trend indicator: " + name);
}

}  // namespace tdx::formula_engine_detail

