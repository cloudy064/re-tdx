#include "futures_issuance_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

namespace tdx::futures_issuance_detail {

Json normalize_ipo_annual(const Json& rows) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["year"] = text_value(row, "$ZQDM");
        item["listed_count"] = number_value(row, "ssjs");
        item["raised_10k_yuan"] = number_value(row, "zmz");
        item["average_listing_gain_pct"] = number_value(row, "jsn");
        item["largest_fundraiser"] = text_value(row, "ggmc");
        item["largest_raised_10k_yuan"] = number_value(row, "mjzj");
        item["market_return_pct"] = number_value(row, "szzf");
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_ipo_industries(const Json& rows) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["industry_code"] = text_value(row, "$ZQDM1");
        item["industry_name"] = text_value(row, "hy");
        item["year"] = text_value(row, "nf");
        item["listed_count"] = number_value(row, "ssjs");
        item["raised_10k_yuan"] = number_value(row, "zmz");
        item["average_listing_gain_pct"] = number_value(row, "jsn");
        item["largest_fundraiser"] = text_value(row, "ggmc");
        item["largest_raised_10k_yuan"] = number_value(row, "mjzj");
        item["industry_key"] = text_value(row, "$ZQDM");
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_ipo_monthly(const Json& rows) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["month"] = text_value(row, "date");
        item["raised_100m_yuan"] = number_value(row, "mjzj");
        item["listed_count"] = number_value(row, "ssjs");
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_listed_issuers(const Json& rows,
                              const std::map<std::pair<int, std::string>, Security>& securities) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["issuer_name"] = text_value(row, "FXRJC");
        item["issuer_id"] = text_value(row, "ID");
        item["detail_key"] = text_value(row, "$ZQDM");
        item["security"] = security_document(text_value(row, "$SC1"),
                                               text_value(row, "$ZQDM1"), securities);
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_unlisted_issuers(const Json& rows) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["issuer_name"] = text_value(row, "FXRJC");
        item["issuer_id"] = text_value(row, "$ZQDM");
        result.push_back(std::move(item));
    }
    return result;
}


}  // namespace tdx::futures_issuance_detail

namespace tdx {

using namespace futures_issuance_detail;

Json normalize_ipo_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("IPO security rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["security"] = security_document(text_value(row, "$SC"),
                                               text_value(row, "$ZQDM"), securities);
        item["listing_date"] = text_value(row, "ssrq");
        item["raised_10k_yuan"] = number_value(row, "mjzj");
        item["issue_price"] = number_value(row, "ssfqj");
        result.push_back(std::move(item));
    }
    return result;
}


}  // namespace tdx

