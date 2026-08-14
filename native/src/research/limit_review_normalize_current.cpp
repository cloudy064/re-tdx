#include "limit_review_internal.hpp"

namespace tdx {

Json normalize_limit_review_current_rows(
    const Json& rows, const std::string& category,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::limit_review;
    if (!rows.is_array()) throw Error("limit-review current rows must be an array");
    if (category != "limit-up" && category != "limit-down" && category != "surge")
        throw Error("current category must be limit-up, limit-down, or surge");
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
        row["security"] = security_document(id, code, securities);
        if (category == "surge") {
            row["date"] = text_value(raw, "tjrq");
            row["trigger_time"] = text_value(raw, "cfsj");
            row["reason"] = text_value(raw, "cfyy");
        } else {
            const bool down = category == "limit-down";
            row["date"] = text_value(raw, down ? "dtrq" : "ztrq");
            row["limit_type"] = text_value(raw, "ztlx");
            row["board_shape"] = text_value(raw, "bx");
            row["reason"] = text_value(raw, "yy");
            row["break_count"] = number_or_null(raw, down ? "dtcs" : "ztcs");
            row["first_time"] = text_value(raw, down ? "dtsj1" : "ztsj1");
            row["last_time"] = text_value(raw, down ? "dtsj2" : "ztsj2");
            row["streak_days"] = number_or_null(raw, "lbts");
            if (!down) {
                Json gene = Json::object();
                gene["past_year_limit_count"] = number_or_null(raw, "ztcs1");
                gene["next_open_above_5pct_count"] = number_or_null(raw, "yj5cs");
                gene["next_day_red_rate"] = number_or_null(raw, "crlpl");
                gene["next_day_mean_return_pct"] = number_or_null(raw, "crjzf");
                gene["first_board_seal_rate"] = number_or_null(raw, "sbfbl");
                gene["continuation_rate"] = number_or_null(raw, "ztlbl");
                row["limit_gene"] = std::move(gene);
            } else {
                row["limit_gene"] = Json(nullptr);
            }
        }
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
