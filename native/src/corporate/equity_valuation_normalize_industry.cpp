#include "equity_valuation_internal.hpp"

namespace tdx::detail::equity_valuation {

Json normalize_pe_industry_row(
    const Json& row, const BlockData& blocks, std::string& identity) {
    auto industry = industry_identity(text(row, "code_hy"), text(row, "market_hy"),
                                      text(row, "name_hy"), blocks);
    identity = industry.at("industry_id").as_string();
    Json item = Json::object();
    item["industry"] = std::move(industry);
    item["pe"] = number_json(number(row, "PE_TTM_industry"));
    return item;
}

Json normalize_pb_industry_row(
    const Json& row, const BlockData& blocks, std::string& identity) {
    auto industry = industry_identity(text(row, "code_hy"), text(row, "market_hy"),
                                      text(row, "name_hy"), blocks);
    identity = industry.at("industry_id").as_string();
    Json item = Json::object();
    item["industry"] = std::move(industry);
    item["pb"] = number_json(number(row, "PB_hy"));
    item["roe_pct"] = number_json(number(row, "ROE_hy"));
    item["member_median_roe_pct"] = number_json(number(row, "mean_ROE_industry"));
    return item;
}

}  // namespace tdx::detail::equity_valuation
