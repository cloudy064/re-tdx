#include "disclosures_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <utility>

namespace tdx::disclosure_detail {
namespace fs = std::filesystem;
namespace {

// The archive refresh always observes the whole market so the merged history
// stays complete regardless of the caller's filters.
DisclosureQuery unfiltered_refresh_query(const DisclosureQuery& query) {
    auto refresh = query;
    refresh.view = "all";
    refresh.status = "all";
    refresh.query.clear();
    refresh.market.clear();
    refresh.code.clear();
    refresh.report_period.clear();
    refresh.date_from.clear();
    refresh.date_to.clear();
    refresh.backfill_announcements = false;
    refresh.limit = 20000;
    return refresh;
}

std::vector<DisclosureBackfillSecurity> due_securities(const Json& audit) {
    std::vector<DisclosureBackfillSecurity> due;
    for (const auto& item : audit.at("items").as_array())
        due.push_back(DisclosureBackfillSecurity{
            text_value(item, "market"), text_value(item, "code"),
            text_value(item, "name")});
    return due;
}

Json empty_backfill_summary(std::size_t requested, const Json& archive_summary) {
    Json summary = Json::object();
    summary["requested"] = static_cast<std::uint64_t>(requested);
    summary["skipped_completed"] = 0;
    summary["succeeded"] = 0;
    summary["failed"] = 0;
    summary["network_requests"] = 0;
    summary["archive"] = archive_summary;
    return summary;
}

}  // namespace

int run_disclosure_maintenance(
    const DisclosureCommandOptions& options,
    const std::vector<DisclosureBackfillSecurity>& securities,
    const fs::path& root, const BlockData& block_data,
    DisclosureService& service) {
    const auto archive_path = resolve_disclosure_archive_path(
        root, options.archive_path_text);
    const auto state_path = resolve_disclosure_state_path(
        root, options.state_path_text);
    const auto audit_periods = collect_disclosure_audit_periods(options);
    Json existing_archive;
    const Json* existing_archive_pointer = nullptr;
    if (fs::is_regular_file(archive_path)) {
        existing_archive = Json::parse(read_text_utf8(archive_path));
        existing_archive_pointer = &existing_archive;
    }

    const auto refreshed_document =
        service.query(unfiltered_refresh_query(options.query));
    auto refreshed_archive = merge_disclosure_archive_document(
        refreshed_document, existing_archive_pointer,
        text_value(refreshed_document, "generated_at"));
    atomic_write_text(archive_path, refreshed_archive.dump(2) + "\n");

    const auto listing_dates = load_disclosure_listing_dates(
        root, options.listing_dates_path_text);
    const Json* listing_dates_pointer =
        listing_dates.is_null() ? nullptr : &listing_dates;
    const auto latest_periods =
        static_cast<std::size_t>(options.latest_audit_periods);
    const auto audit_before = audit_disclosure_coverage(
        securities, refreshed_archive, audit_periods, latest_periods,
        options.audit_as_of, listing_dates_pointer);
    const auto due = due_securities(audit_before);

    Json final_archive = refreshed_archive;
    auto backfill_summary =
        empty_backfill_summary(due.size(), refreshed_archive.at("summary"));
    if (!due.empty()) {
        auto state = load_disclosure_backfill_state(state_path, archive_path);
        auto batch = options.batch;
        batch.completed_ttl_seconds = options.maintenance_refresh_hours * 60 * 60;
        std::size_t request_number = 0;
        auto result = run_disclosure_announcement_backfill_batch(
            due, &refreshed_archive, &state, batch,
            make_disclosure_fetcher(block_data.securities, "maintenance request",
                                    request_number),
            make_disclosure_checkpoint(archive_path, state_path));
        final_archive = std::move(result.archive);
        backfill_summary = std::move(result.summary);
        atomic_write_text(archive_path, final_archive.dump(2) + "\n");
        atomic_write_text(state_path, result.state.dump(2) + "\n");
    }
    const auto audit_after = audit_disclosure_coverage(
        securities, final_archive, audit_periods, latest_periods,
        options.audit_as_of, listing_dates_pointer);

    Json document = Json::object();
    document["schema"] = "tdx-disclosure-maintenance-native-v1";
    document["generated_at"] = now_text();
    document["archive_path"] = path_utf8(archive_path);
    document["state_path"] = path_utf8(state_path);
    Json schedule_refresh = Json::object();
    schedule_refresh["summary"] = refreshed_document.at("summary");
    schedule_refresh["archive"] = refreshed_archive.at("summary");
    document["schedule_refresh"] = std::move(schedule_refresh);
    document["audit_before_backfill"] = audit_before.at("summary");
    document["backfill"] = std::move(backfill_summary);
    document["audit"] = audit_after;
    document["items"] = audit_after.at("items");
    auto option_document = disclosure_backfill_options_document(
        options.batch, options.max_securities);
    option_document["completed_ttl_hours"] = options.maintenance_refresh_hours;
    document["options"] = std::move(option_document);
    atomic_write_text(options.output,
                      document.dump(options.compact ? -1 : 2) + "\n");
    std::cout << "maintained " << securities.size() << " securities; "
              << audit_before.at("summary").at(
                     "securities_needing_backfill").as_number()
              << " due before, "
              << audit_after.at("summary").at(
                     "securities_needing_backfill").as_number()
              << " due after -> " << path_utf8(options.output) << '\n';
    return document.at("backfill").at("failed").as_number() > 0 ? 3 : 0;
}

}  // namespace tdx::disclosure_detail
