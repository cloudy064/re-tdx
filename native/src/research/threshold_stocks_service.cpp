#include "threshold_stocks_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>
#include <cmath>

namespace tdx {

Json ThresholdStocksService::query(const ThresholdStocksQuery& input) {
    using namespace detail::threshold_stocks;
    const auto plan = make_query_plan(input);
    Json sources = Json::array();
    bool refreshed = false;
    int oldest_cache_age = 0;
    auto attach = [&](const FetchResult& fetched) {
        sources.push_back(jsn_source_metadata(fetched.document));
        refreshed = refreshed || fetched.refreshed;
        oldest_cache_age = std::max(oldest_cache_age, fetched.age_seconds);
    };

    Json records = Json::array(), trend = Json::array();
    Json summary = Json::object(), selected = Json(nullptr), catalog = Json::array();
    if (plan.view->kind == ViewKind::catalog) {
        for (const auto& universe : all_universes()) {
            const auto fetched = fetch(universe.master_resource, plan.options);
            attach(fetched);
            const auto history = normalize_threshold_history_rows(
                fetched.document.at("rows"), universe.id);
            Json entry = Json::object();
            entry["universe"] = universe.id;
            entry["label"] = universe.label;
            entry["history_periods"] = static_cast<std::uint64_t>(history.size());
            entry["latest"] = history.size() == 0
                ? Json(nullptr) : history.as_array().front();
            catalog.push_back(std::move(entry));
        }
        summary["universes"] = static_cast<std::uint64_t>(catalog.size());
        records = catalog;
    } else {
        const auto master = fetch(plan.universe->master_resource, plan.options);
        attach(master);
        auto history = normalize_threshold_history_rows(
            master.document.at("rows"), plan.universe->id);
        summary = history_summary(history);
        const Json period = selected_history(history, plan.options.date);
        selected = period;
        if (plan.view->kind == ViewKind::history) {
            sort_rows(history, *plan.view, *plan.sort, plan.options.order);
            records = std::move(history);
        }

        const auto key = period.at("detail_key").as_string();
        const auto trend_fetch = fetch("bygtj3/" + key + ".jsn", plan.options);
        attach(trend_fetch);
        trend = normalize_threshold_trend_rows(
            trend_fetch.document.at("rows"), plan.universe->id);
        Json trend_check = Json::object();
        trend_check["selected_date_present"] = false;
        trend_check["selected_count_matches"] = Json(nullptr);
        for (const auto& point : trend.as_array()) {
            if (point.at("date").as_string() != period.at("date").as_string()) continue;
            trend_check["selected_date_present"] = true;
            const auto a = json_number(point, "count");
            const auto b = json_number(period, "total_count");
            trend_check["selected_count_matches"] = a && b
                ? Json(std::abs(*a - *b) < 0.5) : Json(nullptr);
            break;
        }
        summary["trend_check"] = std::move(trend_check);

        if (plan.view->loads_members) {
            const auto members_fetch = fetch("bygtj1/" + key + ".jsn", plan.options);
            attach(members_fetch);
            auto members = normalize_threshold_member_rows(
                members_fetch.document.at("rows"), plan.universe->id, securities_);
            summary["members"] = member_summary(members, period);
            sort_rows(members, *plan.view, *plan.sort, plan.options.order);
            const auto needle = lower_ascii(trim(plan.options.query));
            for (const auto& row : members.as_array()) {
                if (plan.options.status != "all" &&
                    json_text(row, "status") != plan.options.status)
                    continue;
                if (plan.selected_market >= 0 &&
                    static_cast<int>(row.at("security").at("market_id").as_number()) !=
                        plan.selected_market)
                    continue;
                if (!plan.options.code.empty() &&
                    row.at("security").at("code").as_string() != plan.options.code)
                    continue;
                if (!needle.empty() && !json_contains(row, needle)) continue;
                records.push_back(row);
            }
        }
    }

    const auto matched = records.size();
    Json paged = Json::array();
    for (std::size_t index = static_cast<std::size_t>(plan.options.offset);
         index < records.size() &&
         paged.size() < static_cast<std::size_t>(plan.options.limit); ++index)
        paged.push_back(records.as_array()[index]);
    const auto health = jsn_sources_health(sources);
    Json result = Json::object();
    result["schema"] = "tdx-market-threshold-stocks-native-v1";
    result["generated_at"] = now_text();
    result["view"] = plan.options.view;
    result["universe"] = plan.options.universe;
    result["availability"] = health.at("stale").as_bool()
        ? "stale-cache" : (plan.view->kind != ViewKind::catalog && matched == 0)
            ? "empty" : "live";
    Json filters = Json::object();
    filters["date"] = plan.options.date.empty()
        ? Json(nullptr) : Json(plan.options.date);
    filters["status"] = plan.options.status;
    filters["market"] = plan.options.market.empty()
        ? Json(nullptr) : Json(plan.options.market);
    filters["code"] = plan.options.code.empty()
        ? Json(nullptr) : Json(plan.options.code);
    filters["query"] = plan.options.query;
    filters["sort"] = plan.options.sort;
    filters["order"] = plan.options.order;
    result["filters"] = std::move(filters);
    result["selected_period"] = std::move(selected);
    result["summary"] = std::move(summary);
    Json counts = Json::object();
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(paged.size());
    counts["trend_points"] = static_cast<std::uint64_t>(trend.size());
    result["counts"] = std::move(counts);
    result["records"] = std::move(paged);
    result["trend"] = std::move(trend);
    result["catalog"] = std::move(catalog);
    result["sources"] = std::move(sources);
    result["upstream_health"] = health;
    Json cache = Json::object();
    cache["ttl_seconds"] = plan.options.cache_ttl_seconds;
    cache["refreshed"] = refreshed;
    cache["oldest_age_seconds"] = oldest_cache_age;
    result["cache"] = std::move(cache);
    result["semantics"] =
        "TDX QYSZ page: high-price is the client 百元股 branch and mega-cap is the 千亿市值 branch. A dated member resource contains active members plus securities that exited on that date; active_count therefore equals continuing_count + entered_count, while total rows equal active_count + exited_count. spj/gjjz are intentionally typed as close price/price change for high-price and 100m-yuan market cap/net increase for mega-cap. Live quote syscols and client industry syscols are not fabricated.";
    return result;
}

}  // namespace tdx
