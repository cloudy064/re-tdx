#include "tdx/announcement_signals.hpp"
#include "tdx/announcement_signals_internal.hpp"
#include "tdx/jsn.hpp"

#include <set>

namespace tdx {

using detail::announcement_signals::resources;
using detail::announcement_signals::resource_count;
using detail::announcement_signals::catalog_rows;
using detail::announcement_signals::market_id;
using detail::announcement_signals::market_name;
using detail::announcement_signals::digits;
using detail::announcement_signals::compact_date;
using detail::announcement_signals::iso_date;
using detail::announcement_signals::json_contains;
using detail::announcement_signals::now_text;
using detail::announcement_signals::history_resource;
using detail::announcement_signals::summarize;

Json AnnouncementSignalsService::query(const AnnouncementSignalsQuery& input) {
    AnnouncementSignalsQuery options = input;
    options.view = lower_ascii(trim(options.view));
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    options.query = trim(options.query);
    options.direction = lower_ascii(trim(options.direction));
    options.announcement_type = trim(options.announcement_type);
    options.from = compact_date(options.from, "from");
    options.to = compact_date(options.to, "to");
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    if (!std::set<std::string>{"selected", "risks", "security", "history", "catalog"}.count(options.view))
        throw Error("view must be selected, risks, security, history, or catalog");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = market_id(options.market);
        options.market = market_name(selected_market);
        if (!digits(options.code, 6)) throw Error("code must contain six digits");
    }
    if ((options.view == "security" || options.view == "history") && selected_market < 0)
        throw Error(options.view + " view requires market and code");
    if (!std::set<std::string>{"all", "bullish", "bearish", "unknown"}.count(options.direction))
        throw Error("direction must be all, bullish, bearish, or unknown");
    if (!options.from.empty() && !options.to.empty() && options.from > options.to)
        throw Error("from must not be after to");
    if (options.offset < 0 || options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");

    Json sources = Json::array(), records = Json::array(), warnings = Json::array();
    bool refreshed = false;
    auto append_main = [&](const char* resource, const char* family) {
        bool fetched = false;
        const auto document = fetch(resource, options.refresh,
                                    options.cache_ttl_seconds, options.timeout_ms, fetched);
        refreshed = refreshed || fetched;
        sources.push_back(jsn_source_metadata(document));
        auto normalized = normalize_announcement_signal_rows(
            document.at("rows"), family, securities_);
        for (auto& row : normalized.as_array()) records.push_back(std::move(row));
    };
    auto append_history = [&]() {
        const auto resource = history_resource(selected_market, options.code);
        try {
            bool fetched = false;
            const auto document = fetch(resource, options.refresh,
                                        options.cache_ttl_seconds, options.timeout_ms, fetched);
            refreshed = refreshed || fetched;
            sources.push_back(jsn_source_metadata(document));
            auto normalized = normalize_announcement_history_rows(
                document.at("rows"), selected_market, options.code, securities_);
            for (auto& row : normalized.as_array()) records.push_back(std::move(row));
        } catch (const std::exception& error) {
            Json warning = Json::object();
            warning["resource"] = resource;
            warning["message"] = error.what();
            warnings.push_back(std::move(warning));
        }
    };

    if (options.view == "catalog") {
        records = catalog_rows();
    } else if (options.view == "selected") {
        append_main(resources[0].resource, "selected");
    } else if (options.view == "risks") {
        append_main(resources[1].resource, "risk");
    } else if (options.view == "history") {
        append_history();
    } else {
        append_main(resources[0].resource, "selected");
        append_main(resources[1].resource, "risk");
        if (options.include_history) append_history();
    }

    Json filtered = Json::array();
    const auto needle = lower_ascii(options.query);
    const auto type_needle = lower_ascii(options.announcement_type);
    for (const auto& row : records.as_array()) {
        if (options.view == "catalog") {
            if (needle.empty() || json_contains(row, needle)) filtered.push_back(row);
            continue;
        }
        const auto& security = row.at("security");
        if (selected_market >= 0 &&
            (static_cast<int>(security.at("market_id").as_number()) != selected_market ||
             security.at("code").as_string() != options.code)) continue;
        if (options.direction != "all" && row.at("direction").as_string() != options.direction)
            continue;
        if (!type_needle.empty() &&
            lower_ascii(row.at("announcement_type").as_string()).find(type_needle) == std::string::npos)
            continue;
        const auto date = row.at("date").as_string();
        if (!options.from.empty() && date < iso_date(options.from)) continue;
        if (!options.to.empty() && date > iso_date(options.to)) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        filtered.push_back(row);
    }
    if (options.view != "catalog")
        sort_announcement_signal_rows(filtered, options.sort, options.order);

    const auto summary = options.view == "catalog" ? Json::object() : summarize(filtered);
    const auto matched = filtered.size();
    Json paged = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < filtered.size() && paged.size() < static_cast<std::size_t>(options.limit);
         ++index) paged.push_back(filtered.as_array()[index]);
    const auto health = jsn_sources_health(sources);
    Json result = Json::object();
    result["schema"] = "tdx-market-announcement-signals-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["availability"] = options.view == "catalog" ? "catalog" :
        sources.size() == 0 ? "unavailable" : health.at("stale").as_bool() ? "stale-cache" :
        matched == 0 ? "empty" : "live";
    Json filters = Json::object();
    filters["market"] = options.market.empty() ? Json(nullptr) : Json(options.market);
    filters["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    filters["query"] = options.query;
    filters["direction"] = options.direction;
    filters["announcement_type"] = options.announcement_type;
    filters["from"] = options.from.empty() ? Json(nullptr) : Json(iso_date(options.from));
    filters["to"] = options.to.empty() ? Json(nullptr) : Json(iso_date(options.to));
    filters["include_history"] = options.include_history;
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    result["filters"] = std::move(filters);
    result["summary"] = options.view == "catalog" ? Json::object() : summary;
    Json counts = Json::object();
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(paged.size());
    counts["sources"] = static_cast<std::uint64_t>(sources.size());
    counts["warnings"] = static_cast<std::uint64_t>(warnings.size());
    result["counts"] = std::move(counts);
    result["records"] = std::move(paged);
    result["catalog"] = catalog_rows();
    result["sources"] = std::move(sources);
    result["upstream_health"] = health;
    result["warnings"] = std::move(warnings);
    Json cache = Json::object();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["refreshed"] = refreshed;
    result["cache"] = std::move(cache);
    Json units = Json::object();
    units["recent_3d_return_pct"] = "percent";
    units["recent_10d_return_pct"] = "percent";
    units["pre_3d_return_pct"] = "percent";
    units["post_3d_return_pct"] = "percent";
    result["units"] = std::move(units);
    result["semantics"] =
        "TDX ZXJX/SJQD4/JYFX announcement selections. func_zxjx101 is the current curated bullish/bearish announcement list and func_zxjx103 is the current risk-announcement list. Their zf1/zf2 columns mean recent 3-day and 10-day security return. ggjx/<market-id><code>.jsn is per-security history, where the same raw zf1/zf2 names instead mean the 3-day return before and after the announcement; the normalized fields are deliberately separate. TXT: URLs embedded in titles are exposed as pdf_url. Blank forward returns remain null. Quote, turnover, market-cap and industry client syscols are absent from these JSN resources and are not fabricated. This is public static research data and does not require L2.";
    return result;
}

}  // namespace tdx
