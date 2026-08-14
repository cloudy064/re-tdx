#include "industry_profile_internal.hpp"

#include <algorithm>

namespace tdx {

Json normalize_industry_holdings_rows(
    const Json& rows, const std::string& period_key,
    const std::string& period_label,
    const std::map<std::string, Block>& blocks) {
    if (!rows.is_array()) throw Error("industry holding rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = detail::industry_profile::json_text(row, "$ZQDM");
        const auto found = blocks.find(code);
        if (!detail::industry_profile::valid_research_industry_code(code) || found == blocks.end()) continue;
        const auto market_value = detail::industry_profile::json_number_value(row, "sz");
        const auto previous_market_value = detail::industry_profile::json_number_value(row, "sz_sq");
        const auto shares = detail::industry_profile::json_number_value(row, "gs");
        const auto shares_change = detail::industry_profile::json_number_value(row, "gs_bd");
        const auto float_shares = detail::industry_profile::json_number_value(row, "bgqltgb");
        const auto total_shares = detail::industry_profile::json_number_value(row, "bgqzgb");
        Json item = Json::object();
        item["industry"] = detail::industry_profile::make_basic_industry(found->second);
        item["period_key"] = period_key;
        item["period_label"] = period_label;
        item["report_date"] = detail::industry_profile::json_copy(row, "bgq");
        Json metrics = Json::object();
        metrics["market_value"] = detail::industry_profile::json_copy(row, "sz");
        metrics["previous_market_value"] = detail::industry_profile::json_copy(row, "sz_sq");
        metrics["market_value_change"] = detail::industry_profile::numeric_difference(market_value, previous_market_value);
        metrics["market_value_change_pct"] =
            detail::industry_profile::numeric_percentage_change(market_value, previous_market_value);
        metrics["institution_count"] = detail::industry_profile::json_copy(row, "jgsl");
        metrics["institution_count_change"] = detail::industry_profile::json_copy(row, "jgsl_bd");
        metrics["shares_held"] = detail::industry_profile::json_copy(row, "gs");
        metrics["previous_shares_held"] = shares && shares_change
            ? Json(*shares - *shares_change) : Json(nullptr);
        metrics["shares_change"] = detail::industry_profile::json_copy(row, "gs_bd");
        metrics["shares_change_pct"] = shares && shares_change && *shares != *shares_change
            ? Json(*shares_change / (*shares - *shares_change) * 100.0) : Json(nullptr);
        metrics["float_shares"] = detail::industry_profile::json_copy(row, "bgqltgb");
        metrics["total_shares"] = detail::industry_profile::json_copy(row, "bgqzgb");
        metrics["float_share_pct"] = detail::industry_profile::numeric_percentage_of(shares, float_shares);
        metrics["float_share_change_pct"] = detail::industry_profile::numeric_percentage_of(shares_change, float_shares);
        metrics["total_share_pct"] = detail::industry_profile::numeric_percentage_of(shares, total_shares);
        metrics["total_share_change_pct"] = detail::industry_profile::numeric_percentage_of(shares_change, total_shares);
        item["metrics"] = std::move(metrics);
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_industry_holdings_history_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("industry holding history rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = detail::industry_profile::json_text(row, "$ZQDM");
        if (!detail::industry_profile::valid_research_industry_code(code)) continue;
        const auto shares = detail::industry_profile::json_number_value(row, "gs");
        const auto shares_change = detail::industry_profile::json_number_value(row, "gs_bd");
        const auto float_shares = detail::industry_profile::json_number_value(row, "bgqltgb");
        const auto total_shares = detail::industry_profile::json_number_value(row, "bgqzgb");
        Json item = Json::object();
        item["industry_code"] = code;
        item["report_date"] = detail::industry_profile::json_copy(row, "bgq");
        Json metrics = Json::object();
        metrics["market_value"] = detail::industry_profile::json_copy(row, "sz");
        metrics["market_value_change_pct"] = detail::industry_profile::json_copy(row, "sz_hb");
        metrics["institution_count"] = detail::industry_profile::json_copy(row, "jgsl");
        metrics["institution_count_change"] = detail::industry_profile::json_copy(row, "jgsl_bd");
        metrics["shares_held"] = detail::industry_profile::json_copy(row, "gs");
        metrics["shares_change"] = detail::industry_profile::json_copy(row, "gs_bd");
        metrics["float_shares"] = detail::industry_profile::json_copy(row, "bgqltgb");
        metrics["total_shares"] = detail::industry_profile::json_copy(row, "bgqzgb");
        metrics["float_share_pct"] = detail::industry_profile::numeric_percentage_of(shares, float_shares);
        metrics["float_share_change_pct"] = detail::industry_profile::numeric_percentage_of(shares_change, float_shares);
        metrics["total_share_pct"] = detail::industry_profile::numeric_percentage_of(shares, total_shares);
        metrics["total_share_change_pct"] = detail::industry_profile::numeric_percentage_of(shares_change, total_shares);
        item["metrics"] = std::move(metrics);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return detail::industry_profile::json_text(left, "report_date") > detail::industry_profile::json_text(right, "report_date");
        });
    return result;
}

}  // namespace tdx

