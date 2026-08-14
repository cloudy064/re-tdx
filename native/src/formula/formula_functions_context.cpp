#include "formula_function_dispatch_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <string_view>
#include <unordered_map>

namespace tdx::formula_engine_detail {
namespace {

using FunctionHandler = Series (*)(const std::string&,
                                   const std::vector<Series>&,
                                   const Environment&, std::size_t);

Series required_binding(const std::string& name,
                        const std::vector<Series>& args,
                        const Environment& env, std::size_t,
                        std::string_view requirement) {
    require_arity(name, args, 0, 0);
    const auto found = env.find(name);
    if (found == env.end())
        throw Error(name + " requires automatic " +
                    std::string(requirement) + " context");
    return found->second;
}

Series security_stat_binding(const std::string& name,
                             const std::vector<Series>& args,
                             const Environment& env, std::size_t size) {
    return required_binding(name, args, env, size, "tdxstat host");
}

Series industry_valuation_binding(const std::string& name,
                                  const std::vector<Series>& args,
                                  const Environment& env, std::size_t size) {
    return required_binding(name, args, env, size, "industry-valuation");
}

Series market_breadth_binding(const std::string& name,
                              const std::vector<Series>& args,
                              const Environment& env, std::size_t size) {
    return required_binding(name, args, env, size, "main-index breadth");
}

Series dynamic_quote_binding(const std::string& name,
                             const std::vector<Series>& args,
                             const Environment& env, std::size_t size) {
    return required_binding(name, args, env, size, "public quote");
}

Series security_relation_binding(const std::string& name,
                                 const std::vector<Series>& args,
                                 const Environment& env, std::size_t size) {
    return required_binding(name, args, env, size, "security-relation");
}

Series divfactor_binding(const std::string& name,
                         const std::vector<Series>& args,
                         const Environment& env, std::size_t size) {
    require_arity(name, args, 1, 1);
    if (args[0].empty() || !std::isfinite(args[0].back()))
        return constant(1.0, size);
    const float selector = native_float(args[0].back());
    if (!std::isfinite(selector) || selector >= 2147483648.0f ||
        selector < -2147483648.0f)
        return constant(1.0, size);
    int mode = static_cast<int>(selector);
    if (mode == 0) {
        const auto adjustment = env.find("TQFLAG");
        mode = adjustment == env.end() || adjustment->second.empty() ||
                       !std::isfinite(adjustment->second.back())
                   ? 0
                   : static_cast<int>(
                         native_float(adjustment->second.back()));
    }
    if (mode != 1 && mode != 2) return constant(1.0, size);
    const auto binding = env.find(
        mode == 1 ? "__DIVFACTOR_FRONT" : "__DIVFACTOR_BACK");
    if (binding == env.end())
        throw Error("DIVFACTOR requires automatic corporate-action context");
    return binding->second;
}

Series main_index_quote_binding(const std::string& name,
                                const std::vector<Series>& args,
                                const Environment& env, std::size_t size) {
    require_arity(name, args, 2, 2);
    if (args[0].empty() || !std::isfinite(args[0].back()) ||
        !std::isfinite(args[1].back()))
        return constant(0.0, size);
    const float selector_value = static_cast<float>(args[0].back());
    const float field_value = static_cast<float>(args[1].back());
    if (!std::isfinite(selector_value) || !std::isfinite(field_value) ||
        selector_value >= 2147483648.0f ||
        selector_value < -2147483648.0f ||
        field_value >= 2147483648.0f || field_value < -2147483648.0f)
        return constant(0.0, size);
    const int selector = static_cast<int>(std::trunc(selector_value));
    const int field = static_cast<int>(std::trunc(field_value));
    if (field < 0 || field > 9) return constant(0.0, size);
    const auto key = "MAINZSHQ#" +
        (selector >= 0 && selector <= 5 ? std::to_string(selector)
                                        : std::string("DEFAULT")) +
        "#" + std::to_string(field);
    const auto found = env.find(key);
    if (found == env.end())
        throw Error(
            "MAINZSHQ requires automatic public-index context: " + key);
    return found->second;
}

Series total_market_binding(const std::string& name,
                            const std::vector<Series>& args,
                            const Environment& env, std::size_t size) {
    require_arity(name, args, 1, 1);
    if (args[0].empty() || !std::isfinite(args[0].back()))
        return constant(0.0, size);
    const float field_value = static_cast<float>(args[0].back());
    if (!std::isfinite(field_value) || field_value >= 2147483648.0f ||
        field_value < -2147483648.0f)
        return constant(0.0, size);
    const int field = static_cast<int>(std::trunc(field_value));
    if (field < 1 || field > 6) return constant(0.0, size);
    const auto key = name + "#" + std::to_string(field);
    const auto found = env.find(key);
    if (found == env.end()) {
        if (name == "TOTALMMPAMO" && (field == 5 || field == 6))
            throw Error("TOTALMMPAMO(" + std::to_string(field) +
                        ") requires Level2 total-order context");
        if (name == "TOTALMMPAMO")
            throw Error(
                "TOTALMMPAMO requires automatic public-L1 market-amount context: " +
                key);
        throw Error(
            "TOTALHQINFO requires automatic public-market context: " + key);
    }
    return found->second;
}

Series status_binding(const std::string& name,
                      const std::vector<Series>& args,
                      const Environment& env, std::size_t) {
    require_arity(name, args, 0, 0);
    const auto found = env.find(name);
    if (found == env.end())
        throw Error(
            "missing formula security-status context binding: " + name);
    return found->second;
}

Series block_text_surrogate(const std::string& name,
                            const std::vector<Series>& args,
                            const Environment&, std::size_t size) {
    const std::size_t expected = name == "GETNAMEOFCODE" ? 2 : 1;
    require_arity(name, args, expected, expected);
    return constant(0.0, size);
}

bool final_raw_float_selector(const Series& argument, int& selector) {
    if (argument.empty()) return false;
    const float value = static_cast<float>(argument.back());
    if (!std::isfinite(value) || value >= 2147483648.0f ||
        value < -2147483648.0f)
        return false;
    selector = static_cast<int>(std::trunc(value));
    return true;
}

Series raw_float_context_series(const Series& source, std::size_t size) {
    Series out(size, missing);
    constexpr float tcalc_missing_sentinel = -4.0398103e34F;
    for (std::size_t i = 0; i < size; ++i) {
        if (i >= source.size() || !std::isfinite(source[i])) continue;
        const float value = static_cast<float>(source[i]);
        if (std::isfinite(value) && value != tcalc_missing_sentinel)
            out[i] = static_cast<double>(value);
    }
    return out;
}

Series numbered_context_binding(const std::string& name,
                                const std::vector<Series>& args,
                                const Environment& env, std::size_t size) {
    require_arity(name, args, 1, 1);
    if (size == 0 || args[0].empty()) return {};
    Series out(size, missing);
    // FINANCE/DYNAINFO (TCalc!sub_10026B20) and FINVALUE
    // (TCalc!sub_10011A00) all read the selector from the final raw-f32
    // argument before invoking their host-backed lookup.  FINVALUE then
    // produces an as-of series while the shared evaluator may produce either
    // a series or a broadcast value, but every public result is stored in a
    // float buffer.
    int selector = 0;
    if (!final_raw_float_selector(args[0], selector)) return out;
    const auto key = name + "#" + std::to_string(selector);
    const auto found = env.find(key);
    if (found == env.end())
        throw Error("missing formula context binding: " + key);
    return raw_float_context_series(found->second, size);
}

Series professional_value_binding(const std::string& name,
                                  const std::vector<Series>& args,
                                  const Environment& env, std::size_t size) {
    require_arity(name, args, 3, 3);
    Series out(size, missing);
    // GPJYVALUE/BKJYVALUE/SCJYVALUE read all three selectors from the final
    // raw-f32 argument slots before invoking TCalc's host type-174 lookup.
    // The caller-supplied binding already owns the host/date projection; this
    // layer only mirrors native selector choice and public float-buffer output.
    int data_id = 0;
    int value_field = 0;
    int date_mode = 0;
    if (!final_raw_float_selector(args[0], data_id) ||
        !final_raw_float_selector(args[1], value_field) ||
        !final_raw_float_selector(args[2], date_mode))
        return out;
    const auto key = name + "#" + std::to_string(data_id) + "#" +
        std::to_string(value_field) + "#" + std::to_string(date_mode);
    const auto found = env.find(key);
    if (found == env.end())
        throw Error("missing formula context binding: " + key);
    return raw_float_context_series(found->second, size);
}

Series single_point_binding(const std::string& name,
                            const std::vector<Series>& args,
                            const Environment& env, std::size_t size) {
    const std::size_t expected =
        name == "GPONEDAT" ? 1 : name == "FINONE" ? 3 : 4;
    require_arity(name, args, expected, expected);
    Series out(size, missing);
    for (std::size_t i = 0; i < size; ++i) {
        bool valid = true;
        std::string key = name;
        for (const auto& argument : args) {
            if (!std::isfinite(argument[i])) {
                valid = false;
                break;
            }
            key += "#" +
                   std::to_string(static_cast<int>(argument[i]));
        }
        if (!valid) continue;
        const auto found = env.find(key);
        if (found == env.end())
            throw Error("missing formula context binding: " + key);
        out[i] = found->second[i];
    }
    return out;
}

Series two_selector_binding(const std::string& name,
                            const std::vector<Series>& args,
                            const Environment& env, std::size_t size) {
    require_arity(name, args, 2, 2);
    Series out(size, missing);
    for (std::size_t i = 0; i < size; ++i) {
        if (!std::isfinite(args[0][i]) || !std::isfinite(args[1][i]))
            continue;
        const auto key = name + "#" +
            std::to_string(static_cast<int>(args[0][i])) + "#" +
            std::to_string(static_cast<int>(args[1][i]));
        const auto found = env.find(key);
        if (found == env.end()) {
            if (name == "SIGNALS_QS")
                throw Error(
                    "missing explicit broker formula context binding: " + key);
            if (name == "L2_AMO" || name == "L2_VOL" ||
                name == "L2_VOLNUM")
                throw Error(
                    "missing explicit authorized Level2 formula context binding: " +
                    key);
            throw Error("missing formula context binding: " + key);
        }
        out[i] = found->second[i];
    }
    return out;
}

Series broker_signal_binding(const std::string& name,
                             const std::vector<Series>& args,
                             const Environment& env, std::size_t size) {
    require_arity(name, args, 2, 2);
    Series out(size, missing);
    // TCalc!sub_100111B0 reads both selectors from the final raw-f32
    // argument slots.  Its host type-35 callback returns sparse dated values;
    // this caller-owned aligned binding represents that sparse result, while
    // mode 1 carries the prior output and mode 2 fills absent dates with zero.
    int signal_id = 0;
    int mode = 0;
    if (!final_raw_float_selector(args[0], signal_id) ||
        !final_raw_float_selector(args[1], mode))
        return out;
    const auto key = name + "#" + std::to_string(signal_id) + "#" +
        std::to_string(mode);
    const auto found = env.find(key);
    if (found == env.end())
        throw Error("missing explicit broker formula context binding: " + key);

    constexpr float tcalc_missing_sentinel = -4.0398103e34F;
    for (std::size_t i = 0; i < size; ++i) {
        bool matched = false;
        if (i < found->second.size() && std::isfinite(found->second[i])) {
            const float value = static_cast<float>(found->second[i]);
            if (std::isfinite(value) && value != tcalc_missing_sentinel) {
                out[i] = static_cast<double>(value);
                matched = true;
            }
        }
        if (matched) continue;
        if (mode == 2)
            out[i] = 0.0;
        else if (mode == 1 && i > 0)
            out[i] = out[i - 1];
    }
    return out;
}

Series level2_amount_binding(const std::string& name,
                             const std::vector<Series>& args,
                             const Environment& env, std::size_t size) {
    require_arity(name, args, 2, 2);
    Series out(size, missing);
    // TCalc!sub_10036EE0 reads both L2_AMO selectors from the final raw-f32
    // argument slots, accepts only the unsigned 0..3 grid, initializes the
    // result buffer to the canonical sentinel, and copies matched type-168
    // fields through float stores.  The binding remains caller-supplied.
    int category = 0;
    int side = 0;
    if (!final_raw_float_selector(args[0], category) ||
        !final_raw_float_selector(args[1], side) ||
        category < 0 || category > 3 || side < 0 || side > 3)
        return out;
    const auto key = name + "#" + std::to_string(category) + "#" +
        std::to_string(side);
    const auto found = env.find(key);
    if (found == env.end())
        throw Error(
            "missing explicit authorized Level2 formula context binding: " +
            key);
    return raw_float_context_series(found->second, size);
}

Series level2_scalar_binding(const std::string& name,
                             const std::vector<Series>& args,
                             const Environment& env, std::size_t size) {
    return required_binding(name, args, env, size,
                            "authorized Level2 order-flow");
}

Series trading_signal(const std::string& name,
                      const std::vector<Series>& args,
                      const Environment&, std::size_t size) {
    require_arity(name, args, 2, 2);
    Series out = args[0];
    for (std::size_t i = 0; i < size; ++i)
        if (!std::isfinite(args[0][i]) || !std::isfinite(args[1][i]))
            out[i] = 0.0;
    return out;
}

const std::unordered_map<std::string_view, FunctionHandler>&
context_registry() {
    static const std::unordered_map<std::string_view, FunctionHandler> registry{
        {"BETAVALUE", security_stat_binding},
        {"SHAPE_LONG", security_stat_binding},
        {"SHAPE_MID", security_stat_binding},
        {"SHAPE_SHORT", security_stat_binding},
        {"HYSJL", industry_valuation_binding},
        {"HYSYL", industry_valuation_binding},
        {"INDEXADV", market_breadth_binding},
        {"INDEXDEC", market_breadth_binding},
        {"DYNA_LB", dynamic_quote_binding},
        {"DYNA_NOW", dynamic_quote_binding},
        {"DYNA_ZAF", dynamic_quote_binding},
        {"DYNA_ZAS", dynamic_quote_binding},
        {"DPZSCODE", security_relation_binding},
        {"DPZSNAME", security_relation_binding},
        {"UNDERCODE", security_relation_binding},
        {"UNDERLYC", security_relation_binding},
        {"DIVFACTOR", divfactor_binding},
        {"MAINZSHQ", main_index_quote_binding},
        {"TOTALHQINFO", total_market_binding},
        {"TOTALMMPAMO", total_market_binding},
        {"IST0CODE", status_binding},
        {"ISSTCODE", status_binding},
        {"ISQUITCODE", status_binding},
        {"ISQHQQCODE", status_binding},
        {"ISJYDATE", status_binding},
        {"LOCALDAYNUM", status_binding},
        {"GNBKZSCODE", block_text_surrogate},
        {"FGBKZSCODE", block_text_surrogate},
        {"GETNAMEOFCODE", block_text_surrogate},
        {"FINANCE", numbered_context_binding},
        {"FINVALUE", numbered_context_binding},
        {"DYNAINFO", numbered_context_binding},
        {"GPJYVALUE", professional_value_binding},
        {"BKJYVALUE", professional_value_binding},
        {"SCJYVALUE", professional_value_binding},
        {"FINONE", single_point_binding},
        {"GPJYONE", single_point_binding},
        {"BKJYONE", single_point_binding},
        {"SCJYONE", single_point_binding},
        {"GPONEDAT", single_point_binding},
        {"SPLIT", two_selector_binding},
        {"SPLITBARS", two_selector_binding},
        {"SIGNALS_QS", broker_signal_binding},
        {"L2_AMO", level2_amount_binding},
        {"L2_VOL", two_selector_binding},
        {"L2_VOLNUM", two_selector_binding},
        {"ACTINVOL", level2_scalar_binding},
        {"ACTOUTVOL", level2_scalar_binding},
        {"AVGBIDPX", level2_scalar_binding},
        {"AVGOFFERPX", level2_scalar_binding},
        {"BIDCANCELVOL", level2_scalar_binding},
        {"BIDORDERVOL", level2_scalar_binding},
        {"CUR_BUYORDER", level2_scalar_binding},
        {"CUR_SELLORDER", level2_scalar_binding},
        {"ISBUYORDER", level2_scalar_binding},
        {"OFFERCANCELVOL", level2_scalar_binding},
        {"OFFERORDERVOL", level2_scalar_binding},
        {"BUY", trading_signal},
        {"SELL", trading_signal},
        {"BUYSHORT", trading_signal},
        {"SELLSHORT", trading_signal},
        {"BUYSHORT_BUY", trading_signal},
        {"SELL_SELLSHORT", trading_signal},
    };
    return registry;
}

}  // namespace

std::optional<Series> evaluate_context_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    const auto found = context_registry().find(name);
    if (found == context_registry().end()) return std::nullopt;
    return found->second(name, args, env, size);
}

}  // namespace tdx::formula_engine_detail
