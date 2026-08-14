#include "block_trades_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace tdx {

using block_trade_detail::canonical_market_id;
using block_trade_detail::number_or_null;
using block_trade_detail::number_value;
using block_trade_detail::scaled_number;
using block_trade_detail::security_document;
using block_trade_detail::six_digits;
using block_trade_detail::text_value;
using block_trade_detail::valid_month;

Json normalize_block_trade_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("block-trade master rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!six_digits(code)) continue;
        const int market_id = canonical_market_id(text_value(row, "$SC"));
        Json item = Json::object();
        item["security"] = security_document(market_id, code, securities);
        item["date"] = text_value(row, "date");
        item["price"] = number_or_null(number_value(row, "cjj"));
        item["close"] = number_or_null(number_value(row, "spj"));
        item["premium_pct"] = number_or_null(number_value(row, "zyj"));
        item["amount_yuan"] = scaled_number(row, "cje", 10000.0);
        item["volume_shares"] = scaled_number(row, "cjl", 10000.0);
        Json frequency = Json::object();
        frequency["days_7"] = number_or_null(number_value(row, "sb7"));
        frequency["days_30"] = number_or_null(number_value(row, "sb30"));
        frequency["days_90"] = number_or_null(number_value(row, "sb90"));
        item["frequency"] = std::move(frequency);
        Json buyer = Json::object();
        buyer["name"] = text_value(row, "byyb");
        buyer["label"] = text_value(row, "mfxw1");
        item["buyer"] = std::move(buyer);
        Json seller = Json::object();
        seller["name"] = text_value(row, "syyb");
        seller["label"] = text_value(row, "mfxw2");
        item["seller"] = std::move(seller);
        item["security_type"] = text_value(row, "zttype");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            if (text_value(left, "date") != text_value(right, "date"))
                return text_value(left, "date") > text_value(right, "date");
            return number_value(left, "amount_yuan").value_or(0.0) >
                   number_value(right, "amount_yuan").value_or(0.0);
        });
    return result;
}

Json normalize_block_trade_history_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("block-trade history rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["date"] = text_value(row, "date");
        item["price"] = number_or_null(number_value(row, "cjj"));
        item["amount_yuan"] = scaled_number(row, "cje", 10000.0);
        item["buyer"] = text_value(row, "byyb");
        item["seller"] = text_value(row, "syyb");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "date") > text_value(right, "date");
        });
    return result;
}

Json normalize_block_trade_intention_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool with_security) {
    if (!rows.is_array()) throw Error("block-trade intention rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        if (with_security) {
            const auto code = text_value(row, "$ZQDM");
            if (!six_digits(code)) continue;
            const int market_id = canonical_market_id(text_value(row, "$SC"));
            item["security"] = security_document(market_id, code, securities);
        }
        item["date"] = text_value(row, "date");
        item["declaration_price"] = number_or_null(number_value(row, "sbjg"));
        item["close"] = number_or_null(number_value(row, "drspj"));
        item["premium_pct"] = number_or_null(number_value(row, "yjl"));
        item["quantity_shares"] = scaled_number(row, "sbsl", 10000.0);
        item["direction"] = text_value(row, "mmfx");
        const auto price = number_value(row, "sbjg");
        const auto quantity = number_value(row, "sbsl");
        item["amount_yuan"] = price && quantity
            ? Json(*price * *quantity * 10000.0) : Json(nullptr);
        if (with_security) {
            Json frequency = Json::object();
            frequency["days_7"] = number_or_null(number_value(row, "sb7"));
            frequency["days_30"] = number_or_null(number_value(row, "sb30"));
            frequency["days_90"] = number_or_null(number_value(row, "sb90"));
            item["frequency"] = std::move(frequency);
        }
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "date") > text_value(right, "date");
        });
    return result;
}

Json normalize_block_trade_month_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("block-trade month rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto month = text_value(row, "$ZQDM");
        if (!valid_month(month)) continue;
        Json item = Json::object();
        item["month"] = month;
        item["amount_yuan"] = scaled_number(row, "cje", 10000.0);
        item["volume_shares"] = scaled_number(row, "cjl", 10000.0);
        item["premium_pct"] = number_or_null(number_value(row, "yzjl"));
        item["trade_count"] = number_or_null(number_value(row, "cjcs"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "month") > text_value(right, "month");
        });
    return result;
}

}  // namespace tdx
