#include "leverage_internal.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tdx {
using namespace leverage_detail;

Json normalize_stock_connect_flow_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("stock-connect flow rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json value = Json::object();
        value["date"] = value_text(row, "rq");
        value["turnover_yuan"] = number_json(value_number(row, "bxcje"), 1e8);
        value["net_inflow_yuan"] = number_json(value_number(row, "drzjlr"), 1e8);
        value["daily_balance_yuan"] = number_json(value_number(row, "drye"), 1e8);
        value["buy_turnover_yuan"] = number_json(value_number(row, "mrcje"), 1e8);
        value["sell_turnover_yuan"] = number_json(value_number(row, "mccje"), 1e8);
        value["net_buy_turnover_yuan"] = value_number(row, "mrcje") && value_number(row, "mccje")
            ? Json((*value_number(row, "mrcje") - *value_number(row, "mccje")) * 1e8)
            : Json(nullptr);
        value["average_buy_per_trade_yuan"] = number_json(value_number(row, "mremrbs"), 1e8);
        value["average_sell_per_trade_yuan"] = number_json(value_number(row, "mcemcbs"), 1e8);
        value["reference_index"] = number_json(value_number(row, "szzs"));
        value["reference_index_change_pct"] = number_json(value_number(row, "zdf"));
        result.push_back(std::move(value));
    }
    sort_date(result, true);
    return result;
}

Json normalize_stock_connect_holding_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("stock-connect holding rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const bool quarterly = !value_text(row, "$ZQDM1").empty();
        const auto code = value_text(row, quarterly ? "$ZQDM1" : "$ZQDM");
        int id = -1;
        try { id = market_id(value_text(row, quarterly ? "$SC1" : "$SC")); }
        catch (...) { continue; }
        if (code.empty()) continue;
        Json value = Json::object();
        value["security"] = security_json(id, code, securities);
        value["date"] = value_text(row, quarterly ? "N001" : "zxrq");
        value["holding_shares"] = number_json(value_number(row, quarterly ? "N002" : "cgsl"));
        value["holding_ratio_pct"] = number_json(value_number(row, quarterly ? "N003" : "cgzb"));
        value["holding_market_value_yuan"] = quarterly
            ? number_json(value_number(row, "N004")) : Json(nullptr);
        value["previous_holding_shares"] = number_json(value_number(row, "N005"));
        value["previous_holding_ratio_pct"] = number_json(value_number(row, "N006"));
        const auto shares = value_number(row, quarterly ? "N002" : "cgsl");
        const auto previous = value_number(row, "N005");
        value["holding_share_change"] = shares && previous
            ? Json(*shares - *previous) : Json(nullptr);
        value["net_buy_amount_yuan"] = quarterly
            ? Json(nullptr) : number_json(value_number(row, "jmr"), 1e4);
        value["daily_market_value_change_yuan"] = number_json(value_number(row, "dr"));
        value["five_day_market_value_change_yuan"] = number_json(value_number(row, "j5r"));
        value["first_inclusion_date"] = value_text(row, "date");
        value["channel"] = quarterly ? (id == 1 ? "沪股通" : "深股通") : "港股通";
        value["snapshot_freshness"] = value_text(row, "zxrq") < "20250101" && !quarterly
            ? "historical-snapshot" : "current";
        result.push_back(std::move(value));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return value_number(left, "holding_market_value_yuan").value_or(0.0) >
                   value_number(right, "holding_market_value_yuan").value_or(0.0);
        });
    return result;
}

Json normalize_stock_connect_history_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("stock-connect history rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json value = Json::object();
        value["date"] = value_text(row, "date");
        value["holding_shares"] = number_json(value_number(row, "cgsl"));
        value["holding_share_change"] = number_json(value_number(row, "cgbd"));
        value["holding_market_value_yuan"] = number_json(value_number(row, "cgsz"));
        value["holding_ratio_pct"] = number_json(value_number(row, "cgzb"));
        value["market_value_change_yuan"] = number_json(value_number(row, "jme"));
        value["market_value_change_pct"] = number_json(value_number(row, "jmb"));
        result.push_back(std::move(value));
    }
    sort_date(result, true);
    return result;
}

Json normalize_stock_connect_chart_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("stock-connect chart rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json value = Json::object();
        value["date"] = value_text(row, "date");
        // hsgtcg2 is the chart projection: jme is 万元, unlike hsgtcg1 yuan.
        value["market_value_change_yuan"] = number_json(value_number(row, "jme"), 1e4);
        value["holding_ratio_pct"] = number_json(value_number(row, "cgzb"));
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    sort_date(result, true);
    return result;
}

Json normalize_stock_connect_southbound_member_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("southbound industry member rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = value_text(row, "$ZQDM");
        int id = -1;
        try { id = market_id(value_text(row, "$SC")); }
        catch (...) { continue; }
        if (!digits(code, id == 31 || id == 48 ? 5 : 6)) continue;
        const auto latest = value_number(row, "price1");
        const auto five_day = value_number(row, "price2");
        const auto month = value_number(row, "price3");
        Json value = Json::object();
        value["security"] = security_json(id, code, securities);
        value["daily_net_inflow_yuan"] = number_json(value_number(row, "drjlr"));
        value["five_day_net_inflow_yuan"] = number_json(value_number(row, "wrjlr"));
        value["one_month_net_inflow_yuan"] = number_json(value_number(row, "yyjlr"));
        value["latest_close"] = number_json(latest);
        value["five_day_base_close"] = number_json(five_day);
        value["one_month_base_close"] = number_json(month);
        value["five_day_change_pct"] = latest && five_day && *five_day != 0.0
            ? Json((*latest / *five_day - 1.0) * 100.0) : Json(nullptr);
        value["one_month_change_pct"] = latest && month && *month != 0.0
            ? Json((*latest / *month - 1.0) * 100.0) : Json(nullptr);
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return value_number(left, "daily_net_inflow_yuan").value_or(-1e300) >
                   value_number(right, "daily_net_inflow_yuan").value_or(-1e300);
        });
    return result;
}

