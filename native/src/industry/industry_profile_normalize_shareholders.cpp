#include "industry_profile_internal.hpp"

namespace tdx {

Json normalize_industry_shareholder_rows(
    const Json& rows, const std::map<std::string, Block>& blocks) {
    if (!rows.is_array()) throw Error("industry shareholder rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = detail::industry_profile::json_text(row, "$ZQDM");
        const auto found = blocks.find(code);
        if (!detail::industry_profile::valid_research_industry_code(code) || found == blocks.end()) continue;
        Json item = Json::object();
        item["industry"] = detail::industry_profile::make_basic_industry(found->second);
        item["detail_id"] = detail::industry_profile::json_text(row, "$SC") + code;
        item["start_date"] = detail::industry_profile::json_copy(row, "sdate");
        item["end_date"] = detail::industry_profile::json_copy(row, "edate");
        auto metric = [&](const char* current, const char* initial,
                          const char* share = nullptr) {
            Json value = Json::object();
            const auto a = detail::industry_profile::json_number_value(row, current);
            const auto b = detail::industry_profile::json_number_value(row, initial);
            value["current"] = detail::industry_profile::json_copy(row, current);
            value["initial"] = detail::industry_profile::json_copy(row, initial);
            value["change"] = detail::industry_profile::numeric_difference(a, b);
            value["change_pct"] = detail::industry_profile::numeric_percentage_change(a, b);
            if (share) value["share_pct"] = detail::industry_profile::json_copy(row, share);
            return value;
        };
        Json metrics = Json::object();
        metrics["per_capita_float_shares"] = metric("zyltg1", "zyltg2");
        metrics["top10_float_holding"] = metric("sdlt1", "sdlt2", "sdlt3");
        metrics["top10_holding"] = metric("sdgd1", "sdgd2", "sdgd3");
        metrics["institution_holding"] = metric("jgcc1", "jgcc2", "jgcc3");
        item["metrics"] = std::move(metrics);
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_industry_shareholder_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array())
        throw Error("industry shareholder security rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = detail::industry_profile::json_text(row, "$ZQDM");
        if (!detail::industry_profile::valid_security_code(code)) continue;
        const int market_id = detail::industry_profile::canonical_market_id(detail::industry_profile::json_text(row, "$SC"));
        Json item = Json::object();
        item["security"] = detail::industry_profile::make_security_document(market_id, code, securities);
        Json shareholders = Json::object();
        shareholders["start_date"] = detail::industry_profile::json_copy(row, "date1");
        shareholders["end_date"] = detail::industry_profile::json_copy(row, "date");
        shareholders["days"] = detail::industry_profile::json_copy(row, "date3");
        shareholders["households"] = detail::industry_profile::json_copy(row, "gdrs1");
        shareholders["households_change"] = detail::industry_profile::json_copy(row, "gdrs2");
        shareholders["households_change_pct"] = detail::industry_profile::json_copy(row, "gdrs3");
        const auto total_change = detail::industry_profile::json_number_value(row, "gdrs3");
        const auto days = detail::industry_profile::json_number_value(row, "date3");
        shareholders["daily_change_pct"] = total_change && days && *days != 0.0
            ? Json(*total_change / *days) : Json(nullptr);
        item["shareholders"] = std::move(shareholders);
        Json per_capita = Json::object();
        per_capita["float_shares"] = detail::industry_profile::json_copy(row, "gdrs4");
        item["per_capita"] = std::move(per_capita);
        Json top10_float = Json::object();
        top10_float["report_date"] = detail::industry_profile::json_copy(row, "jzr1");
        top10_float["shares"] = detail::industry_profile::json_copy(row, "sdlt1");
        top10_float["share_pct"] = detail::industry_profile::json_copy(row, "sdlt2");
        item["top10_float"] = std::move(top10_float);
        Json top10 = Json::object();
        top10["report_date"] = detail::industry_profile::json_copy(row, "jzr2");
        top10["shares"] = detail::industry_profile::json_copy(row, "sdgd1");
        top10["share_pct"] = detail::industry_profile::json_copy(row, "sdgd2");
        item["top10"] = std::move(top10);
        Json institution = Json::object();
        institution["report_date"] = detail::industry_profile::json_copy(row, "bgq");
        institution["shares"] = detail::industry_profile::json_copy(row, "jgcc1");
        institution["float_share_pct"] = detail::industry_profile::json_copy(row, "jgcc2");
        item["institution"] = std::move(institution);
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx

