#include "equity_valuation_internal.hpp"

namespace tdx::detail::equity_valuation {

Json normalize_pe_history_row(
    const Json& row, const BlockData& blocks, std::string& identity) {
    auto security = security_identity(text(row, "stock_market"),
                                      text(row, "stock_code"), {}, blocks);
    const auto date = compact_date(text(row, "Date"), "upstream date");
    identity = security.at("security_id").as_string() + ':' + date;

    Json percentiles = Json::object();
    percentiles["p25"] = number_json(number(row, "PE_25"));
    percentiles["p50"] = number_json(number(row, "PE_median"));
    percentiles["p75"] = number_json(number(row, "PE_75"));
    Json range = Json::object();
    range["minimum"] = number_json(number(row, "PE_min"));
    range["maximum"] = number_json(number(row, "PE_max"));

    Json item = Json::object();
    item["date"] = display_date(date);
    item["security"] = std::move(security);
    item["close"] = number_json(number(row, "Stockprice"));
    item["pe"] = number_json(number(row, "PE_TTM_stock"));
    item["pe_percentiles"] = std::move(percentiles);
    item["pe_range"] = std::move(range);
    return item;
}

Json normalize_pe_member_row(
    const Json& row, const BlockData& blocks, std::string& identity) {
    auto security = security_identity(text(row, "stock_market"),
                                      text(row, "stock_code"),
                                      text(row, "stock_name"), blocks);
    identity = security.at("security_id").as_string();
    Json range = Json::object();
    range["minimum"] = number_json(number(row, "PE_stock_min"));
    range["maximum"] = number_json(number(row, "PE_stock_max"));

    Json item = Json::object();
    item["date"] = display_date(text(row, "Date"));
    item["security"] = std::move(security);
    item["pe"] = number_json(number(row, "PE_TTM_stock"));
    item["pe_range"] = std::move(range);
    item["current_year"] = forecast(row, "T");
    item["next_year"] = forecast(row, "T1");
    return item;
}

Json normalize_pb_member_row(
    const Json& row, const BlockData& blocks, std::string& identity) {
    auto security = security_identity(text(row, "market"), text(row, "code"),
                                      text(row, "name"), blocks);
    identity = security.at("security_id").as_string();
    Json item = Json::object();
    item["industry_code"] = text(row, "code_hy");
    item["security"] = std::move(security);
    item["close"] = number_json(number(row, "close"));
    item["pb"] = number_json(number(row, "PB"));
    item["roe_pct"] = number_json(number(row, "ROE"));
    item["forecast_pb"] = number_json(number(row, "forcast_PB"));
    item["valuation_judgment"] = judgment(row, "gzpd");
    return item;
}

}  // namespace tdx::detail::equity_valuation
