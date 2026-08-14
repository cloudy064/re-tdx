#include "formula_engine_internal.hpp"
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

std::optional<Series> evaluate_core_series_builtin_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    if (name == "MA") {
        require_arity(name, args, 2, 2);
        return tcalc_moving_average(args[0], args[1]);
    }
    if (name == "SUM") {
        require_arity(name, args, 2, 2);
        return tcalc_sum(args[0], args[1]);
    }
    if (name == "HHV") {
        require_arity(name, args, 2, 2);
        return tcalc_highest_value(args[0], args[1]);
    }
    if (name == "LLV") {
        require_arity(name, args, 2, 2);
        return tcalc_lowest_value(args[0], args[1]);
    }
    if (name == "MULAR") {
        require_arity(name, args, 2, 2);
        Series out(size, 1.0);
        std::size_t first = size;
        double cumulative = 1.0;
        for (std::size_t i = 0; i < size; ++i) {
            if (first == size && std::isfinite(args[0][i])) first = i;
            if (first == size) continue;
            if (std::isfinite(args[0][i]))
                cumulative = native_float(cumulative * native_float(args[0][i]));
            out[i] = cumulative;
        }
        for (std::size_t i = first; i < size; ++i) {
            if (!std::isfinite(args[1][i])) continue;
            const int window = static_cast<int>(args[1][i]);
            if (window < 1 || static_cast<std::size_t>(window) > i + 1) continue;
            const auto begin = i + 1 - static_cast<std::size_t>(window);
            if (begin < first) continue;
            double product = 1.0;
            for (std::size_t at = begin; at <= i; ++at)
                if (std::isfinite(args[0][at]))
                    product = native_float(
                        product * native_float(args[0][at]));
            out[i] = product;
        }
        return out;
    }
    if (name == "EMA" || name == "EXPMA") {
        require_arity(name, args, 2, 2);
        return tcalc_exponential_moving_average(args[0], args[1]);
    }
    if (name == "EXPMEMA" || name == "MEMA") {
        require_arity(name, args, 2, 2);
        return tcalc_seeded_exponential_moving_average(
            args[0], args[1], name == "MEMA"
                ? TcalcSeededExponentialKind::mema
                : TcalcSeededExponentialKind::expmema);
    }
    if (name == "SMA") {
        require_arity(name, args, 3, 3);
        return tcalc_smoothed_moving_average(args[0], args[1], args[2]);
    }
    if (name == "SAR" || name == "SARTURN") {
        auto result = parabolic_sar(args, env, size);
        return name == "SAR" ? std::move(result.value) : std::move(result.turn);
    }
    if (name == "NEWSAR") return native_newsar(args, env, size);
    if (name == "FFTRANS") return native_fftrans(args, size);
    if (auto evaluated = evaluate_series_function(name, args, env, size))
        return std::move(*evaluated);

    if (auto evaluated = evaluate_context_function(name, args, env, size))
        return std::move(*evaluated);

    if (auto evaluated = evaluate_presentation_function(name, args, env, size))
        return std::move(*evaluated);
    return std::nullopt;
}
}  // namespace tdx::formula_engine_detail
