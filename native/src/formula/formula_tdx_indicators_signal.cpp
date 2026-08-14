#include "formula_tdx_indicators_internal.hpp"
#include "formula_native_constants.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tdx::formula_engine_detail {
namespace {

Series tdx_ndb_call(const std::vector<Series>& args,
                    const Environment& env, std::size_t size) {
    require_arity("TDXNDB", args, 3, 3);
    const int first_average_period = size ? period_at(args[0], size - 1, 1) : 1;
    const int second_average_period = size ? period_at(args[1], size - 1, 1) : 1;
    const int selector = size ? period_at(args[2], size - 1) : 0;
    if (selector < 0 || selector > 2)
        throw Error("TDXNDB selector must be 0..2");
    const auto high = env.find("HIGH"), low = env.find("LOW"), close = env.find("CLOSE");
    if (high == env.end() || low == env.end() || close == env.end())
        throw Error("TDXNDB requires HIGH, LOW and CLOSE series");

    Series ndb(size, 0.0);
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    bool st_security = false;
    if (const auto found = env.find("__IS_ST_SECURITY"); found != env.end() &&
        !found->second.empty())
        st_security = truth(found->second.back());
    for (std::size_t i = 1; i < size; ++i) {
        const double previous_close = native_float(close->second[i - 1]);
        const double current_close = native_float(close->second[i]);
        const double current_high = native_float(high->second[i]);
        const double current_low = native_float(low->second[i]);
        if (!std::isfinite(previous_close) || !std::isfinite(current_close) ||
            !std::isfinite(current_high) || !std::isfinite(current_low)) {
            ndb[i] = missing;
            continue;
        }
        const double tolerance = std::abs(current_close) * relative_epsilon + native_epsilon;
        const bool limit_up = (st_security && previous_close * 1.05 <= current_close - tolerance) ||
                              previous_close * 1.1 < current_close + tolerance;
        const bool limit_down =
            (st_security && previous_close * 0.95 >= current_close + tolerance) ||
            previous_close * 0.9 > current_close - tolerance;
        double first = 0.0, second = 0.0;
        if (limit_up) {
            first = current_close - previous_close;
            second = current_close - current_high;
        } else if (limit_down) {
            first = current_close - current_low;
            second = current_close - previous_close;
        } else {
            first = current_close - current_low;
            second = current_close - current_high;
        }
        if (!std::isfinite(ndb[i - 1])) ndb[i] = missing;
        else ndb[i] = native_float(ndb[i - 1] + native_float(first) + native_float(second));
    }
    if (selector == 0) return ndb;
    return native_recursive_average(
        std::move(ndb), selector == 1 ? first_average_period : second_average_period);
}


Series native_tcalc_ema(Series values, int period) {
    if (period < 1 || values.size() < static_cast<std::size_t>(period)) return values;
    for (int i = 1; i < period; ++i)
        values[static_cast<std::size_t>(i)] = native_float(
            (values[static_cast<std::size_t>(i - 1)] * static_cast<double>(period - 1) +
             values[static_cast<std::size_t>(i)]) /
            static_cast<double>(period));
    for (std::size_t i = static_cast<std::size_t>(period); i < values.size(); ++i)
        values[i] = native_float(
            (values[i - 1] * static_cast<double>(period - 1) + values[i] * 2.0) /
            static_cast<double>(period + 1));
    return values;
}


Series tdx_sc_call(const std::vector<Series>& args,
                   const Environment& env, std::size_t size) {
    require_arity("TDXSC", args, 0, 0);
    const auto high = env.find("HIGH"), low = env.find("LOW"), close = env.find("CLOSE"),
               raw_volume = env.find("__RAW_VOLUME");
    if (high == env.end() || low == env.end() || close == env.end() ||
        raw_volume == env.end())
        throw Error("TDXSC requires HIGH, LOW, CLOSE and raw VOL series");
    Series result(size, 0.0), weighted_price(size, 0.0), native_close(size, 0.0),
           volume(size, 0.0);
    for (std::size_t i = 0; i < size; ++i) {
        native_close[i] = native_float(close->second[i]);
        weighted_price[i] = native_float(
            (native_float(low->second[i]) + native_float(high->second[i]) +
             native_close[i] * 2.0) * 0.25);
        volume[i] = native_float(raw_volume->second[i]);
    }
    const auto ema12 = native_tcalc_ema(weighted_price, 12);
    const auto ema26 = native_tcalc_ema(weighted_price, 26);
    Series difference(size, 0.0);
    for (std::size_t i = 0; i < size; ++i)
        difference[i] = native_float(ema12[i] - ema26[i]);
    const auto signal = native_tcalc_ema(difference, 9);

    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    constexpr double pulse[]{10.0, 9.0, 8.0, 6.0, 3.0};
    const auto tolerance = [&](double value) {
        return std::abs(value) * relative_epsilon + native_epsilon;
    };
    const auto apply_pulse = [&](std::size_t at, double direction) {
        const auto count = std::min<std::size_t>(5, size - at);
        for (std::size_t i = 0; i < count; ++i)
            result[at + i] = native_float(result[at + i] + direction * pulse[i]);
    };

    for (std::size_t i = 34; i < size; ++i) {
        const bool bullish =
            signal[i - 1] >= difference[i - 1] + tolerance(difference[i - 1]) &&
            signal[i] <= difference[i] - tolerance(difference[i]);
        const bool bearish =
            signal[i - 1] <= difference[i - 1] - tolerance(difference[i - 1]) &&
            signal[i] >= difference[i] + tolerance(difference[i]);
        if (bullish) apply_pulse(i, 1.0);
        if (bearish) apply_pulse(i, -1.0);
    }

    const auto close_fast = native_recursive_average(native_close, 5);
    const auto close_slow = native_recursive_average(native_close, 10);
    const auto volume_fast = native_recursive_average(volume, 5);
    const auto volume_slow = native_recursive_average(volume, 10);
    for (std::size_t i = 11; i < size; ++i) {
        const bool price_bullish =
            close_slow[i - 1] >= close_fast[i - 1] + tolerance(close_fast[i - 1]) &&
            close_slow[i] <= close_fast[i] - tolerance(close_fast[i]);
        const bool price_bearish =
            close_slow[i - 1] <= close_fast[i - 1] - tolerance(close_fast[i - 1]) &&
            close_slow[i] >= close_fast[i] + tolerance(close_fast[i]);
        const bool volume_cross_down =
            volume_slow[i - 1] <= volume_fast[i - 1] - tolerance(volume_fast[i - 1]) &&
            volume_slow[i] >= volume_fast[i] + tolerance(volume_fast[i]);
        const bool volume_cross_up =
            volume_slow[i - 1] >= volume_fast[i - 1] + tolerance(volume_fast[i - 1]) &&
            volume_slow[i] <= volume_fast[i] - tolerance(volume_fast[i]);
        if (price_bullish) apply_pulse(i, 1.0);
        if (price_bearish || volume_cross_down || volume_cross_up)
            apply_pulse(i, -1.0);
    }
    return result;
}


Series tdx_xlpl_base_call(const std::vector<Series>& args,
                          const Environment& env, std::size_t size) {
    require_arity("TDXXLPLBASE", args, 0, 0);
    const auto close = env.find("CLOSE");
    if (close == env.end()) throw Error("TDXXLPLBASE requires CLOSE series");

    // TCalc XLPL sub_10031E00 calls sub_1000B0C0 twice with N=13.  The
    // implementation stores every intermediate in float arrays, including
    // the subtraction, division and multiplication by 1000 that follow.
    const auto native_ema13 = [&](const Series& input) {
        Series result(size, missing);
        if (!size) return result;
        const auto first = std::find_if(input.begin(), input.end(), [](double value) {
            return std::isfinite(value);
        });
        if (first == input.end()) return result;
        const auto begin = static_cast<std::size_t>(first - input.begin());
        result[begin] = native_float(input[begin]);
        for (std::size_t i = begin + 1; i < size; ++i) {
            if (!std::isfinite(input[i]) || !std::isfinite(result[i - 1])) continue;
            result[i] = native_float(
                (12.0 * result[i - 1] + 2.0 * native_float(input[i])) / 14.0);
        }
        return result;
    };

    const auto first = native_ema13(close->second);
    const auto second = native_ema13(first);
    Series result(size, missing);
    for (std::size_t i = 0; i < size; ++i) {
        if (!std::isfinite(second[i])) continue;
        const double previous = second[i ? i - 1 : 0];
        if (!std::isfinite(previous) || std::abs(previous) <= 1e-30) continue;
        const double difference = native_float(second[i] - previous);
        const double ratio = native_float(difference / previous);
        result[i] = native_float(ratio * 1000.0);
    }
    return result;
}


}  // namespace

