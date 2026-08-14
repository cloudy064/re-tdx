#include "commodity_links_internal.hpp"

namespace tdx {
using namespace commodity_links_detail;

Json normalize_price_theme_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("price-theme rows must be an array");
    Json result = Json::array();
    std::set<std::string> ids;
    std::uint64_t rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++rank;
        const auto id = text_value(raw, "$ZQDM");
        const auto name = text_value(raw, "name");
        if (id.empty() || name.empty()) continue;
        if (!ids.insert(id).second) throw Error("duplicate theme_id: " + id);
        auto stock_set = parse_security_set(text_value(raw, "$S_ZQDM"), securities);
        Json row = Json::object();
        row["theme_id"] = id;
        row["source_rank"] = rank;
        row["name"] = name;
        row["stock_set"] = stock_set;
        row["stock_count"] = static_cast<std::uint64_t>(stock_set.size());
        row["trigger_date"] = iso_date(text_value(raw, "date"));
        row["logic"] = text_value(raw, "qdlj");
        row["latest_driver_date"] = iso_date(text_value(raw, "date1"));
        row["latest_driver_title"] = text_value(raw, "title");
        row["associated_commodity_id"] = text_value(raw, "gldm");
        row["stocks_resource"] = "zjtc1/" + id + ".jsn";
        row["drivers_resource"] = "zjtc2/" + id + ".jsn";
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_theme_stock_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("theme-stock rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        if (!digits(code, 6) || !seen.insert({id, code}).second) continue;
        Json row = Json::object();
        row["security"] = security_document(id, code, securities);
        row["trigger_price"] = number_json(number_value_any(raw, {"CFJ", "cfj"}));
        row["content"] = text_value_any(raw, {"MS", "ms"});
        row["return_since_trigger_pct"] = Json(nullptr);
        row["current_quote_available"] = false;
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_theme_driver_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("theme-driver rows must be an array");
    Json result = Json::array();
    std::set<std::string> seen;
    for (const auto& raw : rows.as_array()) {
        const auto id = text_value(raw, "$ZQDM");
        if (id.empty() || !seen.insert(id).second) continue;
        auto stock_set = parse_security_set(text_value(raw, "$S_ZQDM"), securities);
        Json row = Json::object();
        row["driver_id"] = id;
        row["date"] = iso_date(text_value(raw, "date"));
        row["title"] = text_value(raw, "name");
        row["stock_set"] = stock_set;
        row["stock_count"] = static_cast<std::uint64_t>(stock_set.size());
        row["stocks_resource"] = "zjtc3/" + id + ".jsn";
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_driver_stock_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    auto result = normalize_theme_stock_rows(rows, securities);
    for (auto& row : result.as_array()) {
        row.as_object().erase("content");
        row.as_object().erase("return_since_trigger_pct");
        row["driver_price"] = row.at("trigger_price");
        row.as_object().erase("trigger_price");
        row["return_since_driver_pct"] = Json(nullptr);
    }
    return result;
}

}  // namespace tdx
