#include "commodity_links_internal.hpp"

namespace tdx {
using namespace commodity_links_detail;

Json normalize_commodity_stock_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("commodity-stock rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        if (!digits(code, 6) || !seen.insert({id, code}).second) continue;
        Json row = Json::object();
        row["security"] = security_document(id, code, securities);
        row["reference_price_3m"] = number_json(number_value(raw, "price1"));
        row["return_since_reference_pct"] = Json(nullptr);
        row["current_quote_available"] = false;
        row["investment_logic"] = text_value(raw, "tzlj");
        row["description"] = text_value(raw, "xxsm");
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_commodity_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("commodity-security rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        if (!digits(code, 6) || !seen.insert({id, code}).second) continue;
        Json row = Json::object();
        row["security"] = security_document(id, code, securities);
        row["quote_fields_available"] = false;
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
