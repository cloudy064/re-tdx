#include "forecasts_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx {

Json normalize_forecast_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace forecast_detail;
    if (!rows.is_array()) throw Error("forecast security rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        const auto market = row_mainland_market(row);
        if (!market || !digits(code, 6)) continue;
        const auto type = text_value(row, "type");
        Json item = Json::object();
        item["security"] = security_document(*market, code, securities);
        item["forecast_date"] = text_value(row, "ygdate");
        item["report_period"] = text_value(row, "bgq");
        item["forecast_type"] = type;
        item["sentiment"] = sentiment(type);
        item["profit_lower_yuan"] = number_or_null(number_value(row, "jlr1"));
        item["profit_upper_yuan"] = number_or_null(number_value(row, "jlr2"));
        item["growth_lower_pct"] = number_value(row, "zj3")
            ? scaled_number(row, "zj3", 100.0)
            : number_or_null(number_value(row, "zj1"));
        item["growth_upper_pct"] = number_value(row, "zj4")
            ? scaled_number(row, "zj4", 100.0)
            : number_or_null(number_value(row, "zj2"));
        item["prior_profit_yuan"] = number_or_null(number_value(row, "jlr3"));
        item["annualized_profit_lower_yuan"] = number_or_null(number_value(row, "x"));
        item["annualized_profit_upper_yuan"] = number_or_null(number_value(row, "y"));
        item["eps"] = number_or_null(number_value(row, "eps"));
        item["total_capital_shares"] = scaled_number(row, "zgb", 10000.0, true);
        item["contents"] = normalized_multiline(text_value(row, "contents"));
        item["reason"] = normalized_multiline(text_value(row, "result"));
        item["source_variant"] = number_value(row, "zj3")
            ? "industry-detail" : "all-market-static";
        item["raw"] = row;
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
