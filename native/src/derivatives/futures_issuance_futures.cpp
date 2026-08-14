#include "futures_issuance_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

namespace tdx::futures_issuance_detail {

Json normalize_monthly_futures(const Json& rows) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["year_month"] = text_value(row, "YF");
        item["product"] = text_value(row, "PZ");
        item["monthly_volume"] = number_value(row, "CJL");
        item["monthly_volume_yoy_pct"] = number_value(row, "CJLTB");
        item["monthly_turnover_yuan"] = number_value(row, "CJE");
        item["monthly_turnover_yoy_pct"] = number_value(row, "CJETB");
        item["year_to_date_volume"] = number_value(row, "LJCJL");
        item["year_to_date_volume_yoy_pct"] = number_value(row, "LJCJLTB");
        item["year_to_date_turnover_yuan"] = number_value(row, "LJCJE");
        item["year_to_date_turnover_yoy_pct"] = number_value(row, "LJCJETB");
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_index_futures(const Json& rows) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["name"] = text_value(row, "GZQH");
        item["contract_code"] = text_value(row, "$ZQDM");
        item["market_id"] = text_value(row, "$SC");
        item["contract_key"] = text_value(row, "$SC") + text_value(row, "$ZQDM");
        item["net_position"] = number_value(row, "ZLHY4");
        item["date"] = text_value(row, "DATE");
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_related_stocks(const Json& rows,
                              const std::map<std::pair<int, std::string>, Security>& securities) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["security"] = security_document(text_value(row, "$SC"),
                                               text_value(row, "$ZQDM"), securities);
        item["return_5d_pct"] = number_value(row, "zf5");
        item["return_10d_pct"] = number_value(row, "zf10");
        item["date"] = text_value(row, "date");
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_position_history(const Json& rows) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["date"] = text_value(row, "date");
        item["net_position"] = number_value(row, "jcc");
        result.push_back(std::move(item));
    }
    return result;
}


}  // namespace tdx::futures_issuance_detail

namespace tdx {

using namespace futures_issuance_detail;

Json normalize_futures_contract_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("futures contract rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["name"] = text_value(row, "SPQH");
        item["contract_code"] = text_value(row, "$ZQDM");
        item["market_id"] = text_value(row, "$SC");
        item["contract_key"] = text_value(row, "$SC") + text_value(row, "$ZQDM");
        item["date"] = text_value(row, "DATE");
        item["return_5d_pct"] = number_value(row, "zdf5");
        item["return_10d_pct"] = number_value(row, "zdf10");
        item["price_change"] = number_value(row, "zd1");
        item["open_interest"] = number_value(row, "ccl");
        item["daily_position_change"] = number_value(row, "rzl");
        item["volume"] = number_value(row, "cjl");
        item["spot_price"] = number_value(row, "xhjg");
        item["basis_state"] = text_value(row, "sts");
        Json recovered = Json::object();
        recovered["zl"] = number_value(row, "zl");
        recovered["zj"] = number_value(row, "zj");
        item["unlabeled_recovered_metrics"] = std::move(recovered);
        result.push_back(std::move(item));
    }
    return result;
}


}  // namespace tdx

