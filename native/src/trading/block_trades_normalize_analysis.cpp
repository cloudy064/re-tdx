#include "block_trades_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace tdx {

using block_trade_detail::broker_period;
using block_trade_detail::canonical_market_id;
using block_trade_detail::number_or_null;
using block_trade_detail::number_value;
using block_trade_detail::performance_point;
using block_trade_detail::scaled_number;
using block_trade_detail::security_document;
using block_trade_detail::six_digits;
using block_trade_detail::text_value;

Json normalize_block_trade_industry_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("block-trade industry rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["industry_id"] = text_value(row, "$ZQDM1");
        item["detail_id"] = text_value(row, "$ZQDM");
        item["name"] = text_value(row, "hy");
        item["amount_yuan"] = scaled_number(row, "cje", 10000.0);
        item["volume_shares"] = scaled_number(row, "cjl", 10000.0);
        item["premium_pct"] = number_or_null(number_value(row, "yzjl"));
        item["trade_count"] = number_or_null(number_value(row, "cjcs"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "amount_yuan").value_or(0.0) >
                   number_value(right, "amount_yuan").value_or(0.0);
        });
    return result;
}

Json normalize_block_trade_industry_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array())
        throw Error("block-trade industry security rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!six_digits(code)) continue;
        const int market_id = canonical_market_id(text_value(row, "$SC"));
        Json item = Json::object();
        item["security"] = security_document(market_id, code, securities);
        item["month"] = text_value(row, "date");
        item["amount_yuan"] = scaled_number(row, "cje", 10000.0);
        item["volume_shares"] = scaled_number(row, "cjl", 10000.0);
        item["trade_count"] = number_or_null(number_value(row, "sbcs"));
        item["trade_detail_text"] = text_value(row, "jyxq");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "amount_yuan").value_or(0.0) >
                   number_value(right, "amount_yuan").value_or(0.0);
        });
    return result;
}

Json normalize_block_trade_broker_rows(const Json& rows,
                                       const std::string& period) {
    if (!rows.is_array()) throw Error("block-trade broker rows must be an array");
    const auto& spec = broker_period(period);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["broker_id"] = text_value(row, "$ZQDM");
        item["period"] = std::string(spec.id);
        item["period_label"] = std::string(spec.label);
        item["name"] = text_value(row, "yybmc");
        item["hot_money_label"] = text_value(row, "yz");
        item["latest_date"] = text_value(row, "zjsbr");
        item["trade_count"] = number_or_null(number_value(row, "zjmmcs"));
        item["buy_count"] = number_or_null(number_value(row, "mrmmcs"));
        item["sell_count"] = number_or_null(number_value(row, "mcmmcs"));
        item["buy_amount_yuan"] = scaled_number(row, "mrcje", 10000.0);
        item["sell_amount_yuan"] = scaled_number(row, "cjemc", 10000.0);
        item["net_buy_yuan"] = scaled_number(row, "jmre", 10000.0);
        Json performance = Json::array();
        performance.push_back(performance_point(1, row, "cglyr", "pjzfyr"));
        performance.push_back(performance_point(3, row, "cglsr", "pjzfsr"));
        performance.push_back(performance_point(5, row, "cglwr", "pjzfwr"));
        performance.push_back(performance_point(10, row, "cglshr", "pjzfshr"));
        item["performance"] = std::move(performance);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "trade_count").value_or(0.0) >
                   number_value(right, "trade_count").value_or(0.0);
        });
    return result;
}

Json normalize_block_trade_broker_detail_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array())
        throw Error("block-trade broker detail rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!six_digits(code)) continue;
        const int market_id = canonical_market_id(text_value(row, "$SC"));
        const auto price = number_value(row, "cjj");
        const auto volume = number_value(row, "cjl");
        Json item = Json::object();
        item["date"] = text_value(row, "rq");
        item["security"] = security_document(market_id, code, securities);
        item["direction"] = text_value(row, "mmfx");
        item["price"] = number_or_null(price);
        item["premium_pct"] = number_or_null(number_value(row, "zyj"));
        item["volume_shares"] = volume
            ? Json(*volume * 10000.0) : Json(nullptr);
        item["amount_yuan"] = price && volume
            ? Json(*price * *volume * 10000.0) : Json(nullptr);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "date") > text_value(right, "date");
        });
    return result;
}

}  // namespace tdx
