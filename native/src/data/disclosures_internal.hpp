#pragma once

#include "tdx/common.hpp"
#include "tdx/disclosures.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::disclosure_detail {

inline constexpr const char* announcement_entry = "CWSearch.tzx_rcache";

using SecurityDirectory = std::map<std::pair<int, std::string>, Security>;
using DisclosureResourceNormalizer = Json (*)(
    const Json&, const SecurityDirectory&);

struct DisclosureResourceDefinition {
    const char* resource;
    const char* kind;
    bool hong_kong;
    DisclosureResourceNormalizer normalize;
};

const std::array<DisclosureResourceDefinition, 5>& disclosure_resources();
const std::vector<std::string>& disclosure_resource_names();

struct PeriodicReportType {
    const char* key;
    const char* suffix;
    const char* title_phrase;
};

const Json* field(const Json&, std::string_view);
std::string text_value(const Json&, std::string_view);
std::optional<double> number_value(const Json&, std::string_view);
Json number_or_null(const Json&, std::string_view);
bool digits(const std::string&, std::size_t);
int integer_market(const Json&);
std::string market_name(int);
std::string market_prefix(int);
Json security_document(
    const Json&, const std::map<std::pair<int, std::string>, Security>&);
std::string disclosure_status(const Json&);
Json change_dates(const Json&);
Json source_summary(const Json&);
const Json& document_for(const Json&, std::string_view);
std::string now_text();
std::string today_text();
std::filesystem::path native_path(const std::string&);
int bounded(const std::string&, std::string_view, int, int);
bool market_matches(const Json&, const std::string&);
std::string primary_date(const Json&);
int mainland_market_id(const std::string&);
std::string scalar_text(const Json&);
std::vector<std::string> tqlex_columns(const Json&);
Json tqlex_rows(const Json&);
std::string compact_announcement_date(const std::string&);
std::optional<PeriodicReportType> periodic_report_type(const std::string&);
std::string report_year(const std::string&, const std::string&);
std::optional<PeriodicReportType> periodic_report_type_for_row(const Json&);
Json direct_security_document(
    int, const std::string&,
    const std::map<std::pair<int, std::string>, Security>&);
Json announcement_evidence(const Json&);

DisclosureBackfillSecurity parse_backfill_security(
    std::string value, const SecurityDirectory& securities,
    std::string name = {});
std::vector<DisclosureBackfillSecurity> read_backfill_input(
    const std::filesystem::path& path, const SecurityDirectory& securities);
std::vector<DisclosureBackfillSecurity> block_backfill_securities(
    const BlockData& data, const std::string& query);
std::vector<DisclosureBackfillSecurity> deduplicate_backfill_securities(
    std::vector<DisclosureBackfillSecurity> values);
std::string normalized_absolute_path(const std::filesystem::path& path);

// Parsed `market disclosures` invocation.  The command front end fills this in
// from the raw argument vector; each execution mode reads what it needs.
struct DisclosureCommandOptions {
    DisclosureQuery query;
    DisclosureBackfillBatchOptions batch;
    std::filesystem::path output;
    std::string root_text;
    std::string archive_path_text;
    std::string state_path_text;
    std::string audit_periods_text;
    std::string audit_as_of;
    std::string listing_dates_path_text;
    std::vector<std::string> audit_period_values;
    std::vector<std::string> security_values;
    std::string securities_text;
    std::vector<std::string> input_values;
    std::vector<std::string> block_values;
    int latest_audit_periods{4};
    int maintenance_refresh_hours{24};
    int max_securities{500};
    bool archive{};
    bool audit_coverage{};
    bool maintain{};
    bool watchlist{};
    bool dry_run{};
    bool compact{};
    // True when any security selector was supplied, which switches the command
    // from a single master query to universe-oriented processing.
    bool batch_inputs{};
    bool universe_mode{};
};

void print_disclosure_command_help();
// Consumes every recognized option and validates mutually exclusive
// combinations.  Throws Error on conflicting or misplaced flags.
DisclosureCommandOptions parse_disclosure_command_options(Args& args);

std::filesystem::path resolve_disclosure_archive_path(
    const std::filesystem::path& root, const std::string& archive_path_text);
std::filesystem::path resolve_disclosure_state_path(
    const std::filesystem::path& root, const std::string& state_path_text);
// Merges --report-period, repeated --audit-period and comma-separated
// --audit-periods into one list.
std::vector<std::string> collect_disclosure_audit_periods(
    const DisclosureCommandOptions& options);
// Reads T0002/hq_cache/base.dbf (or the override) when present.  Returns an
// empty Json when the default file is absent; throws when an explicit
// --listing-dates-path is missing.
Json load_disclosure_listing_dates(
    const std::filesystem::path& root, const std::string& listing_dates_path_text);
// Loads the resume checkpoint or seeds a fresh one, then binds it to the
// archive so a state file cannot be reused across archives.
Json load_disclosure_backfill_state(
    const std::filesystem::path& state_path,
    const std::filesystem::path& archive_path);
// Builds the fetch/checkpoint pair shared by batch and maintenance backfill.
// `label` prefixes the per-security progress line.
DisclosureAnnouncementFetcher make_disclosure_fetcher(
    const SecurityDirectory& securities, const std::string& label,
    std::size_t& request_number);
DisclosureBackfillCheckpoint make_disclosure_checkpoint(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& state_path);
Json disclosure_backfill_options_document(
    const DisclosureBackfillBatchOptions& batch, int max_securities);

// Resolves --security/--securities/--backfill-input/--backfill-watchlist/
// --backfill-block into a deduplicated universe, enforcing --max-securities.
std::vector<DisclosureBackfillSecurity> resolve_disclosure_universe(
    const DisclosureCommandOptions& options, const BlockData& block_data,
    const std::filesystem::path& root);

int run_disclosure_coverage_audit(
    const DisclosureCommandOptions& options,
    const std::vector<DisclosureBackfillSecurity>& securities,
    const std::filesystem::path& root);
int run_disclosure_maintenance(
    const DisclosureCommandOptions& options,
    const std::vector<DisclosureBackfillSecurity>& securities,
    const std::filesystem::path& root, const BlockData& block_data,
    DisclosureService& service);
int run_disclosure_backfill_preview(
    const DisclosureCommandOptions& options,
    const std::vector<DisclosureBackfillSecurity>& securities);
int run_disclosure_backfill_batch(
    const DisclosureCommandOptions& options,
    const std::vector<DisclosureBackfillSecurity>& securities,
    const std::filesystem::path& root, const BlockData& block_data);
int run_disclosure_master_query(
    const DisclosureCommandOptions& options, const std::filesystem::path& root,
    DisclosureService& service);

}  // namespace tdx::disclosure_detail
