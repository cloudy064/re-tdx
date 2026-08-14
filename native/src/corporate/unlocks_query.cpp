#include "unlocks_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

namespace tdx {

using namespace unlocks_detail;

Json UnlockService::query(const UnlockQuery& options) {
    const auto view = lower_ascii(trim(options.view));
    if (view != "calendar" && view != "recent-large" && view != "monthly-pressure")
        throw Error("unlock view must be calendar, recent-large or monthly-pressure");
    const bool recent_large = view == "recent-large";
    const bool monthly_pressure = view == "monthly-pressure";
    if (options.limit < 1 || options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (options.detail_limit < 1 || options.detail_limit > 5000)
        throw Error("detail_limit must be in 1..5000");
    if (options.master_cache_ttl_seconds < 0 || options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 || options.detail_cache_ttl_seconds > 86400)
        throw Error("unlock cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const bool security_mode = !options.market.empty() || !options.code.empty();
    if (security_mode && (options.market.empty() || options.code.empty()))
        throw Error("security selection requires both market and code");
    if (security_mode && !six_digits(options.code))
        throw Error("code must contain six digits");
    if (!options.detail_id.empty() &&
        (options.detail_id.size() != 14 ||
         !std::all_of(options.detail_id.begin(), options.detail_id.end(), [](char ch) {
             return ch >= '0' && ch <= '9';
         })))
        throw Error("detail_id must contain date plus six-digit code");
    if (recent_large && !options.detail_id.empty())
        throw Error("recent-large unlock history has no shareholder detail resource");
    if (!options.start_date.empty() && !eight_digits(options.start_date))
        throw Error("start_date must use YYYYMMDD");
    if (!options.end_date.empty() && !eight_digits(options.end_date))
        throw Error("end_date must use YYYYMMDD");
    if (!options.start_date.empty() && !options.end_date.empty() &&
        options.start_date > options.end_date)
        throw Error("start_date must not be later than end_date");

    if (monthly_pressure) {
        if (security_mode || !options.detail_id.empty() || options.include_details)
            throw Error("monthly-pressure is a market aggregate and has no security details");
        auto master = fetch_monthly_pressure(options);
        Json months = Json::array();
        const auto needle = lower_ascii(trim(options.query));
        for (const auto& row : master.document.at("months").as_array()) {
            const auto month = text_value(row, "month");
            if (!options.start_date.empty() && month < options.start_date.substr(0, 6)) continue;
            if (!options.end_date.empty() && month > options.end_date.substr(0, 6)) continue;
            if (!needle.empty() && lower_ascii(row.dump(-1)).find(needle) == std::string::npos)
                continue;
            months.push_back(row);
        }
        const auto matched = months.size();
        if (static_cast<int>(months.size()) > options.limit)
            months.as_array().resize(static_cast<std::size_t>(options.limit));
        double total_shares = 0.0, total_value = 0.0, peak_value = -1.0;
        std::string first_month, last_month, peak_month;
        std::uint64_t formula_checked = 0, formula_mismatches = 0;
        for (const auto& row : months.as_array()) {
            const auto month = text_value(row, "month");
            if (first_month.empty() || month < first_month) first_month = month;
            if (month > last_month) last_month = month;
            const auto shares = number_value(row, "unlock_shares").value_or(0.0);
            const auto value = number_value(row, "unlock_market_value_yuan").value_or(0.0);
            total_shares += shares;
            total_value += value;
            if (value > peak_value) { peak_value = value; peak_month = month; }
            const auto raw_100m = number_value(row.at("raw"), "jjsz");
            const auto normalized = number_value(row, "unlock_market_value_yuan");
            if (raw_100m && normalized) {
                ++formula_checked;
                if (std::abs(*normalized - *raw_100m * 100000000.0) > 0.01)
                    ++formula_mismatches;
            }
        }
        Json summary = Json::object();
        summary["months"] = static_cast<std::uint64_t>(matched);
        summary["first_month"] = first_month;
        summary["last_month"] = last_month;
        summary["total_unlock_shares"] = total_shares;
        summary["total_unlock_market_value_yuan"] = total_value;
        summary["peak_month"] = peak_month;
        summary["peak_unlock_market_value_yuan"] = peak_value < 0 ? Json(nullptr) : Json(peak_value);
        summary["formula_checked"] = formula_checked;
        summary["formula_mismatches"] = formula_mismatches;
        Json result = Json::object();
        result["schema"] = "tdx-market-unlocks-native-v1";
        result["generated_at"] = now_text();
        result["view"] = view;
        result["mode"] = "monthly-pressure";
        result["availability"] = matched ? "live" : "empty";
        result["months"] = std::move(months);
        result["events"] = Json::array();
        result["selected_event"] = Json(nullptr);
        result["selected_security"] = Json(nullptr);
        result["details"] = Json::array();
        result["detail_errors"] = Json::array();
        result["summary"] = summary;
        result["catalog_summary"] = summary;
        Json sources = Json::array();
        sources.push_back(master.document.at("source"));
        result["sources"] = std::move(sources);
        Json cache = Json::object();
        cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
        cache["master_refreshed"] = master.refreshed;
        cache["master_age_seconds"] = master.age_seconds;
        cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
        cache["detail_refreshed"] = false;
        cache["detail_age_seconds"] = 0;
        cache["stale"] = false;
        result["cache"] = std::move(cache);
        Json counts = Json::object();
        counts["catalog_events"] = 0;
        counts["matched_events"] = 0;
        counts["returned_events"] = 0;
        counts["months"] = static_cast<std::uint64_t>(result.at("months").size());
        counts["details"] = 0;
        counts["detail_errors"] = 0;
        counts["returned_shareholders"] = 0;
        result["counts"] = std::move(counts);
        result["semantics"] =
            "DXFJJ monthly future unlock pressure. jjsl and jjsz are hundred-million "
            "shares/yuan and are also normalized to shares/yuan. Market-cap ratios are "
            "reproduced from the client CFG; this aggregate has no per-security detail key.";
        return result;
    }

    auto master = recent_large ? fetch_recent_large(options) : fetch_master(options);
    const int selected_market = security_mode ? canonical_market_id(options.market) : 0;
    Json matched = Json::array();
    const auto needle = lower_ascii(trim(options.query));
    for (const auto& event : master.document.at("events").as_array()) {
        const auto date = text_value(event, "date");
        const auto& security = event.at("security");
        if (security_mode &&
            (static_cast<int>(security.at("market_id").as_number()) != selected_market ||
             text_value(security, "code") != options.code)) continue;
        if (!options.detail_id.empty() && text_value(event, "detail_id") != options.detail_id)
            continue;
        if (!options.start_date.empty() && date < options.start_date) continue;
        if (!options.end_date.empty() && date > options.end_date) continue;
        if (!event_label_matches(event, "progress", "progresses", options.progress))
            continue;
        if (!event_label_matches(event, "reason", "reasons", options.reason))
            continue;
        if (!needle.empty() && !json_contains(event, needle)) continue;
        matched.push_back(event);
    }
    const auto matched_summary = summarize_events(matched, 0);
    Json events = Json::array();
    for (const auto& event : matched.as_array()) {
        if (static_cast<int>(events.size()) >= options.limit) break;
        events.push_back(event);
    }

    const bool detail_mode = !recent_large && (options.include_details || security_mode ||
                             !options.detail_id.empty());
    Json details = Json::array(), errors = Json::array(), detail_sources = Json::array();
    bool any_detail_refreshed = false;
    int greatest_detail_age = 0;
    if (detail_mode) {
        for (const auto& event : events.as_array()) {
            const auto detail_id = text_value(event, "detail_id");
            const auto resource = "dbljj/" + detail_id + ".jsn";
            try {
                auto fetched = fetch_detail(resource, options);
                auto shareholders = normalize_unlock_shareholder_rows(
                    fetched.document.at("rows"), securities_);
                double detail_shares = 0.0;
                for (const auto& item : shareholders.as_array())
                    detail_shares += number_value(item, "unlock_shares").value_or(0.0);
                if (static_cast<int>(shareholders.size()) > options.detail_limit)
                    shareholders.as_array().resize(static_cast<std::size_t>(options.detail_limit));
                Json detail = Json::object();
                detail["detail_id"] = detail_id;
                detail["resource"] = resource;
                detail["shareholders"] = std::move(shareholders);
                detail["shareholder_count"] = fetched.document.at("row_count");
                detail["returned_shareholders"] =
                    static_cast<std::uint64_t>(detail.at("shareholders").size());
                detail["detail_unlock_shares"] = detail_shares;
                detail["master_unlock_shares"] = event.at("unlock_shares");
                detail["share_difference"] =
                    detail_shares - event.at("unlock_shares").as_number();
                details.push_back(std::move(detail));
                detail_sources.push_back(source_summary(fetched.document));
                any_detail_refreshed = any_detail_refreshed || fetched.refreshed;
                greatest_detail_age = std::max(greatest_detail_age, fetched.age_seconds);
            } catch (const std::exception& error) {
                Json failure = Json::object();
                failure["detail_id"] = detail_id;
                failure["resource"] = resource;
                failure["message"] = error.what();
                errors.push_back(std::move(failure));
            }
        }
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-unlocks-native-v1";
    result["generated_at"] = now_text();
    result["view"] = view;
    result["mode"] = recent_large ? "recent-large" : security_mode ? "security" :
        !options.detail_id.empty() ? "event" : detail_mode ? "detail" : "master";
    result["events"] = std::move(events);
    result["selected_event"] = result.at("events").size()
        ? result.at("events").as_array().front() : Json(nullptr);
    result["selected_security"] = security_mode
        ? security_document(selected_market, options.code, securities_) : Json(nullptr);
    result["details"] = std::move(details);
    result["detail_errors"] = std::move(errors);
    result["summary"] = matched_summary;
    result["catalog_summary"] = master.document.at("summary");
    Json filters = Json::object();
    filters["query"] = options.query;
    filters["start_date"] = options.start_date;
    filters["end_date"] = options.end_date;
    filters["progress"] = options.progress;
    filters["reason"] = options.reason;
    result["filters"] = std::move(filters);
    Json sources = Json::array();
    sources.push_back(master.document.at("source"));
    for (const auto& source : detail_sources.as_array()) sources.push_back(source);
    const auto upstream_health = jsn_sources_health(sources);
    const bool stale = upstream_health.at("stale").as_bool();
    result["availability"] = stale ? "stale-cache" :
        result.at("detail_errors").size() ? "partial" : "live";
    result["sources"] = std::move(sources);
    Json cache = Json::object();
    cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
    cache["detail_refreshed"] = any_detail_refreshed;
    cache["detail_age_seconds"] = greatest_detail_age;
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    counts["catalog_events"] = master.document.at("summary").at("events");
    counts["matched_events"] = matched_summary.at("events");
    counts["returned_events"] = static_cast<std::uint64_t>(result.at("events").size());
    counts["details"] = static_cast<std::uint64_t>(result.at("details").size());
    counts["detail_errors"] = static_cast<std::uint64_t>(result.at("detail_errors").size());
    std::uint64_t returned_shareholders = 0;
    for (const auto& detail : result.at("details").as_array())
        returned_shareholders += static_cast<std::uint64_t>(
            detail.at("returned_shareholders").as_number());
    counts["returned_shareholders"] = returned_shareholders;
    result["counts"] = std::move(counts);
    result["semantics"] = recent_large
        ? "The recent-large view is the client JQGZ rolling window for recent high-ratio unlocks; it can contain both completed and near-future dates. jjgzb is a ratio and unlock_to_total_pct is its percentage-point projection. It is independent from the DBLJJ event calendar and has no shareholder-detail key."
        : "The calendar view groups DBLJJ lots by date plus security. Per-shareholder rows are fetched only from the matching dbljj detail resource.";
    return result;
}

}  // namespace tdx
