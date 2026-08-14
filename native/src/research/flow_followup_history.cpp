#include "flow_followup_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace tdx::flow_followup_detail {

bool placeholder_pair(const Json& record) {
    const auto inflow = number(record, "reported_net_inflow_100m_cny");
    const auto purchase = number(record, "reported_net_purchase_100m_cny");
    return inflow && purchase && std::abs(*inflow - 1040.0) < 1e-12 &&
           std::abs(*purchase) < 1e-12;
}

void mark_placeholder_runs(Json& records) {
    auto& values = records.as_array();
    std::size_t begin = 0;
    while (begin < values.size()) {
        if (!placeholder_pair(values[begin])) { ++begin; continue; }
        std::size_t end = begin + 1;
        while (end < values.size() && placeholder_pair(values[end])) ++end;
        if (end - begin >= placeholder_run_minimum) {
            for (std::size_t index = begin; index < end; ++index) {
                values[index]["net_inflow_100m_cny"] = Json(nullptr);
                values[index]["net_purchase_100m_cny"] = Json(nullptr);
                values[index]["signal_available"] = false;
                values[index]["usable_for_signal_analysis"] = false;
                values[index]["data_status"] = "upstream-placeholder";
            }
        }
        begin = end;
    }
}

Json horizon_statistics(const Json& records, const std::string& key) {
    std::vector<double> values;
    for (const auto& record : records.as_array()) {
        if (!bool_value(record, "signal_available")) continue;
        const auto* forward = field(record, "forward_returns_pct");
        if (!forward) continue;
        const auto value = number(*forward, key);
        if (value) values.push_back(*value);
    }
    Json result = Json::object();
    result["observations"] = static_cast<std::uint64_t>(values.size());
    if (values.empty()) {
        result["mean_pct"] = Json(nullptr);
        result["positive_ratio_pct"] = Json(nullptr);
        result["minimum_pct"] = Json(nullptr);
        result["maximum_pct"] = Json(nullptr);
        return result;
    }
    double sum = 0;
    std::size_t positive = 0;
    for (const auto value : values) {
        sum += value;
        if (value > 0) ++positive;
    }
    const auto [minimum, maximum] = std::minmax_element(values.begin(), values.end());
    result["mean_pct"] = sum / static_cast<double>(values.size());
    result["positive_ratio_pct"] =
        100.0 * static_cast<double>(positive) / static_cast<double>(values.size());
    result["minimum_pct"] = *minimum;
    result["maximum_pct"] = *maximum;
    return result;
}


}  // namespace tdx::flow_followup_detail

namespace tdx {

using namespace flow_followup_detail;

Json normalize_flow_followup_rows(const std::string& raw_view, const Json& rows) {
    if (!rows.is_array()) throw Error("flow follow-up rows must be an array");
    const auto view = canonical_view(raw_view);
    if (!history_view(view))
        throw Error("flow history normalization requires margin or northbound view");
    Json result = Json::array();
    std::set<std::string> dates;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        const auto raw_date = compact_date(text(row, "date"), "upstream date");
        if (!dates.insert(raw_date).second)
            throw Error("flow follow-up response contains duplicate date: " + raw_date);
        Json value = Json::object();
        value["date"] = display_date(raw_date);
        value["csi300_close"] = number_json(number(row, "close"));
        value["signal_available"] = true;
        value["usable_for_signal_analysis"] = true;
        value["data_status"] = "reported";
        if (view == "margin") {
            value["financing_balance_100m_cny"] = number_json(number(row, "valuerzyq"));
            value["securities_lending_balance_100m_cny"] =
                number_json(number(row, "valuerqye"));
            value["active_float_market_cap_10b_cny"] = number_json(number(row, "close1"));
            value["financing_rate_pct"] = number_json(number(row, "rankvaluerzl"));
            value["securities_lending_rate_pct"] = number_json(number(row, "rankvaluerql"));
        } else {
            const auto inflow = number(row, "valuejlr");
            const auto purchase = number(row, "valuejmr");
            value["reported_net_inflow_100m_cny"] = number_json(inflow);
            value["reported_net_purchase_100m_cny"] = number_json(purchase);
            value["net_inflow_100m_cny"] = number_json(inflow);
            value["net_purchase_100m_cny"] = number_json(purchase);
        }
        Json forward = Json::object();
        forward["days_1"] = number_json(number(row, "zdfavg1"));
        forward["days_3"] = number_json(number(row, "zdfavg3"));
        forward["days_5"] = number_json(number(row, "zdfavg5"));
        if (view == "margin")
            forward["days_10"] = number_json(number(row, "zdfavg10"));
        value["forward_returns_pct"] = std::move(forward);
        result.push_back(std::move(value));
    }
    std::sort(result.as_array().begin(), result.as_array().end(),
              [](const Json& left, const Json& right) {
                  return left.at("date").as_string() < right.at("date").as_string();
              });
    if (view == "northbound") mark_placeholder_runs(result);
    return result;
}


}  // namespace tdx

