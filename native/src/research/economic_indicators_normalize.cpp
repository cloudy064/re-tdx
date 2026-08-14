// The public normalization surface: master rows, historical series and the
// related-security set. Each maps upstream JJZB field names onto stable keys and
// keeps the untouched upstream row under "raw".
#include "tdx/economic_indicators_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <set>

namespace tdx {

using namespace tdx::economic_indicator_detail;

Json normalize_economic_indicator_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("economic indicator rows must be an array");
    Json result = Json::array();
    std::set<std::string> ids;
    for (const auto& raw : rows.as_array()) {
        const auto id = text_value(raw, "$ZQDM");
        if (!valid_indicator_id(id) || !ids.insert(id).second)
            throw Error("economic indicator id is missing, invalid, or duplicated");
        Json row = Json::object();
        row["indicator_id"] = id;
        row["name"] = text_value(raw, "NAME");
        row["indicator_type"] = text_value(raw, "lb");
        row["current_value"] = number_json(number_value(raw, "sz"));
        row["unit"] = text_value(raw, "dw");
        row["month_on_month_pct"] = number_json(number_value(raw, "hb"));
        row["year_on_year_pct"] = number_json(number_value(raw, "tb"));
        row["frequency"] = text_value(raw, "pl");
        row["update_date"] = iso_date(text_value(raw, "date"));
        row["report_period"] = iso_date(text_value(raw, "bgq"));
        row["history_resource"] = "jjzb1/" + id + ".jsn";
        row["related_resource"] = "jjzb2/" + id + ".jsn";
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_economic_indicator_history(const Json& rows) {
    if (!rows.is_array()) throw Error("economic indicator history rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto date = iso_date(text_value(raw, "date"));
        const auto value = number_value(raw, "data");
        if (date.empty() || !value) continue;
        Json row = Json::object();
        row["date"] = date;
        row["value"] = *value;
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return left.at("date").as_string() < right.at("date").as_string();
        });
    return result;
}

Json normalize_economic_indicator_relations(
    const Json& rows, const Json& quote_rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("economic indicator relation rows must be an array");
    Json result = Json::array();
    std::set<std::pair<int, std::string>> seen;
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int market = -1;
        try { market = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        if (!seen.insert({market, code}).second) continue;
        const auto* quote = find_quote(quote_rows, market, code);
        Json row = Json::object();
        row["security"] = security_document(market, code, securities);
        row["industry"] = text_value(raw, "hy");
        row["last_price"] = quote_metric(quote, "last_price");
        row["change_pct"] = quote_metric(quote, "change_pct");
        row["amount_yuan"] = quote_metric(quote, "amount");
        row["quote_available"] = quote != nullptr;
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto& li = left.at("industry").as_string();
            const auto& ri = right.at("industry").as_string();
            if (li != ri) return li < ri;
            return left.at("security").at("security_id").as_string() <
                   right.at("security").at("security_id").as_string();
        });
    return result;
}

void sort_economic_indicator_rows(Json& rows, const std::string& sort_value,
                                  const std::string& order_value) {
    if (!rows.is_array()) throw Error("economic indicator rows must be an array");
    const auto sort = lower_ascii(trim(sort_value));
    const auto order = lower_ascii(trim(order_value));
    if (!std::set<std::string>{"update-date", "name", "value", "mom", "yoy"}.count(sort))
        throw Error("indicator sort must be update-date, name, value, mom, or yoy");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool desc = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort == "update-date" || sort == "name") {
                const auto key = sort == "update-date" ? "update_date" : "name";
                const auto& a = left.at(key).as_string();
                const auto& b = right.at(key).as_string();
                if (a != b) return desc ? a > b : a < b;
            } else {
                const auto key = sort == "value" ? "current_value"
                    : sort == "mom" ? "month_on_month_pct" : "year_on_year_pct";
                const auto a = json_number(left, key);
                const auto b = json_number(right, key);
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return desc ? *a > *b : *a < *b;
            }
            return left.at("indicator_id").as_string() < right.at("indicator_id").as_string();
        });
}

}  // namespace tdx
