#include "strong_stocks_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>

namespace tdx {

using namespace detail::strong_stocks;

StrongStocksService::StrongStocksService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

Json StrongStocksService::fetch(const std::string& resource, bool refresh,
                                int cache_ttl_seconds, int timeout_ms,
                                bool& fetched) {
    const auto now = std::time(nullptr);
    const auto found = cache_.find(resource);
    if (!refresh && found != cache_.end() &&
        now - found->second.fetched_at < cache_ttl_seconds) {
        fetched = false;
        return found->second.document;
    }
    auto document = fetch_jsn_resource_rows(resource, "bi", timeout_ms);
    cache_[resource] = {document, std::time(nullptr)};
    fetched = true;
    return document;
}

Json StrongStocksService::query(const StrongStocksQuery& input) {
    StrongStocksQuery options = input;
    options.view = lower_ascii(trim(options.view));
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    options.interval_id = trim(options.interval_id);
    options.query = trim(options.query);
    options.from = compact_date(options.from, "from");
    options.to = compact_date(options.to, "to");
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    if (!valid_view(options.view))
        throw Error("view must be intervals, security, detail, or catalog");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = market_id(options.market);
        options.market = market_name(selected_market);
        if (!digits(options.code, 6)) throw Error("code must contain six digits");
    }
    if (options.view == "security" && options.code.empty())
        throw Error("security view requires market and code");
    if (options.view == "detail" && !digits(options.interval_id, 18))
        throw Error("detail view requires an 18-digit interval_id");
    if (!options.interval_id.empty() && !digits(options.interval_id, 18))
        throw Error("interval_id must contain 18 digits");
    if (!options.from.empty() && !options.to.empty() && options.from > options.to)
        throw Error("from must not be after to");
    const auto& view_policy = view_definition(options.view);
    if (options.sort.empty()) options.sort = std::string(view_policy.default_sort);
    if (options.order.empty()) options.order = std::string(view_policy.default_order);
    if (options.min_trading_days < 0 || options.min_trading_days > 1000)
        throw Error("min_trading_days must be in 0..1000");
    if (options.min_limit_up_days < 0 || options.min_limit_up_days > 1000)
        throw Error("min_limit_up_days must be in 0..1000");
    if (options.offset < 0 || options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");

    Json sources = Json::array(), records = Json::array(), summary = Json::object();
    bool refreshed = false;
    if (options.view == "catalog") {
        records = catalog_rows();
        summary["views"] = static_cast<std::uint64_t>(records.size());
    } else {
        bool main_fetched = false;
        const auto main = fetch(std::string(main_resource()), options.refresh,
                                options.cache_ttl_seconds, options.timeout_ms,
                                main_fetched);
        refreshed = main_fetched;
        sources.push_back(jsn_source_metadata(main));
        auto intervals = normalize_strong_stock_intervals(main.at("rows"), securities_);
        summary = interval_summary(intervals);

        const Json* selected_interval = nullptr;
        if (options.view == "detail") {
            for (const auto& interval : intervals.as_array()) {
                if (interval.at("interval_id").as_string() == options.interval_id) {
                    selected_interval = &interval;
                    break;
                }
            }
            if (!selected_interval)
                throw Error("interval_id is absent from the active strong-stock summary");
            const auto& security = selected_interval->at("security");
            if (selected_market >= 0 &&
                (static_cast<int>(security.at("market_id").as_number()) != selected_market ||
                 security.at("code").as_string() != options.code))
                throw Error("market/code does not match interval_id");
            const auto resource = selected_interval->at("detail_resource").as_string();
            bool detail_fetched = false;
            const auto detail = fetch(resource, options.refresh,
                                      options.cache_ttl_seconds, options.timeout_ms,
                                      detail_fetched);
            refreshed = refreshed || detail_fetched;
            sources.push_back(jsn_source_metadata(detail));
            records = normalize_strong_stock_detail(
                detail.at("rows"), *selected_interval, securities_);
            summary = detail_summary(records, *selected_interval);
            summary["interval"] = *selected_interval;
        } else {
            records = std::move(intervals);
        }
    }

    Json filtered = Json::array();
    const auto needle = lower_ascii(options.query);
    for (const auto& row : records.as_array()) {
        if (options.view == "catalog") {
            if (needle.empty() || json_contains(row, needle)) filtered.push_back(row);
            continue;
        }
        const auto& security = row.at("security");
        if (selected_market >= 0 &&
            (static_cast<int>(security.at("market_id").as_number()) != selected_market ||
             security.at("code").as_string() != options.code)) continue;
        if (!options.interval_id.empty() && options.view != "detail" &&
            row.at("interval_id").as_string() != options.interval_id) continue;
        if (options.view != "detail") {
            const auto start = json_text(row, "start_date");
            const auto end = json_text(row, "end_date");
            if (!options.from.empty() && end < iso_date(options.from)) continue;
            if (!options.to.empty() && start > iso_date(options.to)) continue;
            const auto days = json_number(row, "trading_days");
            const auto limit_ups = json_number(row, "limit_up_days");
            if (options.min_trading_days > 0 &&
                (!days || *days < options.min_trading_days)) continue;
            if (options.min_limit_up_days > 0 &&
                (!limit_ups || *limit_ups < options.min_limit_up_days)) continue;
        } else {
            const auto date = json_text(row, "date");
            if (!options.from.empty() && date < iso_date(options.from)) continue;
            if (!options.to.empty() && date > iso_date(options.to)) continue;
        }
        if (!needle.empty() && !json_contains(row, needle)) continue;
        filtered.push_back(row);
    }
    if (options.view != "catalog")
        sort_strong_stock_rows(filtered, options.view, options.sort, options.order);

    const auto matched = filtered.size();
    Json paged = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < filtered.size() && paged.size() < static_cast<std::size_t>(options.limit);
         ++index) paged.push_back(filtered.as_array()[index]);
    const auto health = jsn_sources_health(sources);
    Json result = Json::object();
    result["schema"] = "tdx-market-strong-stocks-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["availability"] = options.view == "catalog" ? "catalog" :
        health.at("stale").as_bool() ? "stale-cache" : matched == 0 ? "empty" : "live";
    Json filters = Json::object();
    filters["market"] = options.market.empty() ? Json(nullptr) : Json(options.market);
    filters["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    filters["interval_id"] = options.interval_id.empty() ? Json(nullptr) : Json(options.interval_id);
    filters["query"] = options.query;
    filters["from"] = options.from.empty() ? Json(nullptr) : Json(iso_date(options.from));
    filters["to"] = options.to.empty() ? Json(nullptr) : Json(iso_date(options.to));
    filters["min_trading_days"] = options.min_trading_days;
    filters["min_limit_up_days"] = options.min_limit_up_days;
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    result["filters"] = std::move(filters);
    result["summary"] = std::move(summary);
    Json counts = Json::object();
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(paged.size());
    counts["sources"] = static_cast<std::uint64_t>(sources.size());
    result["counts"] = std::move(counts);
    result["records"] = std::move(paged);
    result["catalog"] = catalog_rows();
    result["sources"] = std::move(sources);
    result["upstream_health"] = health;
    Json cache = Json::object();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["refreshed"] = refreshed;
    result["cache"] = std::move(cache);
    Json units = Json::object();
    units["stock_return_pct"] = "percent";
    units["index_return_pct"] = "percent";
    units["excess_return_pct"] = "percentage-points";
    units["turnover_amount_yuan"] = "yuan";
    units["market_seal_success_pct"] = "percent";
    result["units"] = std::move(units);
    result["semantics"] =
        "TDX QSGFX/func_ygzl101 defines historical strong-stock limit-up lifecycles. The main table supplies interval dates, N-day/M-limit-up statistics, finalized stock return and Shanghai Composite return; a blank return is retained as null, not interpreted as zero. ygzl/<interval-id>.jsn supplies the per-trading-day stock return, turnover, limit-up reason and whole-market limit-up/broken-limit/limit-down breadth. Sealing success is reproduced from the client formula limit_up/(limit_up+broken)*100. Current quote and industry are client syscols absent from JSN and are not fabricated. This is public static research data and does not require L2.";
    return result;
}

}  // namespace tdx

