#include "formula_strategy_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formulas.hpp"

#include <cstdint>
#include <set>
#include <utility>

namespace tdx {

using namespace formula_strategy_detail;

Json normalize_formula_strategy_document(const Json& manifest, const Json* library) {
    if (!manifest.is_object()) throw Error("formula strategy manifest must be an object");
    const auto* raw_rules = optional(manifest, "rules");
    if (!raw_rules || !raw_rules->is_array() || raw_rules->size() < 1 ||
        raw_rules->size() > 16)
        throw Error("formula strategy rules must contain 1..16 entries");
    const auto operation = lower_ascii(optional(manifest, "operator") &&
                                       manifest.at("operator").is_string()
        ? trim(manifest.at("operator").as_string()) : "all");
    if (operation != "all" && operation != "any" && operation != "at-least")
        throw Error("formula strategy operator must be all, any, or at-least");
    int minimum = operation == "all" ? static_cast<int>(raw_rules->size()) : 1;
    if (operation == "at-least") {
        const auto* value = optional(manifest, "minimum_matches");
        if (!value || !value->is_number())
            throw Error("at-least strategy requires minimum_matches");
        minimum = static_cast<int>(value->as_number());
        if (value->as_number() != minimum || minimum < 1 ||
            minimum > static_cast<int>(raw_rules->size()))
            throw Error("minimum_matches must be an integer within the rule count");
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-formula-strategy-definition-v1";
    result["code"] = optional(manifest, "code") && manifest.at("code").is_string()
        ? trim(manifest.at("code").as_string()) : "STRATEGY";
    result["name"] = optional(manifest, "name") && manifest.at("name").is_string()
        ? trim(manifest.at("name").as_string()) : result.at("code").as_string();
    result["operator"] = operation;
    result["minimum_matches"] = minimum;
    result["rule_count"] = static_cast<std::uint64_t>(raw_rules->size());
    result["signal_alignment"] = "same-security same-date same-time bar close";
    result["rules"] = Json::array();
    std::set<std::string> ids;
    for (const auto& raw_rule : raw_rules->as_array()) {
        if (!raw_rule.is_object()) throw Error("formula strategy rule must be an object");
        const auto id = required_text(raw_rule, "id");
        if (!valid_identifier(id) || !ids.insert(lower_ascii(id)).second)
            throw Error("formula strategy rule id must be unique ASCII identifier");
        const auto* source = optional(raw_rule, "source");
        const auto* formula_code = optional(raw_rule, "formula");
        if ((source && source->is_string() ? 1 : 0) +
            (formula_code && formula_code->is_string() ? 1 : 0) != 1)
            throw Error("formula strategy rule must choose exactly one of source or formula");
        const auto parameters = parameter_object(optional(raw_rule, "parameters"));
        Json definition;
        std::string source_mode;
        if (source && source->is_string()) {
            if (source->as_string().empty() || source->as_string().size() > 16 * 1024)
                throw Error("formula strategy inline source must be in 1..16384 bytes");
            definition = make_formula_source_definition(
                source->as_string(), "STRATEGY_" + upper_ascii(id), "selection",
                parameters);
            source_mode = "inline";
        } else {
            if (!library) throw Error("formula strategy library rule requires a formula library");
            definition = find_selection_formula(*library, formula_code->as_string());
            source_mode = "library";
        }
        const auto analysis = formula_analysis(definition);
        if (!analysis.at("syntax_supported").as_bool() ||
            !analysis.at("numeric_signal_safe").as_bool() ||
            analysis.at("has_future_function").as_bool() ||
            !analysis.at("executable_with_context").as_bool())
            throw Error("formula strategy rule is not scan-safe: " + id);
        const auto& source_text = definition.at("source_text").as_string();
        Json rule = Json::object();
        rule["id"] = id;
        rule["label"] = optional(raw_rule, "label") && raw_rule.at("label").is_string()
            ? raw_rule.at("label") : Json(id);
        rule["formula_code"] = definition.at("code");
        rule["source_mode"] = source_mode;
        rule["source_md5"] = md5_bytes(Bytes(source_text.begin(), source_text.end()));
        rule["parameters"] = parameter_json(parameters);
        rule["analysis"] = analysis;
        rule["formula_definition"] = std::move(definition);
        result["rules"].push_back(std::move(rule));
    }
    return result;
}

}  // namespace tdx
