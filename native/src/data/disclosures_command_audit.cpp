#include "disclosures_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

namespace tdx::disclosure_detail {
namespace fs = std::filesystem;

int run_disclosure_coverage_audit(
    const DisclosureCommandOptions& options,
    const std::vector<DisclosureBackfillSecurity>& securities,
    const fs::path& root) {
    const auto archive_path = resolve_disclosure_archive_path(
        root, options.archive_path_text);
    if (!fs::is_regular_file(archive_path))
        throw Error("disclosure availability archive is missing: " +
                    path_utf8(archive_path));
    const auto archive_document = Json::parse(read_text_utf8(archive_path));
    const auto listing_dates = load_disclosure_listing_dates(
        root, options.listing_dates_path_text);
    auto document = audit_disclosure_coverage(
        securities, archive_document, collect_disclosure_audit_periods(options),
        static_cast<std::size_t>(options.latest_audit_periods),
        options.audit_as_of, listing_dates.is_null() ? nullptr : &listing_dates);
    document["archive_path"] = path_utf8(archive_path);
    atomic_write_text(options.output,
                      document.dump(options.compact ? -1 : 2) + "\n");
    const auto& summary = document.at("summary");
    std::cout << "audited " << summary.at("security_count").as_number()
              << " securities across "
              << summary.at("report_period_count").as_number()
              << " periods; "
              << summary.at("securities_needing_backfill").as_number()
              << " need backfill -> " << path_utf8(options.output) << '\n';
    return 0;
}

}  // namespace tdx::disclosure_detail
