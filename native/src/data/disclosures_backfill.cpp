#include "disclosures_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/security_directory.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <thread>

namespace tdx {
using namespace disclosure_detail;

DisclosureBackfillBatchResult run_disclosure_announcement_backfill_batch(
    const std::vector<DisclosureBackfillSecurity>& requested,
    const Json* existing_archive,
    const Json* existing_state,
    const DisclosureBackfillBatchOptions& options,
    const DisclosureAnnouncementFetcher& fetcher,
    const DisclosureBackfillCheckpoint& checkpoint) {
    if (requested.empty()) throw Error("announcement backfill batch is empty");
    if (!fetcher) throw Error("announcement backfill fetcher is required");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000 ||
        options.delay_ms < 0 || options.delay_ms > 60000 ||
        options.retry_count < 0 || options.retry_count > 10 ||
        options.retry_delay_ms < 0 || options.retry_delay_ms > 60000 ||
        options.completed_ttl_seconds < -1 ||
        options.completed_ttl_seconds > 31 * 24 * 60 * 60)
        throw Error("announcement backfill batch options are outside the safe range");

    Json empty_observation = Json::object();
    empty_observation["rows"] = Json::array();
    Json archive = existing_archive ? *existing_archive
        : merge_disclosure_archive_document(empty_observation, nullptr);
    Json state;
    std::map<std::string, Json> entries;
    if (existing_state) {
        if (!existing_state->is_object() ||
            text_value(*existing_state, "schema") !=
                "tdx-disclosure-announcement-backfill-state-v1" ||
            !existing_state->as_object().count("entries") ||
            !existing_state->at("entries").is_array())
            throw Error("existing announcement backfill state has an unsupported schema");
        state = *existing_state;
        for (const auto& entry : state.at("entries").as_array()) {
            const auto id = text_value(entry, "security_id");
            if (!id.empty()) entries[id] = entry;
        }
    } else {
        state = Json::object();
        state["schema"] = "tdx-disclosure-announcement-backfill-state-v1";
        state["created_at"] = now_text();
        state["entries"] = Json::array();
    }

    std::map<std::string, DisclosureBackfillSecurity> universe;
    for (const auto& security : requested) {
        const int id = mainland_market_id(security.market);
        if (!digits(security.code, 6))
            throw Error("announcement backfill security code must contain six digits");
        universe[market_prefix(id) + security.code] = DisclosureBackfillSecurity{
            market_name(id), security.code, security.name};
    }
    for (const auto& [security_id, security] : universe) {
        auto found = entries.find(security_id);
        if (found == entries.end()) {
            Json entry = Json::object();
            entry["market"] = security.market;
            entry["code"] = security.code;
            entry["security_id"] = security_id;
            entry["name"] = security.name;
            entry["status"] = "pending";
            entry["attempts"] = 0;
            entry["report_count"] = 0;
            entry["error"] = Json(nullptr);
            entries.emplace(security_id, std::move(entry));
        } else if (!security.name.empty()) {
            found->second["name"] = security.name;
        }
    }

    const auto sync_state = [&]() {
        Json values = Json::array();
        std::uint64_t completed = 0, failed = 0, running = 0, pending = 0;
        for (const auto& [key, entry] : entries) {
            (void)key;
            const auto status = text_value(entry, "status");
            if (status == "completed") ++completed;
            else if (status == "failed") ++failed;
            else if (status == "running") ++running;
            else ++pending;
            values.push_back(entry);
        }
        state["entries"] = std::move(values);
        state["updated_at"] = now_text();
        Json summary = Json::object();
        summary["entry_count"] = static_cast<std::uint64_t>(entries.size());
        summary["completed"] = completed;
        summary["failed"] = failed;
        summary["running"] = running;
        summary["pending"] = pending;
        state["summary"] = std::move(summary);
    };
    const auto save = [&](bool archive_changed = false) {
        sync_state();
        if (checkpoint) checkpoint(archive, state, archive_changed);
    };

