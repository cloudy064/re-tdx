#include "tdx/calendar.hpp"

#include "calendar_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {

using namespace calendar_detail;

CalendarService::CalendarService(
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : securities_(std::move(securities)), jsn_root_(std::move(jsn_root)) {}

Json CalendarService::fetch_master(const CalendarQuery& options, bool& refreshed,
                                   int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_ ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    Json sources = Json::array();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : kResources)
                sources.push_back(load_local_resource_rows(jsn_root_, resource));
        } catch (...) {
            sources = Json::array();
        }
    }
    if (sources.as_array().empty())
        sources = fetch_jsn_resources_rows(kResources, "bi", options.timeout_ms);
    Json document = Json::object();
    Json rows = Json::array();
    Json source_rows = Json::array();
    for (const auto& resource : kResources) {
        const auto& source = document_for(sources, resource);
        auto normalized = normalize_calendar_rows(
            resource, source.at("rows"), securities_);
        source_rows.push_back(source_summary(source, normalized.size()));
        for (auto& row : normalized.as_array()) rows.push_back(std::move(row));
    }
    std::map<std::string, Json> security_by_name;
    std::set<std::string> duplicate_security_names;
    for (const auto& [key, security] : securities_) {
        if (key.first < 0 || key.first > 2 || security.name.empty()) continue;
        const auto [found, inserted] = security_by_name.emplace(
            security.name, security_document_values(
                key.first, key.second, security.name, securities_));
        if (!inserted) duplicate_security_names.insert(security.name);
    }
    for (const auto& name : duplicate_security_names) security_by_name.erase(name);
    for (const auto& row : rows.as_array()) {
        if (!string_is(field(row, "kind"), "ipo-subscription")) continue;
        const auto company_name = text_value(row, "company_name");
        const auto* security = field(row, "security");
        if (!company_name.empty() && security && security->is_object())
            security_by_name[company_name] = *security;
    }
    for (auto& row : rows.as_array()) {
        const bool association_target =
            string_is(field(row, "kind"), "ipo-guidance") ||
            string_is(field(row, "kind"), "ipo-review");
        if (!association_target ||
            !row.at("security").is_null()) continue;
        const auto found = security_by_name.find(text_value(row, "company_name"));
        if (found == security_by_name.end()) continue;
        row["security"] = found->second;
        row["related_securities"].push_back(found->second);
    }
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(), [](const Json& a, const Json& b) {
        const auto left = date_key(a.at("date").as_string());
        const auto right = date_key(b.at("date").as_string());
        if (left != right) return left > right;
        return a.at("kind").as_string() < b.at("kind").as_string();
    });
    document["rows"] = std::move(rows);
    document["sources"] = std::move(source_rows);
    cache_ = document;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return document;
}

Json CalendarService::query(const CalendarQuery& options) {
    const auto &selected_view = view_definition(options.view);
    if (options.limit < 1 || options.limit > 10000) throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty()) throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = market_id(options.market);
        if (selected_market != 74 && !code_digits(options.code))
            throw Error("code must contain digits outside market 74/us");
        if (selected_market == 74 && trim(options.code).empty())
            throw Error("US code must not be empty");
    }
    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto folded = lower_ascii(trim(options.query));
    Json rows = Json::array();
    std::map<std::string, std::uint64_t> kind_counts;
    std::map<std::string, std::uint64_t> rights_issue_stage_counts;
    std::map<std::string, std::uint64_t> ipo_guidance_progress_counts;
    std::map<std::string, std::uint64_t> ipo_guidance_board_counts;
    std::map<std::string, std::uint64_t> ipo_guidance_region_counts;
    std::uint64_t ipo_guidance_linked_securities = 0;
    std::map<std::string, std::uint64_t> ipo_review_status_counts;
    std::map<std::string, std::uint64_t> ipo_review_board_counts;
    std::uint64_t ipo_review_linked_securities = 0;
    std::map<std::string, std::set<std::string>> us_calendar_dates;
    std::map<std::string, std::set<std::string>> us_listed_dates;
    std::map<std::string, std::set<std::string>> us_pending_dates;
    std::map<std::string, double> us_issue_amount_usd;
    std::uint64_t matched = 0;
    for (const auto& row : master.at("rows").as_array()) {
        const auto kind = row.at("kind").as_string();
        ++kind_counts[kind];
        if (kind == "rights-issue")
            ++rights_issue_stage_counts[text_value(row, "rights_issue_stage")];
        if (kind == "ipo-guidance") {
            ++ipo_guidance_progress_counts[text_value(row, "guidance_progress")];
            ++ipo_guidance_board_counts[text_value(row, "board_category")];
            ++ipo_guidance_region_counts[text_value(row, "region")];
            if (!row.at("security").is_null()) ++ipo_guidance_linked_securities;
        }
        if (kind == "ipo-review") {
            ++ipo_review_status_counts[text_value(row, "review_status")];
            ++ipo_review_board_counts[text_value(row, "board_category")];
            if (!row.at("security").is_null()) ++ipo_review_linked_securities;
        }
        if (kind.rfind("us-ipo-", 0) == 0) {
            const auto* security = field(row, "security");
            const auto code = security && security->is_object()
                ? text_value(*security, "code") : std::string{};
            const auto date = date_key(row.at("date").as_string());
            if (!code.empty()) {
                if (kind == "us-ipo-calendar") us_calendar_dates[code].insert(date);
                if (kind == "us-ipo-listed") us_listed_dates[code].insert(date);
                if (kind == "us-ipo-pending") us_pending_dates[code].insert(date);
            }
            us_issue_amount_usd[kind] +=
                number_value(row, "issue_amount_usd").value_or(0.0);
        }
        if (!view_matches(selected_view.view, kind)) continue;
        const auto date = date_key(row.at("date").as_string());
        if (!options.date_from.empty() && date < date_key(options.date_from)) continue;
        if (!options.date_to.empty() && date > date_key(options.date_to)) continue;
        if (!options.event_id.empty() && row.at("event_id").as_string() != options.event_id) continue;
        if (!options.code.empty() &&
            !row_has_security(row, selected_market, options.code)) continue;
        if (!folded.empty()) {
            std::string haystack = row.at("title").as_string() + " " + row.at("content").as_string() +
                " " + row.at("industry").as_string() + " " + row.at("event_type").as_string();
            if (!row.at("security").is_null()) haystack += " " + row.at("security").at("code").as_string() +
                " " + row.at("security").at("name").as_string();
            for (const auto& security : row.at("related_securities").as_array())
                haystack += " " + security.at("code").as_string() + " " +
                    security.at("name").as_string();
            if (lower_ascii(haystack).find(folded) == std::string::npos) continue;
        }
        ++matched;
        if (static_cast<int>(rows.size()) < options.limit) rows.push_back(row);
    }
    Json meeting_members = Json::array();
    Json related_meetings = Json::array();
    Json detail_errors = Json::array();
    std::vector<std::string> meeting_resources;
    std::vector<const Json*> meeting_rows;
    for (const auto& row : master.at("rows").as_array()) if (row.at("kind").as_string() == "meeting") {
        const auto id = row.at("event_id").as_string();
        if ((!options.event_id.empty() && id == options.event_id) || !options.code.empty()) {
            meeting_resources.push_back("cjrl/" + id + ".jsn");
            meeting_rows.push_back(&row);
        }
    }
    if (!meeting_resources.empty()) {
        try {
            const auto details = fetch_jsn_resources_rows(meeting_resources, "bi", options.timeout_ms);
            for (std::size_t index = 0; index < details.size(); ++index) {
                bool selected_related = false;
                for (const auto& raw : details.as_array()[index].at("rows").as_array()) {
                    const auto security = security_document(raw, securities_);
                    if (security.is_null()) continue;
                    Json member = Json::object();
                    member["event_id"] = meeting_rows[index]->at("event_id");
                    member["security"] = security;
                    if (!options.code.empty() && static_cast<int>(security.at("market_id").as_number()) == selected_market &&
                        security.at("code").as_string() == options.code) selected_related = true;
                    if (!options.event_id.empty()) meeting_members.push_back(std::move(member));
                }
                if (selected_related) related_meetings.push_back(*meeting_rows[index]);
            }
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["message"] = error.what();
            detail_errors.push_back(std::move(failure));
        }
    }
    Json summary = Json::object();
    for (const auto& [kind, count] : kind_counts) summary[kind] = count;
    summary["major_events"] = kind_counts["major-event"];
    summary["ipo_announcements"] = kind_counts["ipo-announcement"];
    summary["recent_ipos"] = kind_counts["recent-ipo"];
    summary["ipo_subscriptions"] = kind_counts["ipo-subscription"];
    summary["ipo_subscription_details"] = kind_counts["ipo-subscription-detail"];
    summary["ipo_companion_news"] = kind_counts["ipo-companion-news"];
    summary["ipo_guidance"] = kind_counts["ipo-guidance"];
    summary["ipo_guidance_linked_securities"] = ipo_guidance_linked_securities;
    summary["ipo_guidance_progress"] = Json::object();
    for (const auto& [progress, count] : ipo_guidance_progress_counts)
        if (!progress.empty()) summary["ipo_guidance_progress"][progress] = count;
    summary["ipo_guidance_boards"] = Json::object();
    for (const auto& [board, count] : ipo_guidance_board_counts)
        if (!board.empty()) summary["ipo_guidance_boards"][board] = count;
    summary["ipo_guidance_regions"] = Json::object();
    for (const auto& [region, count] : ipo_guidance_region_counts)
        if (!region.empty()) summary["ipo_guidance_regions"][region] = count;
    summary["ipo_reviews"] = kind_counts["ipo-review"];
    summary["ipo_review_linked_securities"] = ipo_review_linked_securities;
    summary["ipo_review_statuses"] = Json::object();
    for (const auto& [status, count] : ipo_review_status_counts)
        if (!status.empty()) summary["ipo_review_statuses"][status] = count;
    summary["ipo_review_boards"] = Json::object();
    for (const auto& [board, count] : ipo_review_board_counts)
        if (!board.empty()) summary["ipo_review_boards"][board] = count;
    summary["us_ipo_applications"] = kind_counts["us-ipo-application"];
    summary["us_ipo_calendar"] = kind_counts["us-ipo-calendar"];
    summary["us_ipo_listed"] = kind_counts["us-ipo-listed"];
    summary["us_ipo_pending"] = kind_counts["us-ipo-pending"];
    summary["us_ipo_issue_amount_usd"] = Json::object();
    for (const auto& [kind, amount] : us_issue_amount_usd)
        summary["us_ipo_issue_amount_usd"][kind] = amount;
    std::uint64_t us_listed_code_overlap = 0;
    std::uint64_t us_listed_same_date = 0;
    std::uint64_t us_listed_date_changed = 0;
    for (const auto& [code, dates] : us_listed_dates) {
        const auto found = us_calendar_dates.find(code);
        if (found == us_calendar_dates.end()) continue;
        ++us_listed_code_overlap;
        bool same = false;
        for (const auto& date : dates)
            if (found->second.count(date)) { same = true; break; }
        if (same) ++us_listed_same_date;
        else ++us_listed_date_changed;
    }
    std::uint64_t us_pending_code_overlap = 0;
    std::uint64_t us_pending_same_date = 0;
    for (const auto& [code, dates] : us_pending_dates) {
        const auto found = us_calendar_dates.find(code);
        if (found == us_calendar_dates.end()) continue;
        ++us_pending_code_overlap;
        for (const auto& date : dates)
            if (found->second.count(date)) { ++us_pending_same_date; break; }
    }
    summary["us_ipo_calendar_listed_code_overlap"] = us_listed_code_overlap;
    summary["us_ipo_calendar_listed_same_date"] = us_listed_same_date;
    summary["us_ipo_calendar_listed_date_changed"] = us_listed_date_changed;
    summary["us_ipo_calendar_pending_code_overlap"] = us_pending_code_overlap;
    summary["us_ipo_calendar_pending_same_date"] = us_pending_same_date;
    summary["board_news"] = kind_counts["star-news"] +
        kind_counts["chinext-news"] + kind_counts["neeq-news"];
    summary["futures_calendar"] = kind_counts["futures-calendar"];
    summary["rights_issue_events"] = kind_counts["rights-issue"];
    summary["rights_issue_stages"] = Json::object();
    for (const auto& [stage, count] : rights_issue_stage_counts)
        if (!stage.empty()) summary["rights_issue_stages"][stage] = count;
    summary["company_events"] = kind_counts["company"] +
        kind_counts["ipo-listing-event"] + kind_counts["ipo-issue-event"] +
        kind_counts["suspension-resumption"] + kind_counts["special-treatment"] +
        kind_counts["listing-status"] + kind_counts["rights-issue"] +
        kind_counts["additional-issuance"] +
        kind_counts["shareholder-meeting"];
    Json result = Json::object();
    result["schema"] = "tdx-market-calendar-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = options.code.empty() ? "calendar" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = matched;
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["rows"] = std::move(rows);
    result["meeting_members"] = std::move(meeting_members);
    result["related_meetings"] = std::move(related_meetings);
    result["detail_errors"] = std::move(detail_errors);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}


}  // namespace tdx
