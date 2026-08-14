#include "formula_strategy_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::formula_strategy_detail {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return ch < 0x80 ? static_cast<char>(std::toupper(ch)) : static_cast<char>(ch);
    });
    return value;
}

std::string required_text(const Json& object, std::string_view key) {
    const auto* value = optional(object, key);
    if (!value || !value->is_string() || trim(value->as_string()).empty())
        throw Error("formula strategy requires non-empty " + std::string(key));
    return trim(value->as_string());
}

bool valid_identifier(const std::string& value) {
    if (value.empty() || value.size() > 64 ||
        !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_'))
        return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '_';
    });
}

std::map<std::string, double> parameter_object(const Json* value) {
    std::map<std::string, double> result;
    if (!value) return result;
    if (!value->is_object()) throw Error("formula strategy parameters must be an object");
    if (value->size() > 64) throw Error("formula strategy rule has more than 64 parameters");
    for (const auto& [raw_name, raw_value] : value->as_object()) {
        const auto name = upper_ascii(trim(raw_name));
        if (!valid_identifier(name) || !raw_value.is_number() ||
            !std::isfinite(raw_value.as_number()))
            throw Error("formula strategy parameter must use NAME:number");
        result[name] = raw_value.as_number();
    }
    return result;
}

Json parameter_json(const std::map<std::string, double>& parameters) {
    Json result = Json::object();
    for (const auto& [name, value] : parameters) result[name] = value;
    return result;
}

const Json& find_selection_formula(const Json& library, const std::string& code) {
    if (!library.is_object() || !optional(library, "formulas") ||
        !library.at("formulas").is_array())
        throw Error("formula strategy library has no formulas array");
    const auto wanted = upper_ascii(trim(code));
    for (const auto& formula : library.at("formulas").as_array()) {
        if (upper_ascii(formula.at("code").as_string()) == wanted &&
            formula.at("kind_key").as_string() == "selection")
            return formula;
    }
    throw Error("selection formula not found for strategy rule: " + code);
}

Json formula_analysis(const Json& formula) {
    if (const auto* analysis = optional(formula, "analysis");
        analysis && analysis->is_object()) return *analysis;
    std::vector<std::string> names;
    if (const auto* parameters = optional(formula, "parameters");
        parameters && parameters->is_array())
        for (const auto& parameter : parameters->as_array())
            if (const auto* name = optional(parameter, "name"); name && name->is_string())
                names.push_back(name->as_string());
    return analyze_formula_source(required_text(formula, "source_text"), names);
}

std::set<std::string> presentation_outputs(const Json& analysis) {
    std::set<std::string> result;
    if (const auto* values = optional(analysis, "presentation_only_outputs");
        values && values->is_array())
        for (const auto& value : values->as_array())
            if (value.is_string()) result.insert(value.as_string());
    return result;
}

Json strategy_summary(const Json& strategy) {
    Json result = Json::object();
    for (const auto* key : {"code", "name", "operator", "minimum_matches",
                            "rule_count", "signal_alignment"})
        if (const auto* value = optional(strategy, key)) result[key] = *value;
    Json rules = Json::array();
    for (const auto& rule : strategy.at("rules").as_array()) {
        Json row = Json::object();
        for (const auto* key : {"id", "label", "formula_code", "source_mode",
                                "source_md5", "parameters", "analysis"})
            if (const auto* value = optional(rule, key)) row[key] = *value;
        rules.push_back(std::move(row));
    }
    result["rules"] = std::move(rules);
    return result;
}

std::string point_key(const Json& point) {
    return point.at("date").as_string() + "|" + point.at("time").as_string();
}

bool has_external_dependency(const Json& analysis) {
    const auto* value = optional(analysis, "has_external_dependency");
    return value && value->is_bool() && value->as_bool();
}

bool has_finance_dependency(const Json& analysis) {
    const auto* values = optional(analysis, "external_dependencies");
    if (!values || !values->is_array()) return false;
    for (const auto& value : values->as_array())
        if (value.is_string() &&
            (value.as_string() == "FINANCE" || value.as_string() == "FINVALUE"))
            return true;
    return false;
}

std::set<std::string> dependencies(const Json& analysis) {
    std::set<std::string> result;
    if (const auto* values = optional(analysis, "external_dependencies");
        values && values->is_array())
        for (const auto& value : values->as_array())
            if (value.is_string()) result.insert(value.as_string());
    return result;
}

