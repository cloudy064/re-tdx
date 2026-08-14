#include "formula_environment_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_runtime_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;

namespace {

// Converts a strict YYYY-MM-DD context date to a calendar day number, or
// `missing` when the text is malformed.  Context dates come from callers, so any
// deviation is skipped rather than raised.
double iso_date_day_number(const std::string& value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') return missing;
    try {
        return static_cast<double>(calendar_day_number(
            std::stoi(value.substr(0, 4)),
            static_cast<unsigned>(std::stoi(value.substr(5, 2))),
            static_cast<unsigned>(std::stoi(value.substr(8, 2)))));
    } catch (...) { return missing; }
}

// These four context symbols are produced by dedicated TCalc handlers whose
// public result buffers are raw float32 arrays.  Keep the narrowing local to
// those proven direct bindings. Numbered FINANCE/FINVALUE/DYNAINFO values are
// narrowed by their function handler after its final-bar selector is resolved;
// the other caller-owned context values must not inherit either rule.
bool tcalc_raw_float_context_binding(std::string_view name) {
    return name == "CAPITAL" || name == "TOTALCAPITAL" ||
           name == "MINDIFF" || name == "MULTIPLIER";
}

double tcalc_context_binding_value(std::string_view name, double value) {
    return tcalc_raw_float_context_binding(name) ? native_float(value) : value;
}

}  // namespace

Json FormulaEnvironmentBuilder::bind_context(
    Environment& env, StringEnvironment& string_env) const {
    Json names = Json::array();
    if (!context_ || !context_->is_object()) return names;
    bind_context_text_symbols(string_env);
    bind_context_external_signals(env, string_env, names);
    bind_context_catalog(string_env);
    bind_context_numeric_groups(env, names);
    bind_context_symbol_series(env, names);
    // Reads CAPITAL, so it has to follow every phase that can bind it.
    bind_context_turnover(env, names);
    bind_context_ivolat(env, names);
    return names;
}

void FormulaEnvironmentBuilder::bind_context_text_symbols(
    StringEnvironment& string_env) const {
    const auto bind_text_symbols = [&](std::string_view field) {
        if (const auto* text_symbols = optional(*context_, field);
            text_symbols && text_symbols->is_object())
            for (const auto& [key, value] : text_symbols->as_object())
                if (value.is_string())
                    string_env[upper_ascii(key)] =
                        StringSeries(bars_.size(), value.as_string());
    };
    bind_text_symbols("formula_text_symbols");
    bind_text_symbols("formula_private_text_symbols");
}

void FormulaEnvironmentBuilder::bind_context_external_signals(
    Environment& env, StringEnvironment& string_env, Json& names) const {
    if (const auto* ready = optional(*context_, "formula_external_signals_ready");
        ready && ready->is_bool() && ready->as_bool()) {
        env["__EXTERN_SIGNALS_READY"] = constant(1.0, bars_.size());
        if (const auto* values = optional(*context_, "formula_external_values");
            values && values->is_object())
            for (const auto& [key, value] : values->as_object())
                if (value.is_number()) {
                    env["__EXTERNVALUE#" + key] =
                        constant(value.as_number(), bars_.size());
                    names.push_back("EXTERNVALUE#" + key);
                }
        if (const auto* texts = optional(
                *context_, "formula_external_text_values");
            texts && texts->is_object())
            for (const auto& [key, value] : texts->as_object())
                if (value.is_string()) {
                    string_env["__EXTERNSTR#" + key] =
                        StringSeries(bars_.size(), value.as_string());
                    names.push_back("EXTERNSTR#" + key);
                }
    }
    if (const auto* ready = optional(*context_, "formula_external_series_ready");
        ready && ready->is_bool() && ready->as_bool())
        env["__EXTDATA_USER_READY"] = constant(1.0, bars_.size());
}

void FormulaEnvironmentBuilder::bind_context_catalog(
    StringEnvironment& string_env) const {
    if (const auto* overrides = optional(*context_, "formula_code_name_overrides");
        overrides && overrides->is_object()) {
        for (const auto& [key, value] : overrides->as_object()) {
            if (!value.is_string()) continue;
            const auto separator = key.find('|');
            if (separator == std::string::npos) continue;
            try {
                std::size_t used = 0;
                const int market = std::stoi(key.substr(0, separator), &used);
                if (used != separator) continue;
                string_env.code_name_overrides[
                    {market, key.substr(separator + 1)}] = value.as_string();
            } catch (...) { /* Ignore malformed private override keys. */ }
        }
    }
    if (const auto* root = optional(*context_, "formula_code_name_root");
        root && root->is_string() && !root->as_string().empty())
        string_env.security_catalog =
            cached_security_catalog(formula_path_from_utf8(root->as_string()));
}

