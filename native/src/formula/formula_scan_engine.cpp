#include "formula_scan_internal.hpp"

namespace tdx {
using namespace formula_scan_detail;

Json scan_formula_documents(const std::vector<Json>& kline_documents, const Json& formula,
                            const std::map<std::string, double>& parameters, int lookback) {
    if (lookback < 1 || lookback > 10000) throw Error("formula scan lookback must be in 1..10000");
    const auto analysis = analyze_selected_formula(formula);
    if (!analysis.at("numeric_signal_safe").as_bool())
        throw Error("formula scan rejected degraded numeric output; inspect "
                    "degraded_numeric_output_causes with formulas analyze");
    std::set<std::string> presentation_only_outputs;
    if (const auto* values = optional(analysis, "presentation_only_outputs");
        values && values->is_array())
        for (const auto& value : values->as_array())
            if (value.is_string()) presentation_only_outputs.insert(value.as_string());
    Json matches = Json::array(), errors = Json::array(); std::uint64_t evaluated = 0;
    for (const auto& kline : kline_documents) {
        try {
            const auto* context = optional(kline, "formula_context");
            const auto result = evaluate_formula_document(kline, formula, parameters,
                                                          context && context->is_object() ? context : nullptr);
            ++evaluated; const auto& points = result.at("points").as_array();
            Json signals = Json::array(); std::string trigger_date, trigger_time;
            const auto begin = points.size() > static_cast<std::size_t>(lookback) ? points.size() - lookback : 0;
            for (std::size_t i = begin; i < points.size(); ++i) {
                for (const auto& [name, value] : points[i].at("values").as_object()) {
                    if (presentation_only_outputs.count(name)) continue;
                    if (value.is_number() && std::abs(value.as_number()) > 1e-12) {
                        Json signal = Json::object(); signal["output"] = name; signal["value"] = value;
                        signal["date"] = points[i].at("date"); signal["time"] = points[i].at("time");
                        signals.push_back(std::move(signal)); trigger_date = points[i].at("date").as_string();
                        trigger_time = points[i].at("time").as_string();
                    }
                }
            }
            if (signals.size()) {
                Json row = Json::object();
                for (const auto key : {"market", "code", "name", "period",
                                       "adjustment_mode"})
                    if (const auto* value = optional(result, key)) row[key] = *value;
                row["security_id"] = (optional(result, "market") ? result.at("market").as_string() : "") +
                                     (optional(result, "code") ? result.at("code").as_string() : "");
                row["trigger_date"] = trigger_date; row["trigger_time"] = trigger_time;
                if (const auto* metadata = optional(result, "context_metadata"))
                    row["context_metadata"] = *metadata;
                row["signals"] = std::move(signals); matches.push_back(std::move(row));
            }
        } catch (const std::exception& error) {
            Json row = Json::object(); row["error"] = error.what();
            for (const auto key : {"market", "code", "name"}) if (const auto* value = optional(kline, key)) row[key] = *value;
            errors.push_back(std::move(row));
        }
    }
    Json result = Json::object(); result["schema_version"] = 1; result["engine"] = "tdx-source-interpreter-v1";
    result["formula"] = formula.at("code"); result["kind"] = formula.at("kind_key"); result["lookback"] = lookback;
    result["analysis"] = analysis;
    result["input_count"] = static_cast<std::uint64_t>(kline_documents.size()); result["evaluated"] = evaluated;
    result["match_count"] = static_cast<std::uint64_t>(matches.size()); result["error_count"] = static_cast<std::uint64_t>(errors.size());
    result["matches"] = std::move(matches); result["errors"] = std::move(errors); return result;
}

}  // namespace tdx
