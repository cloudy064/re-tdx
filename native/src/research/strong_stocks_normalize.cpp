#include "strong_stocks_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <set>

namespace tdx {

using namespace detail::strong_stocks;

Json normalize_strong_stock_intervals(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("strong-stock interval rows must be an array");
    Json result = Json::array();
    std::set<std::string> interval_ids;
    std::uint64_t source_rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++source_rank;
        const auto code = text_value(raw, "$ZQDM1");
        const auto interval_id = text_value(raw, "$ZQDM");
        if (!digits(code, 6) || !digits(interval_id, 18) ||
            interval_id.substr(0, 6) != code) continue;
        int id = -1;
        try { id = market_id(text_value(raw, "$SC1")); }
        catch (...) { continue; }
        if (!interval_ids.insert(interval_id).second)
            throw Error("duplicate strong-stock interval: " + interval_id);
        const auto statistics = text_value(raw, "jtjb");
        const auto stock_return = number_value(raw, "zf1");
        const auto index_return = number_value(raw, "zf2");
        Json row = Json::object();
        row["interval_id"] = interval_id;
        row["source_rank"] = source_rank;
        row["security"] = security_document(id, code, securities);
        row["start_date"] = iso_date(text_value(raw, "sj1"));
        row["end_date"] = iso_date(text_value(raw, "sj2"));
        row["interval_statistics"] = statistics;
        const auto trading_days = positive_integer_prefix(statistics, "天");
        const auto limit_up_days = positive_integer_prefix(statistics, "板");
        row["trading_days"] = trading_days ? Json(*trading_days) : Json(nullptr);
        row["limit_up_days"] = limit_up_days ? Json(*limit_up_days) : Json(nullptr);
        row["stock_return_pct"] = number_json(stock_return);
        row["index_return_pct"] = number_json(index_return);
        row["excess_return_pct"] = stock_return && index_return
            ? Json(*stock_return - *index_return) : Json(nullptr);
        row["return_finalized"] = stock_return.has_value() && index_return.has_value();
        row["detail_resource"] = "ygzl/" + interval_id + ".jsn";
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_strong_stock_detail(
    const Json& rows, const Json& interval,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("strong-stock detail rows must be an array");
    if (!interval.is_object() || !value_ptr(interval, "security"))
        throw Error("strong-stock detail requires normalized interval context");
    const auto& expected_security = interval.at("security");
    const auto expected_market = static_cast<int>(expected_security.at("market_id").as_number());
    const auto expected_code = expected_security.at("code").as_string();
    Json result = Json::array();
    std::set<std::string> dates;
    std::uint64_t source_rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++source_rank;
        const auto code = text_value(raw, "$ZQDM");
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); }
        catch (...) { continue; }
        if (code != expected_code || id != expected_market)
            throw Error("strong-stock detail security does not match interval");
        const auto raw_date = text_value(raw, "sj");
        if (!digits(raw_date, 8)) continue;
        if (!dates.insert(raw_date).second)
            throw Error("duplicate strong-stock detail date: " + raw_date);
        const auto limit_up = number_value(raw, "ztjs");
        const auto broken = number_value(raw, "zbjs");
        std::optional<double> seal_rate;
        if (limit_up && broken && *limit_up + *broken > 0)
            seal_rate = *limit_up / (*limit_up + *broken) * 100.0;
        Json row = Json::object();
        row["interval_id"] = interval.at("interval_id");
        row["source_rank"] = source_rank;
        row["date"] = iso_date(raw_date);
        row["security"] = security_document(id, code, securities);
        row["stock_return_pct"] = number_json(number_value(raw, "zf1"));
        row["turnover_amount_yuan"] = number_json(number_value(raw, "zcje"));
        row["limit_up_reason"] = text_value(raw, "yy");
        row["market_limit_up_count"] = number_json(limit_up);
        row["market_broken_limit_count"] = number_json(broken);
        row["market_limit_down_count"] = number_json(number_value(raw, "dtjs"));
        row["market_seal_success_pct"] = number_json(seal_rate);
        row["index_return_pct"] = number_json(number_value(raw, "zf2"));
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx

