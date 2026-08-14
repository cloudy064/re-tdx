#include "limit_review_internal.hpp"

namespace tdx {

Json normalize_limit_review_annual_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::limit_review;
    if (!rows.is_array()) throw Error("limit-review annual rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); }
        catch (...) { continue; }
        Json row = Json::object();
        row["category"] = "annual";
        row["security"] = security_document(id, code, securities);
        row["latest_date"] = text_value(raw, "DATE");
        row["newly_listed"] = text_value(raw, "CXG");
        Json up = Json::object();
        up["close_count"] = number_or_null(raw, "NZTC1");
        up["intraday_count"] = number_or_null(raw, "NZTC2");
        up["total_count"] = number_or_null(raw, "NZTC");
        up["average_turnover_pct"] = number_or_null(raw, "PJHSL1");
        up["next_day_gap_up_rate_pct"] = number_or_null(raw, "GKL");
        up["next_day_higher_close_rate_pct"] = number_or_null(raw, "GSL");
        row["limit_up"] = std::move(up);
        Json down = Json::object();
        down["close_count"] = number_or_null(raw, "NDTC1");
        down["intraday_count"] = number_or_null(raw, "NDTC2");
        down["total_count"] = number_or_null(raw, "NDTC");
        down["average_turnover_pct"] = number_or_null(raw, "PJHSL2");
        down["next_day_gap_down_rate_pct"] = number_or_null(raw, "DKL");
        down["next_day_lower_close_rate_pct"] = number_or_null(raw, "DSL");
        row["limit_down"] = std::move(down);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_limit_review_market_history_rows(const Json& rows) {
    using namespace detail::limit_review;
    if (!rows.is_array()) throw Error("limit-review market-history rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto date = text_value(raw, "$ZQDM");
        if (!digits(date, 8)) continue;
        Json row = Json::object();
        row["category"] = "market-history";
        row["date"] = date;
        row["shanghai_index_change_pct"] = number_or_null(raw, "szzs1");
        row["market_turnover_100m_yuan"] = number_or_null(raw, "szzs2");
        row["market_turnover_yuan"] = number_or_null(raw, "szzs2", 100000000.0);
        Json up = Json::object();
        up["all_count"] = number_or_null(raw, "ztjs1");
        up["closed_count"] = number_or_null(raw, "ztjs2");
        up["broken_count"] = number_or_null(raw, "ztjs3");
        up["streak_count"] = number_or_null(raw, "ztjs4");
        up["one_price_count"] = number_or_null(raw, "ztjs5");
        up["total_seal_amount_yuan"] = number_or_null(raw, "ztjs6");
        up["max_seal_amount_yuan"] = number_or_null(raw, "ztjs7");
        up["total_turnover_yuan"] = number_or_null(raw, "ztjs8");
        up["at_limit_turnover_yuan"] = number_or_null(raw, "ztjs9");
        up["max_streak"] = number_or_null(raw, "szcz13");
        up["seal_rate_pct"] = number_or_null(raw, "szcz14");
        Json distribution = Json::object();
        for (int streak = 1; streak <= 12; ++streak)
            distribution[std::to_string(streak)] =
                number_or_null(raw, "szcz" + std::to_string(streak));
        up["streak_distribution"] = std::move(distribution);
        row["limit_up"] = std::move(up);
        Json down = Json::object();
        down["all_count"] = number_or_null(raw, "dtjs1");
        down["closed_count"] = number_or_null(raw, "dtjs2");
        down["intraday_count"] = number_or_null(raw, "dtjs3");
        row["limit_down"] = std::move(down);
        row["up_down_count_ratio"] = number_or_null(raw, "zdtb1");
        row["up_down_break_ratio"] = number_or_null(raw, "zdtb2");
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_limit_review_security_history_rows(const Json& rows) {
    using namespace detail::limit_review;
    if (!rows.is_array()) throw Error("limit-review security-history rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        Json row = Json::object();
        row["date"] = text_value(raw, "date");
        row["limit_type"] = text_value(raw, "lx");
        row["reason"] = text_value(raw, "yy");
        row["streak_days"] = number_or_null(raw, "lbts");
        row["explanation"] = text_value(raw, "Title");
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