    std::uint64_t skipped = 0, succeeded = 0, failed = 0, requests = 0;
    bool prior_request = false;
    for (const auto& [security_id, security] : universe) {
        auto& entry = entries.at(security_id);
        bool completed_is_fresh = text_value(entry, "status") == "completed";
        if (completed_is_fresh && options.completed_ttl_seconds >= 0) {
            completed_is_fresh = false;
            const auto found = entry.as_object().find("completed_unix");
            if (found != entry.as_object().end() && found->second.is_number()) {
                const auto completed_unix = static_cast<std::time_t>(
                    found->second.as_number());
                const auto age = std::max<std::time_t>(
                    0, std::time(nullptr) - completed_unix);
                completed_is_fresh = age < options.completed_ttl_seconds;
            }
        }
        if (!options.refresh_completed && completed_is_fresh) {
            ++skipped;
            continue;
        }
        bool completed = false;
        for (int attempt = 0; attempt <= options.retry_count; ++attempt) {
            if (attempt > 0 && options.retry_delay_ms)
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    std::min(60000, options.retry_delay_ms * attempt)));
            else if (prior_request && options.delay_ms)
                std::this_thread::sleep_for(std::chrono::milliseconds(options.delay_ms));
            prior_request = true;
            ++requests;
            const auto attempts = entry.as_object().count("attempts") &&
                                  entry.at("attempts").is_number()
                ? static_cast<std::uint64_t>(entry.at("attempts").as_number()) : 0;
            entry["attempts"] = attempts + 1;
            entry["status"] = "running";
            entry["last_attempt_at"] = now_text();
            entry["last_attempt_unix"] = static_cast<std::int64_t>(std::time(nullptr));
            if (!entry.as_object().count("first_started_at"))
                entry["first_started_at"] = entry.at("last_attempt_at");
            entry["error"] = Json(nullptr);
            save();
            try {
                const auto document = fetcher(
                    security.market, security.code, options.timeout_ms);
                archive = merge_disclosure_archive_document(
                    document, &archive, text_value(document, "generated_at"));
                entry["status"] = "completed";
                entry["completed_at"] = now_text();
                entry["completed_unix"] = static_cast<std::int64_t>(std::time(nullptr));
                entry["report_count"] = document.is_object() &&
                    document.as_object().count("rows") && document.at("rows").is_array()
                        ? static_cast<std::uint64_t>(document.at("rows").size()) : 0;
                entry["error"] = Json(nullptr);
                ++succeeded;
                completed = true;
                save(true);
                break;
            } catch (const std::exception& error) {
                entry["error"] = error.what();
                entry["status"] = attempt == options.retry_count ? "failed" : "running";
                save();
                if (attempt == options.retry_count) ++failed;
            }
        }
        (void)completed;
    }
    sync_state();
    Json summary = Json::object();
    summary["requested"] = static_cast<std::uint64_t>(universe.size());
    summary["skipped_completed"] = skipped;
    summary["succeeded"] = succeeded;
    summary["failed"] = failed;
    summary["network_requests"] = requests;
    summary["archive"] = archive.at("summary");
    return DisclosureBackfillBatchResult{
        std::move(archive), std::move(state), std::move(summary)};
}

