#include "hk_events_internal.hpp"

#include "tdx/minute.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace tdx {

Json HkEventService::query(const HkEventQuery& options) {
    const auto* view = detail::find_hk_event_view(options.view);
    if (options.view != "all" && !view)
        throw Error("view must be all, dividends, holdings, short-selling, or applications");
    if (options.limit < 1 || options.limit > 20000)
        throw Error("limit must be in 1..20000");
    if (!options.code.empty() && !detail::valid_hk_code(options.code))
        throw Error("code must contain five digits");

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto folded = lower_ascii(trim(options.query));
    const std::string_view selected_kind = view ? view->kind : std::string_view{};
    Json rows = Json::array();
    for (const auto& row : master.at("rows").as_array()) {
        if (!selected_kind.empty() && detail::json_text(row, "kind") != selected_kind)
            continue;
        const auto date = detail::hk_date_key(detail::json_text(row, "date"));
        if (!options.date_from.empty() && date < detail::hk_date_key(options.date_from)) continue;
        if (!options.date_to.empty() && date > detail::hk_date_key(options.date_to)) continue;
        if (!options.code.empty()) {
            const auto* security = detail::json_field(row, "security");
            if (!security || security->is_null() ||
                detail::json_text(*security, "code") != options.code) continue;
        }
        if (!folded.empty() && lower_ascii(row.dump(-1)).find(folded) == std::string::npos)
            continue;
        rows.push_back(row);
    }
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto left_date = detail::hk_date_key(detail::json_text(left, "date"));
            const auto right_date = detail::hk_date_key(detail::json_text(right, "date"));
            if (left_date != right_date) return left_date > right_date;
            const auto left_kind = detail::json_text(left, "kind");
            const auto right_kind = detail::json_text(right, "kind");
            if (left_kind != right_kind) return left_kind < right_kind;
            return detail::json_text(left, "event_id") < detail::json_text(right, "event_id");
        });

    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> securities;
    double short_amount = 0.0;
    double turnover_amount = 0.0;
    std::string earliest_date;
    std::string latest_date;
    for (const auto& row : rows.as_array()) {
        ++counts[detail::json_text(row, "kind")];
        const auto date = detail::hk_date_key(detail::json_text(row, "date"));
        if (!date.empty()) {
            if (earliest_date.empty() || date < earliest_date) earliest_date = date;
            if (latest_date.empty() || date > latest_date) latest_date = date;
        }
        const auto* security = detail::json_field(row, "security");
        if (security && !security->is_null())
            securities.insert(detail::json_text(*security, "security_id"));
        short_amount += detail::json_number_value(row, "short_amount_currency_units").value_or(0.0);
        turnover_amount += detail::json_number_value(row, "turnover_currency_units").value_or(0.0);
    }
    const auto matched = rows.size();
    while (static_cast<int>(rows.size()) > options.limit)
        rows.as_array().pop_back();

    Json summary = Json::object();
    summary["dividends"] = counts["dividend"];
    summary["holding_disclosures"] = counts["holding-disclosure"];
    summary["short_selling"] = counts["short-selling"];
    summary["listing_applications"] = counts["listing-application"];
    summary["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    summary["short_amount_currency_units"] = short_amount;
    summary["turnover_amount_currency_units"] = turnover_amount;
    summary["earliest_date"] = earliest_date;
    summary["latest_date"] = latest_date;

    Json result = Json::object();
    result["schema"] = "tdx-market-hk-events-native-v1";
    result["generated_at"] = detail::current_time_text();
    result["view"] = options.view;
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["rows"] = std::move(rows);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

Json HkEventService::query_short_history(const HkShortHistoryQuery& options) {
    if (options.market != "31" && options.market != "48")
        throw Error("HK short history market must be 31 or 48");
    if (!detail::valid_hk_code(options.code))
        throw Error("code must contain five digits");
    if (options.pages < 1 || options.pages > 20)
        throw Error("pages must be in 1..20");
    if (options.page_size < 1 || options.page_size > 800)
        throw Error("page_size must be in 1..800");
    if (options.start < 0 || options.start > 65535)
        throw Error("start must be in 0..65535");

    HkEventQuery event_query;
    event_query.view = "short-selling";
    event_query.code = options.code;
    event_query.refresh = options.refresh;
    event_query.limit = 20000;
    event_query.cache_ttl_seconds = options.cache_ttl_seconds;
    event_query.timeout_ms = options.timeout_ms;
    const auto events = query(event_query);
    const auto kline = fetch_kline_document(
        options.market, options.code, "stock", "day", options.pages,
        options.page_size, options.start, "all", options.timeout_ms);
    auto result = normalize_hk_short_history(
        kline, events.at("rows"), options.market, options.code);
    result["generated_at"] = detail::current_time_text();
    result["event_sources"] = events.at("sources");
    result["sources"] = events.at("sources");
    result["event_cache"] = events.at("cache");
    result["paging"] = Json::object();
    result["paging"]["start"] = static_cast<std::uint64_t>(options.start);
    result["paging"]["page_size"] = static_cast<std::uint64_t>(options.page_size);
    result["paging"]["pages"] = static_cast<std::uint64_t>(options.pages);
    if (const auto* next = detail::json_field(kline, "next_start"))
        result["paging"]["next_start"] = *next;
    if (const auto* more = detail::json_field(kline, "has_more"))
        result["paging"]["has_more"] = *more;
    return result;
}

}  // namespace tdx
