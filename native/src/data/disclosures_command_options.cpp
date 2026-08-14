#include "disclosures_internal.hpp"

#include "tdx/common.hpp"

#include <iostream>
#include <utility>

namespace tdx::disclosure_detail {

void print_disclosure_command_help() {
    std::cout <<
        "Usage: tdx-tool market disclosures [--view all|schedule|express|recent|announcement] "
        "[--status all|scheduled|rescheduled|disclosed] [--market MARKET --code CODE] "
        "[--report-period YYYYMMDD] [--from DATE] [--to DATE] [--query TEXT] "
        "[--backfill-announcements] "
        "[--archive [--archive-path PATH]] "
        "[--root PATH] [--output PATH] [--compact]\n\n"
        "Batch announcement backfill (requires --backfill-announcements and, except dry-run, --archive):\n"
        "  --security CODE         Repeatable sz300503 / sz:300503 / 0300503\n"
        "  --securities LIST       Comma-separated securities\n"
        "  --backfill-input PATH   Repeatable JSON, CSV, text, or TDX .blk file\n"
        "  --backfill-watchlist    Read T0002/blocknew/zxg.blk\n"
        "  --backfill-block TEXT   Repeatable exact block id/code/name (includes descendants)\n"
        "  --state-path PATH       Resume checkpoint (default under T0002/tdx-tool)\n"
        "  --max-securities N      Safety cap (default 500)\n"
        "  --delay-ms N            Delay between securities (default 250)\n"
        "  --retries N             Retries per security (default 2)\n"
        "  --retry-delay-ms N      Linear retry delay base (default 1000)\n"
        "  --refresh-completed     Re-fetch securities already completed in state\n"
        "  --dry-run               Resolve universe without network or persistent state writes\n\n"
        "Coverage audit (uses the same security/watchlist/input/block selectors):\n"
        "  --audit-coverage         Read the archive and emit coverage plus a reusable items queue\n"
        "  --audit-period YYYYMMDD  Repeatable explicit quarter end\n"
        "  --audit-periods LIST     Comma-separated explicit quarter ends\n"
        "  --latest-periods N       Default period count when none is explicit (default 4)\n"
        "  --audit-as-of YYYYMMDD   Evaluate future/overdue schedules as of this date\n"
        "  --listing-dates-path P   Override T0002/hq_cache/base.dbf listing dates\n";
    std::cout <<
        "\nIncremental maintenance (uses the same selectors and archive/state paths):\n"
        "  --maintain               Refresh schedule archive, audit, then backfill due gaps\n"
        "  --maintenance-refresh-hours N  Recheck a completed due gap after N hours (default 24)\n";
}

namespace {

// Default output file per mode.  Maintenance and audit write their own reports;
// every universe run shares the batch report name.
const char* default_output_path(bool maintain, bool audit_coverage,
                                bool universe_mode) {
    if (maintain) return "output/tdx-disclosure-maintenance.json";
    if (audit_coverage) return "output/tdx-disclosure-coverage-audit.json";
    if (universe_mode)
        return "output/tdx-disclosure-announcement-backfill-batch.json";
    return "output/tdx-market-disclosures-native.json";
}

void validate_disclosure_command_options(
    const DisclosureCommandOptions& options, bool explicit_view,
    bool explicit_latest_audit_periods, bool explicit_maintenance_refresh) {
    if (options.query.backfill_announcements && explicit_view &&
        options.query.view != "all" && options.query.view != "announcement")
        throw Error("--backfill-announcements requires --view all or announcement");
    if (options.audit_coverage && options.maintain)
        throw Error("--audit-coverage and --maintain are mutually exclusive");
    if ((options.audit_coverage || options.maintain) &&
        options.query.backfill_announcements)
        throw Error("audit and maintenance modes do not use --backfill-announcements");
    if (options.batch_inputs && !options.query.backfill_announcements &&
        !options.audit_coverage && !options.maintain)
        throw Error("batch inputs require --backfill-announcements");
    if ((options.audit_coverage || options.maintain) && options.dry_run)
        throw Error("audit and maintenance modes cannot use --dry-run");
    if (!options.audit_coverage && !options.maintain &&
        (!options.audit_period_values.empty() ||
         !options.audit_periods_text.empty() || !options.audit_as_of.empty() ||
         !options.listing_dates_path_text.empty() ||
         explicit_latest_audit_periods))
        throw Error("coverage period options require --audit-coverage or --maintain");
    if (!options.maintain && explicit_maintenance_refresh)
        throw Error("--maintenance-refresh-hours requires --maintain");
    if (options.universe_mode && !options.audit_coverage && !options.maintain &&
        !options.dry_run && !options.archive &&
        options.archive_path_text.empty())
        throw Error("batch announcement backfill requires --archive or --archive-path");
}

}  // namespace

DisclosureCommandOptions parse_disclosure_command_options(Args& args) {
    DisclosureCommandOptions options;
    auto& query = options.query;
    const bool explicit_view = args.has("--view");
    query.backfill_announcements = args.take_flag("--backfill-announcements");
    query.view = lower_ascii(trim(args.take_option(
        "--view", query.backfill_announcements ? "announcement" : "schedule")));
    query.status = lower_ascii(trim(args.take_option("--status", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.report_period = trim(args.take_option("--report-period"));
    query.date_from = trim(args.take_option("--from"));
    query.date_to = trim(args.take_option("--to"));
    query.limit = bounded(args.take_option("--limit", "10000"), "--limit", 1, 20000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    query.refresh = true;
    options.root_text = args.take_option("--root");
    options.archive = args.take_flag("--archive");
    options.archive_path_text = args.take_option("--archive-path");
    options.audit_coverage = args.take_flag("--audit-coverage");
    options.maintain = args.take_flag("--maintain");
    options.audit_period_values = args.take_options("--audit-period");
    options.audit_periods_text = args.take_option("--audit-periods");
    options.audit_as_of = trim(args.take_option("--audit-as-of"));
    options.listing_dates_path_text = trim(args.take_option("--listing-dates-path"));
    const bool explicit_latest_audit_periods = args.has("--latest-periods");
    options.latest_audit_periods = bounded(args.take_option("--latest-periods", "4"),
                                           "--latest-periods", 1, 20);
    const bool explicit_maintenance_refresh = args.has("--maintenance-refresh-hours");
    options.maintenance_refresh_hours = bounded(args.take_option(
        "--maintenance-refresh-hours", "24"), "--maintenance-refresh-hours", 1, 720);
    options.security_values = args.take_options("--security");
    options.securities_text = args.take_option("--securities");
    options.input_values = args.take_options("--backfill-input");
    options.block_values = args.take_options("--backfill-block");
    options.watchlist = args.take_flag("--backfill-watchlist");
    options.state_path_text = args.take_option("--state-path");
    options.max_securities = bounded(args.take_option("--max-securities", "500"),
                                     "--max-securities", 1, 10000);
    auto& batch = options.batch;
    batch.timeout_ms = query.timeout_ms;
    batch.delay_ms = bounded(args.take_option("--delay-ms", "250"),
                             "--delay-ms", 0, 60000);
    batch.retry_count = bounded(args.take_option("--retries", "2"),
                                "--retries", 0, 10);
    batch.retry_delay_ms = bounded(args.take_option(
        "--retry-delay-ms", "1000"), "--retry-delay-ms", 0, 60000);
    batch.refresh_completed = args.take_flag("--refresh-completed");
    options.dry_run = args.take_flag("--dry-run");
    options.batch_inputs = options.watchlist || !options.security_values.empty() ||
        !options.securities_text.empty() || !options.input_values.empty() ||
        !options.block_values.empty();
    options.universe_mode =
        options.audit_coverage || options.maintain || options.batch_inputs;
    options.output = native_path(args.take_option(
        "--output", default_output_path(options.maintain, options.audit_coverage,
                                       options.universe_mode)));
    options.compact = args.take_flag("--compact");
    args.require_empty();
    validate_disclosure_command_options(options, explicit_view,
                                        explicit_latest_audit_periods,
                                        explicit_maintenance_refresh);
    return options;
}

}  // namespace tdx::disclosure_detail
