#include "forecasts_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx {

Json normalize_hong_kong_forecast_rows(const Json& rows) {
    using namespace forecast_detail;
    if (!rows.is_array()) throw Error("Hong Kong forecast rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        const auto market_text = text_value(row, "$SC");
        if (!digits(code, 5) || !digits(market_text, 2)) continue;
        const int market = std::stoi(market_text);
        const auto type = text_value(row, "type");
        const auto [contents, reason] = split_hong_kong_text(text_value(row, "contents"));
        Json item = Json::object();
        item["security"] = security_document(market, code, {});
        item["forecast_date"] = text_value(row, "ygdate");
        item["report_type"] = text_value(row, "bgq");
        item["currency"] = text_value(row, "bz");
        item["start_date"] = text_value(row, "ksrq");
        item["end_date"] = text_value(row, "jzri");
        item["forecast_type"] = type;
        item["sentiment"] = sentiment(type);
        item["profit_lower"] = number_or_null(number_value(row, "jlr1"));
        item["profit_upper"] = number_or_null(number_value(row, "jlr2"));
        item["growth_lower_pct"] = scaled_number(row, "zj1", 100.0);
        item["growth_upper_pct"] = scaled_number(row, "zj2", 100.0);
        item["prior_profit"] = number_or_null(number_value(row, "jlr3"));
        item["annualized_profit_lower"] = number_or_null(number_value(row, "jlr4"));
        item["annualized_profit_upper"] = number_or_null(number_value(row, "jlr5"));
        item["total_capital_shares"] = number_or_null(number_value(row, "zgb"));
        item["eps"] = number_or_null(number_value(row, "eps"));
        item["industry"] = text_value(row, "hy");
        item["contents"] = contents;
        item["reason"] = reason;
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return forecast_detail::text_value(left, "forecast_date") >
                   forecast_detail::text_value(right, "forecast_date");
        });
    return result;
}

}  // namespace tdx
