#include "formula_strategy_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_engine.hpp"

#include <cmath>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace tdx {

using namespace formula_strategy_detail;

Json evaluate_formula_strategy_document(Json kline_document, const Json& strategy) {
    if (!strategy.is_object() || !optional(strategy, "rules") ||
        !strategy.at("rules").is_array() || strategy.at("rules").size() == 0)
        throw Error("normalized formula strategy has no rules");
    struct Execution {
        std::string id;
        Json document;
        std::set<std::string> presentation;
    };
    std::vector<Execution> executions;
    const Json* contexts = optional(kline_document, "formula_strategy_contexts");
    const Json* common_context = optional(kline_document, "formula_context");
    for (const auto& rule : strategy.at("rules").as_array()) {
        const auto id = rule.at("id").as_string();
        const Json* context = nullptr;
        if (contexts && contexts->is_object()) context = optional(*contexts, id);
        if (!context && common_context && common_context->is_object()) context = common_context;
        auto parameters = parameter_object(optional(rule, "parameters"));
        auto document = evaluate_formula_document(
            kline_document, rule.at("formula_definition"), parameters,
            context && context->is_object() ? context : nullptr);
        executions.push_back(Execution{
            id, std::move(document), presentation_outputs(rule.at("analysis"))});
    }
    const auto& base = executions.front().document.at("points").as_array();
    for (const auto& execution : executions) {
        const auto& points = execution.document.at("points").as_array();
        if (points.size() != base.size())
            throw Error("formula strategy rule point counts are not aligned");
        for (std::size_t index = 0; index < base.size(); ++index)
            if (point_key(points[index]) != point_key(base[index]))
                throw Error("formula strategy rule dates/times are not aligned");
    }
    const int minimum = static_cast<int>(strategy.at("minimum_matches").as_number());
    Json points = Json::array();
    for (std::size_t index = 0; index < base.size(); ++index) {
        Json point = Json::object();
        for (const auto* key : {"date", "time", "open", "high", "low", "close",
                                "volume", "amount"})
            if (const auto* value = optional(base[index], key)) point[key] = *value;
        Json rule_rows = Json::array();
        Json matched_ids = Json::array();
        int matched_count = 0;
        for (const auto& execution : executions) {
            const auto& source_point = execution.document.at("points").as_array()[index];
            Json signals = Json::array();
            for (const auto& [name, value] : source_point.at("values").as_object()) {
                if (execution.presentation.count(name)) continue;
                if (value.is_number() && std::abs(value.as_number()) > 1e-12) {
                    Json signal = Json::object();
                    signal["output"] = name;
                    signal["value"] = value;
                    signals.push_back(std::move(signal));
                }
            }
            const bool matched = signals.size() > 0;
            if (matched) {
                ++matched_count;
                matched_ids.push_back(execution.id);
            }
            Json row = Json::object();
            row["id"] = execution.id;
            row["matched"] = matched;
            row["signals"] = std::move(signals);
            rule_rows.push_back(std::move(row));
        }
        point["matched"] = matched_count >= minimum;
        point["matched_rule_count"] = matched_count;
        point["matched_rule_ids"] = std::move(matched_ids);
        point["rules"] = std::move(rule_rows);
        points.push_back(std::move(point));
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-formula-strategy-evaluation-v1";
    result["engine"] = "tdx-formula-strategy-v1";
    result["execution_mode"] = "native-cpp";
    result["strategy"] = strategy_summary(strategy);
    result["count"] = static_cast<std::uint64_t>(points.size());
    result["points"] = std::move(points);
    for (const auto* key : {"market", "code", "name", "period",
                            "adjustment_mode", "adjustment"})
        if (const auto* value = optional(executions.front().document, key)) result[key] = *value;
    return result;
}

}  // namespace tdx
