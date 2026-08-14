#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx {

struct DisclosureQuery {
    std::string view{"schedule"};
    std::string status{"all"};
    std::string query;
    std::string market;
    std::string code;
    std::string report_period;
    std::string date_from;
    std::string date_to;
    bool backfill_announcements{};
    bool refresh{};
    int limit{10000};
    int cache_ttl_seconds{300};
    int timeout_ms{15000};
};

struct DisclosureBackfillSecurity {
    std::string market;
    std::string code;
    std::string name;
};

struct DisclosureBackfillBatchOptions {
    int timeout_ms{15000};
    int delay_ms{250};
    int retry_count{2};
    int retry_delay_ms{1000};
    int completed_ttl_seconds{-1};
    bool refresh_completed{};
};

struct DisclosureBackfillBatchResult {
    Json archive;
    Json state;
    Json summary;
};

using DisclosureAnnouncementFetcher = std::function<Json(
    const std::string&, const std::string&, int)>;
using DisclosureBackfillCheckpoint =
    std::function<void(const Json&, const Json&, bool archive_changed)>;

Json normalize_disclosure_schedule_rows(
    const Json& rows, bool hong_kong,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_disclosure_express_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json normalize_recent_disclosure_rows(
    const Json& rows, bool hong_kong,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
// Normalize the official per-security announcement cache into full-report
// availability rows.  Only exchange-specific periodic-report type codes are
// accepted; report summaries and similarly named notices do not unlock gpcw.
Json normalize_disclosure_announcement_response(
    const Json& response, const std::string& market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json fetch_disclosure_announcement_reports(
    const std::string& market, const std::string& code, int timeout_ms = 15000,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
std::vector<DisclosureBackfillSecurity> parse_tdx_watchlist_securities(
    std::string_view text,
    const std::map<std::pair<int, std::string>, Security>& securities = {});
Json parse_disclosure_listing_dates_dbf(const Bytes& data);
DisclosureBackfillBatchResult run_disclosure_announcement_backfill_batch(
    const std::vector<DisclosureBackfillSecurity>& securities,
    const Json* existing_archive,
    const Json* existing_state,
    const DisclosureBackfillBatchOptions& options,
    const DisclosureAnnouncementFetcher& fetcher,
    const DisclosureBackfillCheckpoint& checkpoint = {});
// Audit point-in-time report-date coverage for a selected security universe.
// When report_periods is empty, the latest periods represented by the archive
// are used.  The top-level items array is directly reusable as --backfill-input.
Json audit_disclosure_coverage(
    const std::vector<DisclosureBackfillSecurity>& securities,
    const Json& archive,
    std::vector<std::string> report_periods = {},
    std::size_t latest_period_count = 4,
    std::string as_of_date = {},
    const Json* listing_dates = nullptr);

// The public disclosure resources only expose the current reporting cycle and
// roughly one month of recent reports.  Persisting each observation turns that
// rolling window into a prospective point-in-time availability history.
std::filesystem::path default_disclosure_archive_path(
    const std::filesystem::path& tdx_root);
Json merge_disclosure_archive_document(
    const Json& disclosure_document,
    const Json* existing_archive = nullptr,
    std::string observed_at = {});

class DisclosureService {
public:
    explicit DisclosureService(
        std::map<std::pair<int, std::string>, Security> securities = {});
    Json query(const DisclosureQuery& options);

private:
    Json fetch_master(const DisclosureQuery& options, bool& refreshed,
                      int& age_seconds);
    std::map<std::pair<int, std::string>, Security> securities_;
    Json cache_;
    std::time_t cache_time_{};
    std::map<std::string, std::pair<Json, std::time_t>> announcement_cache_;
};

int command_market_disclosures(const std::vector<std::string>& args);

}  // namespace tdx