Json normalize_stock_connect_southbound_trend_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("southbound industry trend rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json value = Json::object();
        value["date"] = value_text(row, "date");
        // The chart legend explicitly declares drjlr in 亿元.
        value["daily_net_inflow_yuan"] = number_json(value_number(row, "drjlr"), 1e8);
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    sort_date(result, true);
    return result;
}

Json normalize_stock_connect_activity_rows(
    const Json& rows, const std::string& category,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("stock-connect activity rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = value_text(row, "$ZQDM");
        int id = -1;
        try { id = market_id(value_text(row, "$SC"), true); }
        catch (...) { continue; }
        if (!digits(code, 6)) continue;
        const auto date = value_text(row, "tjrq");
        const auto increase = value_number(row, "sjzc");
        const auto decrease = value_number(row, "sjjc");
        const auto cumulative_increase = value_number(row, "ljzc");
        const auto cumulative_decrease = value_number(row, "ljjc");
        const auto five_day_change = value_number(row, "wrzc");
        Json value = Json::object();
        value["security"] = security_json(id, code, securities);
        value["category"] = category;
        value["date"] = date;
        value["source_age_days"] = date_age_days(date) < 0
            ? Json(nullptr) : Json(date_age_days(date));
        value["snapshot_freshness"] = snapshot_freshness(date);
        value["net_buy_amount"] = number_json(
            value_number(row, value_number(row, "wrjmr") ? "wrjmr" : "jmr"));
        value["holding_shares"] = number_json(value_number(row, "cgsl"));
        value["previous_holding_shares"] = number_json(value_number(row, "srsl"));
        value["holding_share_change"] = increase ? number_json(increase)
            : decrease ? number_json(decrease)
            : cumulative_increase ? number_json(cumulative_increase)
            : cumulative_decrease ? number_json(cumulative_decrease)
            : number_json(five_day_change);
        value["consecutive_days"] = number_json(
            value_number(row, value_number(row, "lzts") ? "lzts" : "ljts"));
        value["holding_change_pct"] = number_json(
            value_number(row, value_number(row, "zcfd") ? "zcfd" : "jcfd"));
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    return result;
}

Json normalize_stock_connect_industry_rows(const Json& rows,
                                           const std::string& category) {
    if (!rows.is_array()) throw Error("stock-connect industry rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto date = value_text(row, "date");
        const auto latest = value_number(row, "price1");
        const auto five_day_base = value_number(row, "price2");
        const auto month_base = value_number(row, "price3");
        Json value = Json::object();
        value["category"] = category;
        value["classification_code"] = value_text(row, "$ZQDM");
        value["classification_market"] = value_text(row, "$SC");
        value["date"] = date;
        value["source_age_days"] = date_age_days(date) < 0
            ? Json(nullptr) : Json(date_age_days(date));
        value["snapshot_freshness"] = snapshot_freshness(date);
        value["daily_net_inflow"] = number_json(value_number(row, "drjlr"));
        value["five_day_net_inflow"] = number_json(value_number(row, "wrjlr"));
        value["one_month_net_inflow"] = number_json(value_number(row, "yyjlr"));
        value["latest_close"] = number_json(latest);
        value["five_day_base_close"] = number_json(five_day_base);
        value["one_month_base_close"] = number_json(month_base);
        value["five_day_change_pct"] = latest && five_day_base && *five_day_base != 0
            ? Json((*latest / *five_day_base - 1.0) * 100.0) : Json(nullptr);
        value["one_month_change_pct"] = latest && month_base && *month_base != 0
            ? Json((*latest / *month_base - 1.0) * 100.0) : Json(nullptr);
        value["float_market_value"] = number_json(
            value_number(row, value_number(row, "LTSZ") ? "LTSZ" : "$J_LTSZ"));
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    return result;
}

Json normalize_stock_connect_active_rows(
    const Json& rows, const std::string& channel,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("stock-connect active rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = value_text(row, "$ZQDM");
        int id = -1;
        try { id = market_id(value_text(row, "$SC")); }
        catch (...) { continue; }
        if (!digits(code, id == 31 || id == 48 ? 5 : 6)) continue;
        const auto total = value_number(row, "cje");
        const auto buy = value_number(row, "bje");
        const auto sell = value_number(row, "sje");
        const auto connect_turnover = value_number(row, "cjje");
        const auto net = buy && sell ? std::optional<double>(*buy - *sell)
                                     : value_number(row, "jme");
        Json value = Json::object();
        value["security"] = security_json(id, code, securities);
        value["channel"] = channel;
        value["date"] = value_text(row, "date");
        value["close"] = number_json(value_number(row, "price"));
        value["change_pct"] = number_json(value_number(row, "zf"));
        value["total_turnover_10k_yuan"] = number_json(total);
        value["net_buy_10k_yuan"] = number_json(net);
        value["buy_10k_yuan"] = number_json(buy);
        value["sell_10k_yuan"] = number_json(sell);
        value["connect_turnover_yuan"] = number_json(connect_turnover);
        value["net_buy_total_turnover_pct"] = net && total && *total != 0
            ? Json(std::abs(*net) / *total * 100.0) : Json(nullptr);
        value["connect_total_turnover_pct"] = connect_turnover && total && *total != 0
            ? Json(*connect_turnover / 10000.0 / *total * 100.0) : Json(nullptr);
        value["raw"] = row;
        result.push_back(std::move(value));
    }
    return result;
}

} // namespace tdx
