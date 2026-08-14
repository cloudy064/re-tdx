#include "formula_strategy_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <exception>
#include <utility>

namespace tdx {

using namespace formula_strategy_detail;

Json scan_formula_strategy_documents(const std::vector<Json>& kline_documents,
                                     const Json& strategy, int lookback) {
    if (lookback < 1 || lookback > 10000)
        throw Error("formula strategy lookback must be in 1..10000");
    Json matches = Json::array(), errors = Json::array();
    std::uint64_t evaluated = 0;
    for (const auto& kline : kline_documents) {
        try {
            const auto evaluation = evaluate_formula_strategy_document(kline, strategy);
            ++evaluated;
            const auto& points = evaluation.at("points").as_array();
            const auto begin = points.size() > static_cast<std::size_t>(lookback)
                ? points.size() - static_cast<std::size_t>(lookback) : 0;
            Json triggers = Json::array();
            std::string trigger_date, trigger_time;
            for (std::size_t index = begin; index < points.size(); ++index) {
                if (!points[index].at("matched").as_bool()) continue;
                Json trigger = Json::object();
                for (const auto* key : {"date", "time", "matched_rule_count",
                                        "matched_rule_ids", "rules"})
                    trigger[key] = points[index].at(key);
                trigger_date = points[index].at("date").as_string();
                trigger_time = points[index].at("time").as_string();
                triggers.push_back(std::move(trigger));
            }
            if (!triggers.size()) continue;
            Json row = Json::object();
            for (const auto* key : {"market", "code", "name", "period",
                                    "adjustment_mode"})
                if (const auto* value = optional(evaluation, key)) row[key] = *value;
            row["security_id"] = required_text(evaluation, "market") +
                                 required_text(evaluation, "code");
            row["trigger_date"] = trigger_date;
            row["trigger_time"] = trigger_time;
            row["triggers"] = std::move(triggers);
            matches.push_back(std::move(row));
        } catch (const std::exception& error) {
            Json row = Json::object();
            for (const auto* key : {"market", "code", "name"})
                if (const auto* value = optional(kline, key)) row[key] = *value;
            row["error"] = error.what();
            errors.push_back(std::move(row));
        }
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-formula-strategy-scan-v1";
    result["engine"] = "tdx-formula-strategy-v1";
    result["strategy"] = strategy_summary(strategy);
    result["lookback"] = lookback;
    result["input_count"] = static_cast<std::uint64_t>(kline_documents.size());
    result["evaluated"] = evaluated;
    result["match_count"] = static_cast<std::uint64_t>(matches.size());
    result["error_count"] = static_cast<std::uint64_t>(errors.size());
    result["matches"] = std::move(matches);
    result["errors"] = std::move(errors);
    return result;
}

}  // namespace tdx