Series evaluate_tdx_signal_indicator(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    if (name == "TDXXLPLBASE") return tdx_xlpl_base_call(args, env, size);
    if (name == "TDXZXNH") {
        require_arity(name, args, 0, 0);
        const auto high = env.find("HIGH"), low = env.find("LOW"), close = env.find("CLOSE"),
                   amount = env.find("AMOUNT"), raw_volume = env.find("__RAW_VOLUME"),
                   scale = env.find("__MCST_VOLUME_SCALE"),
                   typical_mode = env.find("__ZXNH_TYPICAL_PRICE");
        if (high == env.end() || low == env.end() || close == env.end() ||
            amount == env.end() || raw_volume == env.end())
            throw Error("TDXZXNH requires HIGH, LOW, CLOSE, AMOUNT and raw VOL series");
        if (scale == env.end() || typical_mode == env.end())
            throw Error("TDXZXNH requires the native market/security context");

        Series result(size, 0.0);
        if (size < 4) return result;
        const auto sar = evaluate_call("TDXSAR", {
            constant(4.0, size), constant(2.0, size),
            constant(2.0, size), constant(20.0, size)}, env, size);
        Series fitted_price(size, missing);
        const bool use_typical = truth(typical_mode->second.back());
        const double volume_scale = native_float(scale->second.back());
        for (std::size_t i = 0; i < size; ++i) {
            const double typical = native_float(
                (native_float(high->second[i]) + native_float(low->second[i]) +
                 native_float(close->second[i])) / 3.0);
            const double volume = native_float(raw_volume->second[i]);
            if (use_typical || volume <= 0.0 || !std::isfinite(amount->second[i])) {
                fitted_price[i] = typical;
            } else {
                fitted_price[i] = native_float(
                    native_float(amount->second[i]) / (volume * volume_scale));
            }
        }

        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        const auto tolerance = [&](double value) {
            return std::abs(value) * relative_epsilon + native_epsilon;
        };
        result.front() = 1.0;
        result.back() = 1.0;
        std::size_t anchor = 4;
        while (anchor < size) {
            std::size_t candidate = anchor;
            double candidate_price = fitted_price[anchor];
            std::size_t next = anchor + 1;
            const bool above_sar = native_float(close->second[anchor]) >=
                                   sar[anchor] + tolerance(sar[anchor]);
            while (next < size) {
                const double next_sar = sar[next];
                const double next_close = native_float(close->second[next]);
                if (above_sar) {
                    if (next_close <= next_sar - tolerance(next_sar)) break;
                    const double price = fitted_price[next];
                    if (candidate_price <= price - tolerance(price)) {
                        candidate = next;
                        candidate_price = price;
                    }
                } else {
                    if (next_close >= next_sar + tolerance(next_sar)) break;
                    const double price = fitted_price[next];
                    if (candidate_price >= price + tolerance(price)) {
                        candidate = next;
                        candidate_price = price;
                    }
                }
                ++next;
            }
            if (candidate > 4) result[candidate] = 1.0;
            anchor = next;
        }
        return result;
    }
    if (name == "TDXNDB") return tdx_ndb_call(args, env, size);
    if (name == "TDXSC") return tdx_sc_call(args, env, size);
    throw Error("unregistered TDX proprietary-signal indicator: " + name);
}

}  // namespace tdx::formula_engine_detail

