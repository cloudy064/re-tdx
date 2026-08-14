#include "fund_analytics_internal.hpp"

#include <optional>
#include <utility>

namespace tdx::fund_analytics_detail {

std::optional<Json> normalize_reported_holdings_row(const Json& row) {
    if (text(row, "fund_code").empty()) return std::nullopt;
    Json item = common_fund_record(row);
    item["report_date"] = optional_date(text(row, "reportDate"));
    const auto turnover = number(row, "turnoverRate");
    item["turnover_ratio"] = number_json(turnover);
    item["turnover_pct"] = turnover
        ? Json(*turnover * 100.0) : Json(nullptr);
    item["reported_security_count"] = number_json(number(row, "holdPosNum"));
    item["stock_concentration_pct"] = number_json(
        number(row, "stockConcentration"));
    item["reported_industry_count"] = number_json(
        number(row, "holdIndustryNum"));
    item["industry_concentration_pct"] = number_json(
        number(row, "IndustryConcentration"));
    item["stock_market_value_yuan"] = number_json(number(row, "marketValue"));
    return item;
}

std::optional<Json> normalize_reported_industry_row(const Json& row) {
    if (text(row, "InduatryName").empty()) return std::nullopt;
    Json item = Json::object();
    item["industry"] = text(row, "InduatryName");
    item["holding_market_value_yuan"] = number_json(
        number(row, "InduatryValue"));
    item["share_of_stock_investment_pct"] = number_json(
        number(row, "IndustryRatio"));
    return item;
}

std::optional<Json> normalize_reported_security_row(const Json& row) {
    if (text(row, "StockCode").empty()) return std::nullopt;
    Json item = Json::object();
    item["security_code"] = text(row, "StockCode");
    item["security_name"] = text(row, "StockName");
    item["current_price"] = number_json(number(row, "NowPrice"));
    item["current_change_pct"] = number_json(number(row, "ChangeRate"));
    item["holding_shares"] = number_json(number(row, "SharesNumber"));
    item["holding_market_value_yuan"] = number_json(number(row, "SharesValue"));
    item["share_of_fund_nav_pct"] = number_json(number(row, "SharesRatio"));
    return item;
}

std::optional<Json> normalize_holdings_stability_row(const Json& row) {
    if (text(row, "fund_code").empty()) return std::nullopt;
    Json item = common_fund_record(row);
    item["average_turnover_ratio"] = number_json(
        number(row, "turnoverRateAvg"));
    item["average_stock_concentration_pct"] = number_json(
        number(row, "stockConcentrationAvg"));
    item["average_industry_concentration_pct"] = number_json(
        number(row, "IndustryConcentrationAvg"));
    Json stability = Json::object();
    stability["turnover"] = number_json(number(row, "turnoverRateStability"));
    stability["stock_concentration"] = number_json(
        number(row, "stockConcentStability"));
    stability["industry_concentration"] = number_json(
        number(row, "IndustryConcentStability"));
    stability["stock_position"] = number_json(
        number(row, "stockPositionStability"));
    item["stability"] = std::move(stability);
    return item;
}

std::optional<Json> normalize_holding_industry_row(const Json& row) {
    if (text(row, "InduatryName").empty()) return std::nullopt;
    Json item = Json::object();
    item["industry"] = text(row, "InduatryName");
    item["average_market_value"] = number_json(
        number(row, "InduatryAvgValue"));
    item["average_market_value_ratio_pct"] = number_json(
        number(row, "IndustryAvgRatio"));
    return item;
}

std::optional<Json> normalize_holding_history_row(const Json& row) {
    if (text(row, "reportDate").empty()) return std::nullopt;
    Json item = Json::object();
    item["report_date"] = display_date(text(row, "reportDate"));
    item["turnover_ratio"] = number_json(number(row, "turnoverRate"));
    item["stock_concentration_pct"] = number_json(
        number(row, "stockConcentration"));
    item["industry_concentration_pct"] = number_json(
        number(row, "IndustryConcentration"));
    item["stock_market_value"] = number_json(number(row, "SharesValue"));
    item["stock_position_pct"] = number_json(number(row, "SharesRatio"));
    return item;
}

}  // namespace tdx::fund_analytics_detail
