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

std::optional<Series> evaluate_host_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    if (name == "RAND") {
        require_arity(name, args, 1, 1);
        const auto seed_binding = env.find("__RAND_SEED");
        if (seed_binding == env.end() || seed_binding->second.empty())
            throw Error("RAND requires an evaluation seed");
        std::uint32_t state = static_cast<std::uint32_t>(
            seed_binding->second.front());
        Series output(size, missing);
        for (std::size_t index = 0; index < size; ++index) {
            if (!std::isfinite(args[0][index])) continue;
            const float bound = static_cast<float>(args[0][index]);
            if (!std::isfinite(bound) || bound < 1.0f || bound > 1000000.0f)
                continue;
            const int modulus = static_cast<int>(
                static_cast<double>(bound) + tdx::formula_engine_detail::tcalc_constants::integer_bias);
            output[index] = static_cast<double>(static_cast<float>(
                tcalc_msvc_rand(state) % static_cast<std::uint32_t>(modulus) + 1u));
        }
        return output;
    }
    if (custom_formula_security_score_functions.count(name)) {
        require_arity(name, args, 0, 0);
        const auto found = env.find(name);
        if (found == env.end())
            throw Error(name + " requires local specgpext score context");
        Series output(size, missing);
        constexpr float native_valid_minimum = -0.0000099999997f;
        for (std::size_t index = 0; index < size; ++index) {
            if (!std::isfinite(found->second[index])) continue;
            const float value = static_cast<float>(found->second[index]);
            if (value > native_valid_minimum)
                output[index] = static_cast<double>(value);
        }
        return output;
    }
    if (name == "EXTDATA_USER") {
        require_arity(name, args, 2, 2);
        if (!env.count("__EXTDATA_USER_READY"))
            throw Error("EXTDATA_USER requires automatic external-series context");
        const auto key = "EXTDATA_USER#" +
                         std::to_string(tcalc_external_integer(args[0].back())) + "#" +
                         std::to_string(tcalc_external_integer(args[1].back()));
        const auto found = env.find(key);
        if (found == env.end())
            throw Error("EXTDATA_USER automatic context is missing binding " + key);
        return found->second;
    }
    if (name == "SIGNALS_SYS" || name == "SIGNALS_USER") {
        require_arity(name, args, 2, 2);
        const auto key = name + "#" +
                         std::to_string(tcalc_external_integer(args[0].back())) + "#" +
                         std::to_string(tcalc_external_integer(args[1].back()));
        const auto found = env.find(key);
        if (found == env.end())
            throw Error(name + " requires automatic local-signal context: " + key);
        return found->second;
    }
    if (name == "EXTERNVALUE") {
        require_arity(name, args, 2, 2);
        if (!env.count("__EXTERN_SIGNALS_READY"))
            throw Error("EXTERNVALUE requires automatic external-signal context");
        const auto key = "__EXTERNVALUE#" +
                         tcalc_external_binding_key(args[0].back(), args[1].back());
        const auto found = env.find(key);
        // TdxW zeroes the 255-byte callback buffer before lookup, so a missing
        // record is a successful callback whose numeric payload is exactly 0.
        return found == env.end() ? constant(0.0, size) : found->second;
    }
    if (name == "EXTERNSTR") {
        require_arity(name, args, 2, 2);
        // Numeric consumers see only TCalc's opaque string-pool handle.  The
        // exact UTF-8 value is resolved from the AST by evaluate_string_node.
        return constant(0.0, size);
    }
    if (custom_formula_adjustment_functions.count(name)) {
        require_arity(name, args, 0, 0);
        const auto found = env.find(name);
        if (found == env.end())
            throw Error(name + " requires the K-line adjustment-mode context");
        return found->second;
    }
    if (custom_formula_directional_bar_functions.count(name)) {
        require_arity(name, args, 0, 0);
        const auto found = env.find(name);
        if (found == env.end())
            throw Error(name + " requires native directional-bar context");
        return found->second;
    }
    if (custom_formula_machine_clock_functions.count(name)) {
        require_arity(name, args, 0, 0);
        const auto found = env.find(name);
        if (found == env.end())
            throw Error(name + " requires the native machine-clock snapshot");
        return found->second;
    }
    if (name == "LFS") {
        require_arity(name, args, 0, 0);
        const auto raw_volume = env.find("__RAW_VOLUME");
        const auto capital = env.find("CAPITAL");
        const auto supported = env.find("__LFS_SECURITY_SUPPORTED");
        if (raw_volume == env.end())
            throw Error("LFS requires the raw VOL series");
        if (capital == env.end())
            throw Error("LFS requires the historical circulating CAPITAL context series");
        if (supported == env.end())
            throw Error("LFS requires the native market/security context");

        Series result(size, missing);
        if (!size || !truth(supported->second.back())) return result;
        constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
        constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
        const auto invalid_capital = [&](double value) {
            return !std::isfinite(value) ||
                   value - std::abs(value) * relative_epsilon - native_epsilon < 1.0;
        };

        bool initialized = false;
        double previous_capital = missing;
        double fast = 0.0, slow = 0.0;
        for (std::size_t i = 0; i < size; ++i) {
            // Public formula CAPITAL is expressed in hands.  Type-103 and the
            // 35-byte native bar both use shares, including the <1-share gap
            // boundary, so restore the callback unit before calculating.
            double native_capital = native_float(capital->second[i] * 100.0);
            if (invalid_capital(native_capital)) {
                if (!initialized || invalid_capital(previous_capital)) continue;
                native_capital = previous_capital;
            } else {
                previous_capital = native_capital;
            }
            if (native_capital < native_epsilon) continue;

            const double turnover = native_float(
                native_float(raw_volume->second[i]) / native_capital);
            if (!initialized) {
                fast = turnover;
                slow = turnover;
                initialized = true;
            } else {
                fast = native_float(
                    (1.0 - turnover) * 4.0 / 5.0 * fast + turnover);
                slow = native_float(
                    turnover + (1.0 - turnover) * 12.0 / 13.0 * slow);
            }
            result[i] = native_float((1.0 - fast / slow) * 100.0);
        }
        return result;
    }
    if (auto evaluated = evaluate_tdx_indicator_function(
            name, args, env, size))
        return std::move(*evaluated);

    if (name == "IVOLAT") {
        require_arity(name, args, 2, 2);
        const auto history_days = env.find("__IVOLAT_HISTORY_DAY");
        const auto history_closes = env.find("__IVOLAT_HISTORY_CLOSE");
        const auto bar_days = env.find("__IVOLAT_BAR_DAY");
        if (history_days == env.end() || history_closes == env.end() ||
            bar_days == env.end() || history_days->second.size() != history_closes->second.size())
            throw Error("missing IVOLAT cross-security formula context");
        const auto scalar = [&](std::string_view key, double fallback) {
            const auto found = env.find(std::string(key));
            return found == env.end() || found->second.empty() ? fallback : found->second.front();
        };
        const bool futures_model = truth(scalar("__IVOLAT_FUTURES_MODEL", 0.0));
        const bool american = truth(scalar("__IVOLAT_AMERICAN", 0.0));
        const bool call = truth(scalar("__IVOLAT_CALL", 0.0));
        const double divisor = scalar("__IVOLAT_DIVISOR", 1.0);
        const double strike = scalar("__IVOLAT_STRIKE", missing);
        const double risk_free = scalar("__IVOLAT_RISK_FREE", 0.0187);
        const double expiry_day = scalar("__IVOLAT_EXPIRY_DAY", missing);
        const auto& option_close = env.at("CLOSE");
        Series out(size, missing);
        for (std::size_t i = 0; i < size; ++i) {
            if (!std::isfinite(bar_days->second[i])) continue;
            const auto end = std::upper_bound(
                history_days->second.begin(), history_days->second.end(), bar_days->second[i]);
            if (end == history_days->second.begin()) continue;
            const std::size_t end_index = static_cast<std::size_t>(end - history_days->second.begin());
            const int n = period_at(args[0], i, 2);
            const std::size_t begin_index = end_index > static_cast<std::size_t>(n)
                ? end_index - static_cast<std::size_t>(n) : 0;
            std::vector<double> closes(history_closes->second.begin() +
                                           static_cast<std::ptrdiff_t>(begin_index),
                                       history_closes->second.begin() +
                                           static_cast<std::ptrdiff_t>(end_index));
            const double historical = tdx_historical_volatility(closes);
            const int mode = period_at(args[1], i);
            if (mode == 0) {
                if (closes.size() >= 2) out[i] = historical;
                continue;
            }
            if (mode != 1 || !std::isfinite(expiry_day) || expiry_day < bar_days->second[i] ||
                !std::isfinite(option_close[i]) || option_close[i] <= 0.0 ||
                !std::isfinite(strike))
                continue;
            const double underlying = history_closes->second[end_index - 1];
            const double years = (expiry_day - bar_days->second[i] + 1.0) / 365.0;
            out[i] = tdx_implied_volatility(
                futures_model, american, call, divisor, underlying, strike, years,
                historical > 0.000001 ? historical : 0.2, risk_free, option_close[i]);
        }
        return out;
    }
    if (auto evaluated = evaluate_chip_function(name, args, env, size))
        return std::move(*evaluated);
    return std::nullopt;
}
}  // namespace tdx::formula_engine_detail
