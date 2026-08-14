#include "disclosures_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <algorithm>
#include <set>

namespace tdx {
using namespace disclosure_detail;

DisclosureService::DisclosureService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

Json DisclosureService::fetch_master(const DisclosureQuery& options,
                                     bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    const auto sources = fetch_jsn_resources_rows(
        disclosure_resource_names(), "bi", options.timeout_ms);
    Json rows = Json::array();
    const auto append = [&rows](Json normalized) {
        for (auto& row : normalized.as_array()) rows.push_back(std::move(row));
    };
    for (const auto& resource : disclosure_resources())
        append(resource.normalize(
            document_for(sources, resource.resource).at("rows"), securities_));
    Json source_rows = Json::array();
    for (const auto& source : sources.as_array())
        source_rows.push_back(source_summary(source));
    Json master = Json::object();
    master["rows"] = std::move(rows);
    master["sources"] = std::move(source_rows);
    cache_ = master;
    cache_time_ = now;
    refreshed = true;
    age_seconds = 0;
    return master;
}

Json DisclosureService::query(const DisclosureQuery& options) {
    const std::set<std::string> views{
        "all", "schedule", "express", "recent", "announcement"};
    const std::set<std::string> statuses{
        "all", "scheduled", "rescheduled", "disclosed"};
    if (!views.count(options.view))
        throw Error("view must be all, schedule, express, or recent");
    if (!statuses.count(options.status))
        throw Error("status must be all, scheduled, rescheduled, or disclosed");
    if (!options.code.empty() && !digits(options.code, 5) && !digits(options.code, 6))
        throw Error("code must contain five or six digits");
    if (!options.code.empty() && options.market.empty())
        throw Error("market is required when code is provided");
    if (options.backfill_announcements &&
        (options.market.empty() || !digits(options.code, 6)))
        throw Error("announcement backfill requires --market and a six-digit --code");
    if (options.backfill_announcements && options.view != "all" &&
        options.view != "announcement")
        throw Error("announcement backfill requires view all or announcement");
    if (!options.report_period.empty() && !digits(options.report_period, 8))
        throw Error("report_period must use YYYYMMDD");
    if (options.limit < 1 || options.limit > 20000)
        throw Error("limit must be in 1..20000");

    bool refreshed = false;
    int age_seconds = 0;
    auto master = fetch_master(options, refreshed, age_seconds);
    bool announcement_refreshed = false;
    int announcement_age_seconds = 0;
    if (options.backfill_announcements) {
        const int id = mainland_market_id(options.market);
        const auto key = std::to_string(id) + "|" + options.code;
        const auto now = std::time(nullptr);
        auto found = announcement_cache_.find(key);
        announcement_age_seconds = found == announcement_cache_.end() ? 0
            : static_cast<int>(std::max<std::time_t>(0, now - found->second.second));
        if (options.refresh || found == announcement_cache_.end() ||
            announcement_age_seconds >= options.cache_ttl_seconds) {
            announcement_cache_[key] = {
                fetch_disclosure_announcement_reports(
                    market_name(id), options.code, options.timeout_ms, securities_), now};
            found = announcement_cache_.find(key);
            announcement_age_seconds = 0;
            announcement_refreshed = true;
        }
        for (const auto& row : found->second.first.at("rows").as_array())
            master["rows"].push_back(row);
        Json source = found->second.first.at("source");
        source["kind"] = "announcement-report";
        source["summary"] = found->second.first.at("summary");
        master["sources"].push_back(std::move(source));
    }
    const auto folded = lower_ascii(trim(options.query));
    Json rows = Json::array();
    std::map<std::string, std::uint64_t> kind_counts, status_counts;
    for (const auto& row : master.at("rows").as_array()) {
        ++kind_counts[text_value(row, "kind")];
        ++status_counts[text_value(row, "status")];
        const auto row_kind = text_value(row, "kind");
        if (options.view != "all" &&
            !(options.view == "announcement" && row_kind == "announcement-report") &&
            row_kind != options.view) continue;
        if (options.status != "all" && text_value(row, "status") != options.status) continue;
        const auto& security = row.at("security");
        if (!market_matches(security, options.market)) continue;
        if (!options.code.empty() && text_value(security, "code") != options.code) continue;
        if (!options.report_period.empty() &&
            text_value(row, "report_period") != options.report_period) continue;
        const auto date = primary_date(row);
        if (!options.date_from.empty() && date < options.date_from) continue;
        if (!options.date_to.empty() && date > options.date_to) continue;
        if (!folded.empty()) {
            const auto haystack = lower_ascii(
                text_value(security, "code") + " " + text_value(security, "name") +
                " " + text_value(row, "industry") + " " +
                text_value(row, "listing_board"));
            if (haystack.find(folded) == std::string::npos) continue;
        }
        if (static_cast<int>(rows.size()) < options.limit) rows.push_back(row);
    }
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [](const Json& left, const Json& right) {
            return primary_date(left) > primary_date(right);
        });

    Json summary = Json::object();
    Json kinds = Json::object(), statuses_json = Json::object();
    for (const auto& [kind, count] : kind_counts) kinds[kind] = count;
    for (const auto& [status, count] : status_counts) statuses_json[status] = count;
    summary["all_rows"] = static_cast<std::uint64_t>(master.at("rows").size());
    summary["returned_rows"] = static_cast<std::uint64_t>(rows.size());
    summary["by_kind"] = std::move(kinds);
    summary["by_status"] = std::move(statuses_json);

    Json result = Json::object();
    result["schema"] = "tdx-market-disclosures-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["status"] = options.status;
    result["rows"] = std::move(rows);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["availability_semantics"] =
        "available_from is populated only by an actual disclosure/announcement date";
    result["history_limit"] =
        "schedule is the current reporting period and recent disclosures are a rolling one-month table";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    if (options.backfill_announcements) {
        cache["announcement_refreshed"] = announcement_refreshed;
        cache["announcement_age_seconds"] = announcement_age_seconds;
    }
    result["cache"] = std::move(cache);
    if (options.backfill_announcements) {
        result["announcement_backfill"] = true;
        result["history_limit"] =
            "announcement backfill is the latest one year, at most 300 announcements";
    }
    return result;
}

}  // namespace tdx
