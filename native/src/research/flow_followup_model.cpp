#include "flow_followup_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx::flow_followup_detail {

std::string bucket_label(const std::string& view, const std::string& raw_code) {
    const auto code = trim(raw_code);
    static const std::map<std::string, std::string> financing{
        {"0.0", "0 ~ 3.0"}, {"3.0", "3.0 ~ 3.8"}, {"3.8", "3.8 ~ 4.0"},
        {"4.0", "4.0 ~ 4.1"}, {"4.1", "4.1 ~ 4.2"}, {"4.2", "4.2 ~ 4.3"},
        {"4.3", "4.3 ~ 4.4"}, {"4.4", "4.4 ~ 4.5"}, {"4.5", "4.5 ~ 4.6"},
        {"4.6", "4.6 ~ 4.8"}, {"4.8", ">= 4.8"}};
    static const std::map<std::string, std::string> lending{
        {"0.0", "0 ~ 0.01"}, {"0.01", "0.01 ~ 0.03"},
        {"0.03", "0.03 ~ 0.04"}, {"0.04", "0.04 ~ 0.06"},
        {"0.06", "0.06 ~ 0.08"}, {"0.08", "0.08 ~ 0.12"},
        {"0.12", "0.12 ~ 0.28"}, {"0.28", "0.28 ~ 0.40"},
        {"0.4", "0.40 ~ 0.43"}, {"0.43", "0.43 ~ 0.45"},
        {"0.45", ">= 0.45"}};
    static const std::map<std::string, std::string> northbound{
        {"-200", "< -200"}, {"-150", "-200 ~ -150"},
        {"-100", "-150 ~ -100"}, {"-80", "-100 ~ -80"},
        {"-60", "-80 ~ -60"}, {"-40", "-60 ~ -40"},
        {"-20", "-40 ~ -20"}, {"-0", "-20 ~ 0"},
        {"+0", "0 ~ 20"}, {"20", "20 ~ 40"}, {"40", "40 ~ 60"},
        {"60", "60 ~ 80"}, {"80", "80 ~ 100"},
        {"100", "100 ~ 150"}, {"150", "150 ~ 200"},
        {"200", ">= 200"}};
    const auto& labels = view == "financing-model" ? financing
        : view == "lending-model" ? lending : northbound;
    const auto found = labels.find(code);
    return found == labels.end() ? code : found->second;
}

std::optional<double> number_between(const std::string& value,
                                     const std::string& prefix,
                                     const std::string& suffix,
                                     std::size_t begin = 0) {
    const auto left = value.find(prefix, begin);
    if (left == std::string::npos) return std::nullopt;
    const auto first = left + prefix.size();
    const auto right = value.find(suffix, first);
    if (right == std::string::npos || right <= first) return std::nullopt;
    auto raw = trim(value.substr(first, right - first));
    try {
        std::size_t used = 0;
        const double result = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(result)) return result;
    } catch (...) {}
    return std::nullopt;
}

std::string summary_date(const std::string& value) {
    if (value.size() >= 10 && value[4] == '-' && value[7] == '-' &&
        digits(value.substr(0, 4) + value.substr(5, 2) + value.substr(8, 2)))
        return value.substr(0, 10);
    if (value.size() >= 8 && digits(value.substr(0, 8)))
        return display_date(value.substr(0, 8));
    return {};
}

Json forward_summary(const std::string& value, const FlowModelDefinition& definition) {
    Json result = Json::object();
    const std::vector<int> horizons = definition.ten_day
        ? std::vector<int>{1, 3, 5, 10} : std::vector<int>{1, 3, 5};
    for (const auto days : horizons) {
        const std::string marker = days == 1 ? "次日涨跌幅为"
                                             : "未来" + std::to_string(days) + "日涨跌幅为";
        const auto marker_position = value.find(marker);
        Json item = Json::object();
        item["mean_return_pct"] = number_json(number_between(value, marker, "%"));
        item["positive_ratio_pct"] = marker_position == std::string::npos
            ? Json(nullptr)
            : number_json(number_between(value, "上涨概率为", "%", marker_position));
        result["days_" + std::to_string(days)] = std::move(item);
    }
    return result;
}


}  // namespace tdx::flow_followup_detail

namespace tdx {

using namespace flow_followup_detail;

Json normalize_flow_model_rows(const std::string& raw_view, const Json& rows) {
    if (!rows.is_array()) throw Error("flow model rows must be an array");
    const auto view = canonical_view(raw_view);
    if (history_view(view)) throw Error("selected view is not a flow model");
    const auto& definition = model_definition(view);
    Json result = Json::array();
    std::set<std::string> codes;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        const auto code = text(row, "rankvalue");
        if (code.empty()) throw Error("flow model bucket is missing rankvalue");
        if (!codes.insert(code).second)
            throw Error("flow model response contains duplicate bucket: " + code);
        Json item = Json::object();
        item["bucket_code"] = code;
        item["bucket_label"] = bucket_label(view, code);
        item["bucket_unit"] = definition.northbound
            ? "100-million-CNY" : "percentage-points";
        item["observations"] = number_json(number(
            row, definition.northbound ? "times" : "count"));
        const auto current = number(row, "dcts");
        item["is_current_bucket"] = current && *current >= 0.5;
        Json forward = Json::object();
        const std::vector<int> horizons = definition.ten_day
            ? std::vector<int>{1, 3, 5, 10} : std::vector<int>{1, 3, 5};
        for (const auto days : horizons) {
            Json metric = Json::object();
            metric["mean_return_pct"] = number_json(number(
                row, "zdfavg" + std::to_string(days)));
            metric["positive_ratio_pct"] = number_json(number(
                row, "uppercentum" + std::to_string(days)));
            forward["days_" + std::to_string(days)] = std::move(metric);
        }
        item["csi300_forward_performance"] = std::move(forward);
        result.push_back(std::move(item));
    }
    return result;
}


Json normalize_flow_model_summary(const std::string& raw_view, const Json& rows) {
    if (!rows.is_array()) throw Error("flow model summary rows must be an array");
    const auto view = canonical_view(raw_view);
    if (history_view(view)) throw Error("selected view is not a flow model");
    const auto& definition = model_definition(view);
    if (rows.size() != 1 || !rows.as_array().front().is_object())
        throw Error("flow model summary must contain exactly one row");
    const auto value = text(rows.as_array().front(), "zwts");
    if (value.empty()) throw Error("flow model summary text is empty");
    Json result = Json::object();
    const auto date = summary_date(value);
    result["date"] = date.empty() ? Json(nullptr) : Json(date);
    result["signal_kind"] = definition.signal_kind;
    result["signal_available"] = true;
    result["usable_for_signal_analysis"] = true;
    result["data_status"] = "reported";
    if (definition.northbound) {
        const auto marker = view == "northbound-inflow-model"
            ? "北上资金净流入" : "北上资金净买入";
        result["flow_100m_cny"] = number_json(number_between(value, marker, "亿"));
    } else {
        const auto balance_marker = view == "financing-model" ? "融资余额" : "融券余额";
        const auto rate_marker = view == "financing-model" ? "融资率" : "融券率";
        result["balance_100m_cny"] =
            number_json(number_between(value, balance_marker, "亿"));
        result["rate_pct"] = number_json(number_between(value, rate_marker, "%"));
    }
    result["csi300_forward_performance"] = forward_summary(value, definition);
    result["upstream_text"] = value;
    return result;
}


}  // namespace tdx
