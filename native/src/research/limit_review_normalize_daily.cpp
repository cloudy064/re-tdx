#include "limit_review_internal.hpp"

namespace tdx {

Json normalize_limit_review_daily_rows(
    const Json& rows, const std::string& category, const std::string& date,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::limit_review;
    if (!rows.is_array()) throw Error("limit-review daily rows must be an array");
    if (category != "limit-up" && category != "limit-down")
        throw Error("daily category must be limit-up or limit-down");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); }
        catch (...) { continue; }
        Json row = Json::object();
        row["category"] = category;
        row["direction"] = category == "limit-down" ? "down" : "up";
        row["date"] = date;
        row["security"] = security_document(id, code, securities);
        row["daily_change_pct"] = number_or_null(raw, "zdf");
        row["limit_category"] = text_value(raw, "lb");
        row["reason"] = text_value(raw, "yy");
        row["first_time"] = text_value(raw, "time1");
        row["last_time"] = text_value(raw, "time2");
        row["break_count"] = number_or_null(raw, "dkcs");
        row["streak_days"] = number_or_null(raw, "lbts");
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
