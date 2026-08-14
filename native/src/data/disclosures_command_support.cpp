#include "disclosures_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

namespace tdx::disclosure_detail {
namespace fs = std::filesystem;

fs::path resolve_disclosure_archive_path(const fs::path& root,
                                         const std::string& archive_path_text) {
    return archive_path_text.empty()
        ? default_disclosure_archive_path(root) : native_path(archive_path_text);
}

fs::path resolve_disclosure_state_path(const fs::path& root,
                                       const std::string& state_path_text) {
    return state_path_text.empty()
        ? root / "T0002" / "tdx-tool" /
            "disclosure-announcement-backfill-state.json"
        : native_path(state_path_text);
}

std::vector<std::string> collect_disclosure_audit_periods(
    const DisclosureCommandOptions& options) {
    auto periods = options.audit_period_values;
    if (!options.query.report_period.empty())
        periods.push_back(options.query.report_period);
    for (const auto& value : split(options.audit_periods_text, ','))
        if (!trim(value).empty()) periods.push_back(trim(value));
    return periods;
}

Json load_disclosure_listing_dates(const fs::path& root,
                                   const std::string& listing_dates_path_text) {
    const auto listing_dates_path = listing_dates_path_text.empty()
        ? root / "T0002" / "hq_cache" / "base.dbf"
        : native_path(listing_dates_path_text);
    if (!fs::is_regular_file(listing_dates_path)) {
        if (!listing_dates_path_text.empty())
            throw Error("listing-date DBF is missing: " +
                        path_utf8(listing_dates_path));
        return Json();
    }
    auto document = parse_disclosure_listing_dates_dbf(
        read_bytes(listing_dates_path));
    document["source_path"] = path_utf8(listing_dates_path);
    return document;
}

Json load_disclosure_backfill_state(const fs::path& state_path,
                                    const fs::path& archive_path) {
    Json state;
    if (fs::is_regular_file(state_path))
        state = Json::parse(read_text_utf8(state_path));
    else {
        state = Json::object();
        state["schema"] = "tdx-disclosure-announcement-backfill-state-v1";
        state["created_at"] = now_text();
        state["entries"] = Json::array();
    }
    const auto archive_identity = normalized_absolute_path(archive_path);
    const auto previous_archive = text_value(state, "archive_path");
    if (!previous_archive.empty() &&
        lower_ascii(previous_archive) != archive_identity)
        throw Error("backfill state belongs to a different archive path");
    state["archive_path"] = archive_identity;
    return state;
}

DisclosureAnnouncementFetcher make_disclosure_fetcher(
    const SecurityDirectory& securities, const std::string& label,
    std::size_t& request_number) {
    return [&securities, label, &request_number](
               const std::string& market, const std::string& code,
               int timeout_ms) {
        ++request_number;
        std::cout << '[' << label << ' ' << request_number << "] "
                  << market_prefix(mainland_market_id(market)) << code
                  << " fetch\n";
        return fetch_disclosure_announcement_reports(
            market, code, timeout_ms, securities);
    };
}

DisclosureBackfillCheckpoint make_disclosure_checkpoint(
    const fs::path& archive_path, const fs::path& state_path) {
    return [archive_path, state_path](const Json& archive_document,
                                      const Json& state_document,
                                      bool archive_changed) {
        if (archive_changed)
            atomic_write_text(archive_path, archive_document.dump(2) + "\n");
        atomic_write_text(state_path, state_document.dump(2) + "\n");
    };
}

Json disclosure_backfill_options_document(
    const DisclosureBackfillBatchOptions& batch, int max_securities) {
    Json options = Json::object();
    options["delay_ms"] = batch.delay_ms;
    options["retry_count"] = batch.retry_count;
    options["retry_delay_ms"] = batch.retry_delay_ms;
    options["refresh_completed"] = batch.refresh_completed;
    options["maximum_securities"] = max_securities;
    return options;
}

}  // namespace tdx::disclosure_detail