Json audit_disclosure_coverage(
    const std::vector<DisclosureBackfillSecurity>& requested,
    const Json& archive,
    std::vector<std::string> report_periods,
    std::size_t latest_period_count,
    std::string as_of_date,
    const Json* listing_dates) {
    if (requested.empty()) throw Error("disclosure coverage universe is empty");
    if (latest_period_count < 1 || latest_period_count > 20)
        throw Error("latest disclosure coverage period count must be in 1..20");
    if (!archive.is_object() ||
        text_value(archive, "schema") != "tdx-disclosure-availability-archive-v1" ||
        !archive.as_object().count("entries") || !archive.at("entries").is_array())
        throw Error("disclosure coverage audit needs a supported availability archive");
    as_of_date = trim(std::move(as_of_date));
    if (as_of_date.empty()) as_of_date = today_text();
    if (!digits(as_of_date, 8))
        throw Error("disclosure coverage as-of date must use YYYYMMDD");

    std::map<std::string, std::string> listing_date_index;
    if (listing_dates) {
        if (!listing_dates->is_object() ||
            text_value(*listing_dates, "schema") !=
                "tdx-listing-dates-dbf-native-v1" ||
            !listing_dates->as_object().count("records") ||
            !listing_dates->at("records").is_array())
            throw Error("disclosure coverage listing dates have an unsupported schema");
        for (const auto& record : listing_dates->at("records").as_array()) {
            const auto security_id = text_value(record, "security_id");
            const auto listing_date = text_value(record, "listing_date");
            if (!security_id.empty() && digits(listing_date, 8))
                listing_date_index[security_id] = listing_date;
        }
    }

    const auto valid_period = [](const std::string& value) {
        if (!digits(value, 8)) return false;
        const auto suffix = value.substr(4);
        return suffix == "0331" || suffix == "0630" || suffix == "0930" ||
               suffix == "1231";
    };
    std::set<std::string> archive_periods;
    std::map<std::string, const Json*> archive_entries;
    std::map<std::string, std::string> archive_names;
    for (const auto& entry : archive.at("entries").as_array()) {
        if (!entry.is_object()) continue;
        const auto security_id = text_value(entry, "security_id");
        const auto period = text_value(entry, "report_period");
        if (security_id.empty() || !valid_period(period)) continue;
        archive_periods.insert(period);
        archive_entries[security_id + "|" + period] = &entry;
        const auto name = text_value(entry, "name");
        if (!name.empty()) archive_names[security_id] = name;
    }

    const bool explicit_periods = !report_periods.empty();
    std::set<std::string, std::greater<>> selected_periods;
    if (explicit_periods) {
        for (auto& period : report_periods) {
            period = trim(std::move(period));
            if (!valid_period(period))
                throw Error("coverage report period must be a quarter end in YYYYMMDD: " +
                            period);
            selected_periods.insert(period);
        }
    } else {
        for (auto found = archive_periods.rbegin();
             found != archive_periods.rend() &&
             selected_periods.size() < latest_period_count; ++found)
            selected_periods.insert(*found);
    }
    if (selected_periods.empty())
        throw Error("disclosure archive contains no quarterly report periods to audit");

    std::map<std::string, DisclosureBackfillSecurity> universe;
    for (const auto& security : requested) {
        const int id = mainland_market_id(security.market);
        if (!digits(security.code, 6))
            throw Error("disclosure coverage security code must contain six digits");
        const auto security_id = market_prefix(id) + security.code;
        auto name = security.name;
        if (name.empty() && archive_names.count(security_id))
            name = archive_names.at(security_id);
        universe[security_id] = DisclosureBackfillSecurity{
            market_name(id), security.code, std::move(name)};
    }

    Json period_values = Json::array();
    for (const auto& period : selected_periods) period_values.push_back(period);
    Json coverage = Json::array();
    Json queue = Json::array();
    std::map<std::string, std::uint64_t> status_counts;
    std::uint64_t fully_covered = 0, covered_pairs = 0, pending_pairs = 0,
                  actionable_gap_pairs = 0, applicable_securities = 0,
                  not_applicable_securities = 0, required_pairs = 0,
                  pre_listing_pairs = 0, listing_date_matches = 0;
    for (const auto& [security_id, security] : universe) {
        const int market_id = mainland_market_id(security.market);
        const auto category = classify_security_directory_record(
            market_id, security.code);
        const bool applicable = category == "a_share";
        Json item = Json::object();
        item["market"] = security.market;
        item["code"] = security.code;
        item["security_id"] = security_id;
        item["name"] = security.name;
        item["category"] = category;
        item["applicable"] = applicable;
        const auto listing_found = listing_date_index.find(security_id);
        const auto listing_date = listing_found == listing_date_index.end()
            ? std::string{} : listing_found->second;
        item["listing_date"] = listing_date.empty()
            ? Json(nullptr) : Json(listing_date);
        item["listing_date_source"] = listing_date.empty()
            ? Json(nullptr) : Json("T0002/hq_cache/base.dbf#SSDATE");
        if (!listing_date.empty()) ++listing_date_matches;
        Json periods = Json::array();
        if (!applicable) {
            ++not_applicable_securities;
            for (const auto& period : selected_periods) {
                Json detail = Json::object();
                detail["report_period"] = period;
                detail["report_available_from"] = Json(nullptr);
                detail["express_available_from"] = Json(nullptr);
                detail["scheduled_disclosure_date"] = Json(nullptr);
                detail["first_scheduled_date"] = Json(nullptr);
                detail["schedule_status"] = Json(nullptr);
                detail["source_kinds"] = Json::array();
                detail["status"] = "not_applicable";
                detail["actionable_backfill"] = false;
                periods.push_back(std::move(detail));
                ++status_counts["not_applicable"];
            }
            item["periods"] = std::move(periods);
            item["required_periods"] = 0;
            item["pre_listing_periods"] = 0;
            item["covered_periods"] = 0;
            item["gap_periods"] = 0;
            item["pending_periods"] = 0;
            item["actionable_gap_periods"] = 0;
            item["fully_covered"] = Json(nullptr);
            coverage.push_back(std::move(item));
            continue;
        }
        ++applicable_securities;
        Json missing_periods = Json::array();
        Json reasons = Json::array();
        std::uint64_t security_covered = 0, security_pending = 0,
                      security_actionable = 0, security_required = 0,
                      security_pre_listing = 0;
        for (const auto& period : selected_periods) {
            Json detail = Json::object();
            detail["report_period"] = period;
            if (!listing_date.empty() && period < listing_date) {
                detail["report_available_from"] = Json(nullptr);
                detail["express_available_from"] = Json(nullptr);
                detail["scheduled_disclosure_date"] = Json(nullptr);
                detail["first_scheduled_date"] = Json(nullptr);
                detail["schedule_status"] = Json(nullptr);
                detail["source_kinds"] = Json::array();
                detail["status"] = "pre_listing";
                detail["actionable_backfill"] = false;
                periods.push_back(std::move(detail));
                ++status_counts["pre_listing"];
                ++pre_listing_pairs;
                ++security_pre_listing;
                continue;
            }
            ++required_pairs;
            ++security_required;
            const auto found = archive_entries.find(security_id + "|" + period);
            std::string status;
            if (found == archive_entries.end()) {
                status = "missing_entry";
                detail["report_available_from"] = Json(nullptr);
                detail["express_available_from"] = Json(nullptr);
                detail["scheduled_disclosure_date"] = Json(nullptr);
                detail["first_scheduled_date"] = Json(nullptr);
                detail["schedule_status"] = Json(nullptr);
                detail["source_kinds"] = Json::array();
            } else {
                const auto& entry = *found->second;
                const auto report_date = text_value(entry, "report_available_from");
                const auto express_date = text_value(entry, "express_available_from");
                const auto scheduled_date = text_value(
                    entry, "scheduled_disclosure_date");
                detail["report_available_from"] = report_date.empty()
                    ? Json(nullptr) : Json(report_date);
                detail["express_available_from"] = express_date.empty()
                    ? Json(nullptr) : Json(express_date);
                detail["scheduled_disclosure_date"] = scheduled_date.empty()
                    ? Json(nullptr) : Json(scheduled_date);
                const auto first_scheduled = text_value(entry, "first_scheduled_date");
                detail["first_scheduled_date"] = first_scheduled.empty()
                    ? Json(nullptr) : Json(first_scheduled);
                const auto schedule_status = text_value(entry, "schedule_status");
                detail["schedule_status"] = schedule_status.empty()
                    ? Json(nullptr) : Json(schedule_status);
                detail["source_kinds"] = entry.as_object().count("source_kinds") &&
                    entry.at("source_kinds").is_array()
                        ? entry.at("source_kinds") : Json::array();
                if (!report_date.empty() && !digits(report_date, 8))
                    status = "invalid_report_date";
                else if (!report_date.empty())
                    status = "covered";
                else if (digits(scheduled_date, 8) && scheduled_date > as_of_date &&
                         digits(express_date, 8))
                    status = "express_only_scheduled_future";
                else if (digits(scheduled_date, 8) && scheduled_date > as_of_date)
                    status = "scheduled_future";
                else if (digits(express_date, 8))
                    status = "express_only";
                else if (digits(scheduled_date, 8) && scheduled_date <= as_of_date)
                    status = "overdue_without_report";
                else
                    status = "observed_without_report";
            }
            detail["status"] = status;
            ++status_counts[status];
            const bool pending = status == "scheduled_future" ||
                status == "express_only_scheduled_future";
            const bool actionable = status != "covered" && !pending;
            detail["actionable_backfill"] = actionable;
            if (status == "covered") {
                ++covered_pairs;
                ++security_covered;
            } else if (pending) {
                ++pending_pairs;
                ++security_pending;
            } else {
                ++actionable_gap_pairs;
                ++security_actionable;
                missing_periods.push_back(period);
                Json reason = Json::object();
                reason["report_period"] = period;
                reason["status"] = status;
                reasons.push_back(std::move(reason));
            }
            periods.push_back(std::move(detail));
        }
        item["periods"] = std::move(periods);
        item["required_periods"] = security_required;
        item["pre_listing_periods"] = security_pre_listing;
        item["covered_periods"] = security_covered;
        item["gap_periods"] = security_required - security_covered;
        item["pending_periods"] = security_pending;
        item["actionable_gap_periods"] = security_actionable;
        item["fully_covered"] = security_required
            ? Json(security_covered == security_required) : Json(nullptr);
        if (security_required && security_covered == security_required) {
            ++fully_covered;
        } else if (security_actionable) {
            Json queued = Json::object();
            queued["market"] = security.market;
            queued["code"] = security.code;
            queued["security_id"] = security_id;
            queued["name"] = security.name;
            queued["missing_periods"] = std::move(missing_periods);
            queued["reasons"] = std::move(reasons);
            queue.push_back(std::move(queued));
        }
        coverage.push_back(std::move(item));
    }

    const auto selected_pair_count = static_cast<std::uint64_t>(
        universe.size() * selected_periods.size());
    const auto a_share_pair_count = static_cast<std::uint64_t>(
        applicable_securities * selected_periods.size());
    Json by_status = Json::object();
    for (const auto& [status, count] : status_counts) by_status[status] = count;
    Json summary = Json::object();
    summary["security_count"] = static_cast<std::uint64_t>(universe.size());
    summary["applicable_security_count"] = applicable_securities;
    summary["not_applicable_security_count"] = not_applicable_securities;
    summary["report_period_count"] =
        static_cast<std::uint64_t>(selected_periods.size());
    summary["selected_security_period_pairs"] = selected_pair_count;
    summary["a_share_security_period_pairs"] = a_share_pair_count;
    summary["security_period_pairs"] = required_pairs;
    summary["pre_listing_pairs"] = pre_listing_pairs;
    summary["not_applicable_pairs"] = selected_pair_count - a_share_pair_count;
    summary["listing_date_matches"] = listing_date_matches;
    summary["covered_pairs"] = covered_pairs;
    summary["gap_pairs"] = required_pairs - covered_pairs;
    summary["pending_pairs"] = pending_pairs;
    summary["actionable_gap_pairs"] = actionable_gap_pairs;
    summary["fully_covered_securities"] = fully_covered;
    summary["securities_needing_backfill"] =
        static_cast<std::uint64_t>(queue.size());
    summary["coverage_ratio"] = required_pairs
        ? static_cast<double>(covered_pairs) / static_cast<double>(required_pairs) : 0.0;
    summary["by_status"] = std::move(by_status);

    Json result = Json::object();
    result["schema"] = "tdx-disclosure-coverage-audit-native-v1";
    result["generated_at"] = now_text();
    result["as_of_date"] = as_of_date;
    result["period_basis"] = explicit_periods
        ? "explicit" : "latest_quarter_ends_represented_in_archive";
    result["report_periods"] = std::move(period_values);
    result["summary"] = std::move(summary);
    result["coverage"] = std::move(coverage);
    result["items"] = std::move(queue);
    result["queue_semantics"] =
        "top-level items contain only actionable gaps and can be passed directly to market disclosures --backfill-input";
    if (listing_dates) {
        Json source = Json::object();
        source["source_path"] = listing_dates->as_object().count("source_path")
            ? listing_dates->at("source_path") : Json(nullptr);
        source["source_format"] = listing_dates->at("source_format");
        source["dbf_updated_date"] = listing_dates->at("dbf_updated_date");
        source["available_dates"] = listing_dates->at("summary").at(
            "listing_date_count");
        source["matched_securities"] = listing_date_matches;
        result["listing_date_source"] = std::move(source);
        result["listing_date_boundary"] =
            "quarter ends before local base.dbf SSDATE are pre_listing and excluded from required coverage";
    } else {
        result["listing_date_source"] = Json(nullptr);
        result["listing_date_boundary"] =
            "listing-date DBF was unavailable; missing periods can include pre-listing quarters";
    }
    result["history_limit"] =
        "announcement backfill remains limited to the latest one year and at most 300 announcements per security";
    return result;
}

}  // namespace tdx
