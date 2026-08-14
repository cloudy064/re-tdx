#include "relative_valuation_internal.hpp"

#include "tdx/cloud_resilience.hpp"

#include <algorithm>

namespace tdx::detail::relative_valuation {

int market_id(const std::string& raw) {
    if (raw == "0" || lower_ascii(raw) == "sz") return 0;
    if (raw == "1" || lower_ascii(raw) == "sh") return 1;
    if (raw == "2" || lower_ascii(raw) == "bj") return 2;
    if (digits(raw)) {
        try {
            const int value = std::stoi(raw);
            if (value >= 0 && value <= 255) return value;
        } catch (...) {}
    }
    throw Error("relative valuation row has invalid TDX market: " + raw);
}

std::string market_name(int value) {
    if (value == 0) return "sz";
    if (value == 1) return "sh";
    if (value == 2) return "bj";
    return "tdx-" + std::to_string(value);
}

std::string fallback_index_name(int market, const std::string& code) {
    static const std::map<std::pair<int, std::string>, std::string> values{
        {{1, "000001"}, "上证指数"}, {{1, "000016"}, "上证50"},
        {{1, "000300"}, "沪深300"}, {{1, "000905"}, "中证500"},
        {{0, "399006"}, "创业板指"}};
    const auto found = values.find({market, code});
    return found == values.end() ? std::string{} : found->second;
}

Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto found = securities.find({market, code});
    const std::string name = found == securities.end()
        ? fallback_index_name(market, code) : found->second.name;
    Json result = Json::object();
    result["market"] = market_name(market);
    result["market_id"] = market;
    result["code"] = code;
    const auto prefix = market == 0 ? std::string("SZ") :
        market == 1 ? std::string("SH") :
        market == 2 ? std::string("BJ") :
        std::string("TDX") + std::to_string(market) + ":";
    result["security_id"] = prefix + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

Json relative_position(const std::optional<double>& quantile) {
    Json result = Json::object();
    result["quantile_pct"] = number_json(quantile);
    result["computed_locally"] = true;
    if (!quantile) {
        result["band"] = Json(nullptr);
        result["label"] = Json(nullptr);
    } else if (*quantile <= 20) {
        result["band"] = "very-low";
        result["label"] = "相对估值比分位很低";
    } else if (*quantile <= 40) {
        result["band"] = "low";
        result["label"] = "相对估值比分位偏低";
    } else if (*quantile < 60) {
        result["band"] = "middle";
        result["label"] = "相对估值比分位居中";
    } else if (*quantile < 80) {
        result["band"] = "high";
        result["label"] = "相对估值比分位偏高";
    } else {
        result["band"] = "very-high";
        result["label"] = "相对估值比分位很高";
    }
    return result;
}

bool transient_error(const std::string& message) {
    return detail::is_transient_cloud_error(message);
}

Json source_document(const Json& upstream, const Json& raw_rows,
                     const std::string& transport, const std::string& request_id) {
    Json result = Json::object();
    result["transport"] = transport;
    result["request_id"] = request_id;
    if (const auto* value = field(upstream, "source_file")) result["source_file"] = *value;
    if (const auto* value = field(upstream, "entry")) result["entry"] = *value;
    if (const auto* value = field(upstream, "module")) result["module"] = *value;
    if (const auto* value = field(upstream, "rpc_id")) result["rpc_id"] = *value;
    if (const auto* value = field(upstream, "rounds")) result["rounds"] = *value;
    if (const auto* value = field(upstream, "raw_size")) result["raw_size"] = *value;
    result["decoded_row_count"] = static_cast<std::uint64_t>(raw_rows.size());
    const auto* response = field(upstream, "response");
    const auto* sets = response ? field(*response, "ResultSets") : nullptr;
    if (sets && sets->is_array() && sets->size()) {
        const auto& table = sets->as_array().front();
        if (const auto* value = field(table, "RowNum"))
            result["declared_row_count"] = *value;
        if (const auto* value = field(table, "ColNum"))
            result["declared_column_count"] = *value;
    }
    return result;
}

Json range_summary(const Json& history) {
    Json result = Json::object();
    result["first_date"] = history.size()
        ? history.as_array().front().at("date") : Json(nullptr);
    result["last_date"] = history.size()
        ? history.as_array().back().at("date") : Json(nullptr);
    result["latest_ratio"] = history.size()
        ? history.as_array().back().at("ratio") : Json(nullptr);
    std::vector<double> values;
    for (const auto& record : history.as_array())
        if (record.at("ratio").is_number())
            values.push_back(record.at("ratio").as_number());
    if (values.empty()) {
        result["minimum_ratio"] = Json(nullptr);
        result["maximum_ratio"] = Json(nullptr);
        result["average_ratio"] = Json(nullptr);
    } else {
        const auto bounds = std::minmax_element(values.begin(), values.end());
        double sum = 0;
        for (const auto value : values) sum += value;
        result["minimum_ratio"] = *bounds.first;
        result["maximum_ratio"] = *bounds.second;
        result["average_ratio"] = sum / static_cast<double>(values.size());
    }
    return result;
}

Json parameters_document(const QueryPlan& plan) {
    Json result = Json::object();
    result["index_type"] = plan.index_type->name;
    result["index_type_code"] = plan.index_type->code;
    result["benchmark"] = plan.benchmark->code;
    result["benchmark_name"] = plan.benchmark->name;
    result["method"] = plan.method->name;
    result["method_code"] = plan.method->code;
    result["start_date"] = display_date(plan.options.start_date);
    result["end_date"] = display_date(plan.options.end_date);
    return result;
}

Json methodology_document() {
    Json result = Json::object();
    result["ratio_definition"] =
        "Target index valuation measure divided by benchmark index valuation measure.";
    result["quantile_definition"] =
        "Historical percentile of the current relative valuation ratio in the requested interval.";
    result["position_bands"] =
        "0-20 very-low; (20,40] low; (40,60) middle; [60,80) high; [80,100] very-high";
    result["position_bands_computed_locally"] = true;
    result["investment_signal"] = false;
    return result;
}

}  // namespace tdx::detail::relative_valuation