void FormulaEnvironmentBuilder::bind_context_numeric_groups(
    Environment& env, Json& names) const {
    const auto bind_group = [&](std::string_view group) {
        const auto found = context_->as_object().find(group);
        if (found == context_->as_object().end() || !found->second.is_object()) return;
        for (const auto& [key, value] : found->second.as_object()) {
            if (!value.is_number()) continue;
            const auto binding = upper_ascii(std::string(group)) + "#" + key;
            env[binding] = constant(value.as_number(), bars_.size());
            names.push_back(binding);
        }
    };
    bind_group("finance"); bind_group("finvalue"); bind_group("dynainfo");
    if (const auto* scalars = optional(*context_, "formula_scalar_bindings");
        scalars && scalars->is_object()) {
        for (const auto& [key, value] : scalars->as_object()) {
            if (!value.is_number() && !value.is_null()) continue;
            const auto binding = upper_ascii(key);
            env[binding] = constant(
                value.is_number()
                    ? tcalc_context_binding_value(binding, value.as_number())
                    : missing,
                bars_.size());
            names.push_back(binding);
        }
    }
}

void FormulaEnvironmentBuilder::bind_context_symbol_series(
    Environment& env, Json& names) const {
    const auto symbols = context_->as_object().find("symbols");
    if (symbols != context_->as_object().end() && symbols->second.is_object()) {
        for (const auto& [key, value] : symbols->second.as_object())
            if (value.is_number()) {
                const auto binding = upper_ascii(key);
                env[binding] = constant(
                    tcalc_context_binding_value(binding, value.as_number()),
                    bars_.size());
                names.push_back(binding);
            }
    }
    const auto series = context_->as_object().find("series");
    if (series != context_->as_object().end() && series->second.is_object()) {
        for (const auto& [key, values] : series->second.as_object()) {
            if (!values.is_object()) continue;
            const auto name = upper_ascii(key);
            Series binding(bars_.size(), missing);
            for (std::size_t i = 0; i < bars_.size(); ++i) {
                const auto point = values.as_object().find(
                    bars_[i].date + "|" + bars_[i].time);
                if (point != values.as_object().end() && point->second.is_number())
                    binding[i] = tcalc_context_binding_value(
                        name, point->second.as_number());
            }
            env[name] = std::move(binding);
            names.push_back(name);
        }
    }
}

void FormulaEnvironmentBuilder::bind_context_turnover(
    Environment& env, Json& names) const {
    const auto capital = env.find("CAPITAL");
    if (capital == env.end()) return;
    Series turnover(bars_.size(), missing);
    for (std::size_t i = 0; i < bars_.size(); ++i)
        if (std::isfinite(capital->second[i]) && std::abs(capital->second[i]) > 1e-15)
            turnover[i] = 100.0 * env.at("VOL")[i] / capital->second[i];
    env["HSL"] = std::move(turnover);
    names.push_back("HSL");
}

void FormulaEnvironmentBuilder::bind_context_ivolat(
    Environment& env, Json& names) const {
    const auto* ivolat = optional(*context_, "ivolat");
    if (!ivolat || !ivolat->is_object()) return;
    const auto bind_scalar = [&](std::string_view field, std::string_view name,
                                 double fallback) {
        const auto* value = optional(*ivolat, field);
        env[std::string(name)] = constant(
            value && value->is_number() ? value->as_number() :
            value && value->is_bool() ? (value->as_bool() ? 1.0 : 0.0) : fallback,
            bars_.size());
    };
    bind_scalar("strike", "__IVOLAT_STRIKE", missing);
    bind_scalar("divisor", "__IVOLAT_DIVISOR", 1.0);
    bind_scalar("risk_free", "__IVOLAT_RISK_FREE", 0.0187);
    bind_scalar("call", "__IVOLAT_CALL", 0.0);
    bind_scalar("american", "__IVOLAT_AMERICAN", 0.0);
    bind_scalar("futures_model", "__IVOLAT_FUTURES_MODEL", 0.0);
    double expiry_day = missing;
    if (const auto* expiry = optional(*ivolat, "expiry");
        expiry && expiry->is_string())
        expiry_day = iso_date_day_number(expiry->as_string());
    env["__IVOLAT_EXPIRY_DAY"] = constant(expiry_day, bars_.size());
    Series history_days, history_closes;
    if (const auto* history = optional(*ivolat, "history");
        history && history->is_array()) {
        for (const auto& point : history->as_array()) {
            const auto* date = optional(point, "date");
            const auto* close = optional(point, "close");
            if (!date || !date->is_string() || !close || !close->is_number()) continue;
            const auto day = iso_date_day_number(date->as_string());
            if (!std::isfinite(day)) continue;
            history_days.push_back(day);
            history_closes.push_back(close->as_number());
        }
    }
    env["__IVOLAT_HISTORY_DAY"] = std::move(history_days);
    env["__IVOLAT_HISTORY_CLOSE"] = std::move(history_closes);
    names.push_back("IVOLAT");
}

}  // namespace tdx::formula_runtime_detail
