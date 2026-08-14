#include "fund_analytics_internal.hpp"

#include <cmath>
#include <optional>

namespace tdx::fund_analytics_detail {

std::optional<Json> normalize_position_estimate_row(const Json& row) {
    if (text(row, "fund_code").empty()) return std::nullopt;
    Json item = common_fund_record(row);
    item["report_date"] = optional_date(text(row, "reportDate"));
    item["reported_position_pct"] = number_json(number(row, "reportHoldPos"));
    item["estimate_date"] = optional_date(text(row, "estimateDate"));
    item["estimated_position_pct"] = number_json(number(row, "estimateHoldPos"));
    const auto reported = number(row, "reportHoldPos");
    const auto estimated = number(row, "estimateHoldPos");
    item["estimated_change_from_report_pct_points"] = reported && estimated
        ? Json(std::round((*estimated - *reported) * 1000000.0) / 1000000.0)
        : Json(nullptr);
    return item;
}

std::optional<Json> normalize_market_position_row(const Json& row) {
    if (text(row, "estimateDate").empty()) return std::nullopt;
    Json item = Json::object();
    item["date"] = display_date(text(row, "estimateDate"));
    item["stock_fund_position_pct"] = number_json(number(row, "StockEstPos"));
    item["equity_mixed_fund_position_pct"] = number_json(
        number(row, "MixedEstPos"));
    item["combined_market_position_pct"] = number_json(
        number(row, "MarketEstPos"));
    return item;
}

}  // namespace tdx::fund_analytics_detail