SecurityKey parse_security(std::string value) {
    value = lower_ascii(trim(value));
    SecurityKey key;
    const auto colon = value.find(':');
    if (colon != std::string::npos) {
        key.market = value.substr(0, colon);
        key.code = value.substr(colon + 1);
    } else if (value.size() == 8 &&
               (value.rfind("sz", 0) == 0 || value.rfind("sh", 0) == 0 ||
                value.rfind("bj", 0) == 0)) {
        key.market = value.substr(0, 2);
        key.code = value.substr(2);
    } else {
        key.code = value;
        if (value.size() == 6)
            key.market = value.rfind("92", 0) == 0 || value[0] == '8' ? "bj" :
                         value[0] == '6' || value[0] == '9' ? "sh" : "sz";
    }
    if ((key.market != "sz" && key.market != "sh" && key.market != "bj") ||
        key.code.size() != 6 ||
        !std::all_of(key.code.begin(), key.code.end(),
                     [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("invalid formula strategy security: " + value);
    return key;
}

std::vector<Json> kline_documents(const Json& document) {
    if (document.is_object() && optional(document, "bars")) return {document};
    const Json* rows = document.is_array() ? &document : optional(document, "securities");
    std::vector<Json> result;
    if (!rows || !rows->is_array()) return result;
    for (const auto& row : rows->as_array()) {
        if (row.is_object() && optional(row, "bars")) result.push_back(row);
        else if (row.is_object()) {
            const auto* kline = optional(row, "kline");
            if (kline && kline->is_object() && optional(*kline, "bars"))
                result.push_back(*kline);
        }
    }
    return result;
}

std::vector<SecurityKey> securities_from_document(const Json& document) {
    const Json* rows = document.is_array() ? &document : optional(document, "securities");
    std::vector<SecurityKey> result;
    if (!rows || !rows->is_array()) return result;
    for (const auto& row : rows->as_array()) {
        if (!row.is_object() || optional(row, "bars")) continue;
        const auto* market = optional(row, "market");
        const auto* code = optional(row, "code");
        if (!market || !market->is_string() || !code || !code->is_string()) continue;
        auto key = parse_security(market->as_string() + ":" + code->as_string());
        if (const auto* name = optional(row, "name"); name && name->is_string())
            key.name = name->as_string();
        result.push_back(std::move(key));
    }
    return result;
}

int integer_option(Args& args, std::string_view name, int fallback,
                   int minimum, int maximum) {
    const auto raw = args.take_option(name, std::to_string(fallback));
    std::size_t used = 0;
    int value = 0;
    try { value = std::stoi(raw, &used); }
    catch (...) { throw Error(std::string(name) + " must be an integer"); }
    if (used != raw.size() || value < minimum || value > maximum)
        throw Error(std::string(name) + " is outside the safe range");
    return value;
}

double number_option(Args& args, std::string_view name, double fallback,
                     double minimum, double maximum) {
    const auto raw = args.take_option(name, std::to_string(fallback));
    std::size_t used = 0;
    double value = 0;
    try { value = std::stod(raw, &used); }
    catch (...) { throw Error(std::string(name) + " must be numeric"); }
    if (used != raw.size() || !std::isfinite(value) || value < minimum || value > maximum)
        throw Error(std::string(name) + " is outside the safe range");
    return value;
}

bool manifest_needs_library(const Json& manifest) {
    const auto* rules = optional(manifest, "rules");
    if (!rules || !rules->is_array()) return false;
    for (const auto& rule : rules->as_array())
        if (rule.is_object() && optional(rule, "formula")) return true;
    return false;
}

void attach_strategy_contexts(Json& kline, const Json& strategy,
                              const fs::path& root, int timeout,
                              const BlockData* block_data,
                              bool point_in_time_finance,
                              bool historical_backtest,
                              const Json* formula_library) {
    Json contexts = Json::object();
    for (const auto& rule : strategy.at("rules").as_array()) {
        const auto& analysis = rule.at("analysis");
        if (!has_external_dependency(analysis)) continue;
        const auto deps = dependencies(analysis);
        if (historical_backtest) {
            for (const auto& dependency : deps)
                if (dependency != "FINANCE" && dependency != "FINVALUE" &&
                    !formula_engine_support::supported_formula_reference_dependency(
                        dependency))
                    throw Error("strategy portfolio backtest does not admit external dependency " +
                                dependency + "; point-in-time safety is unproven");
            if (has_finance_dependency(analysis) && !point_in_time_finance)
                throw Error("strategy finance backtest requires --point-in-time-finance");
        }
        const auto market = required_text(kline, "market");
        const auto code = required_text(kline, "code");
        const auto rule_parameters = parameter_object(
            optional(rule, "parameters"));
        contexts[rule.at("id").as_string()] = build_formula_market_context_document(
            root, market, code, analysis, timeout, block_data, &kline,
            point_in_time_finance, formula_library, {}, nullptr,
            &rule_parameters);
    }
    if (contexts.size()) kline["formula_strategy_contexts"] = std::move(contexts);
}

}  // namespace tdx::formula_strategy_detail
