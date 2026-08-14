#include "disclosures_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

namespace tdx::disclosure_detail {
namespace fs = std::filesystem;
namespace {

// The archived observation is always the unfiltered master view so the merged
// history does not inherit the caller's filters.  Announcement rows are
// re-attached from the caller's result because they are fetched per security.
DisclosureQuery archive_observation_query(const DisclosureQuery& query) {
    auto archive = query;
    archive.view = "all";
    archive.status = "all";
    archive.query.clear();
    archive.market.clear();
    archive.code.clear();
    archive.report_period.clear();
    archive.date_from.clear();
    archive.date_to.clear();
    archive.backfill_announcements = false;
    archive.refresh = false;
    archive.limit = 20000;
    return archive;
}

}  // namespace

int run_disclosure_master_query(const DisclosureCommandOptions& options,
                                const fs::path& root,
                                DisclosureService& service) {
    auto document = service.query(options.query);
    if (options.archive || !options.archive_path_text.empty()) {
        auto observation =
            service.query(archive_observation_query(options.query));
        if (options.query.backfill_announcements) {
            for (const auto& row : document.at("rows").as_array())
                if (text_value(row, "kind") == "announcement-report")
                    observation["rows"].push_back(row);
        }
        const auto archive_path = resolve_disclosure_archive_path(
            root, options.archive_path_text);
        Json existing;
        const Json* existing_pointer = nullptr;
        if (fs::exists(archive_path)) {
            existing = Json::parse(read_text_utf8(archive_path));
            existing_pointer = &existing;
        }
        const auto merged = merge_disclosure_archive_document(
            observation, existing_pointer, text_value(observation, "generated_at"));
        atomic_write_text(archive_path, merged.dump(2) + "\n");
        Json metadata = Json::object();
        metadata["path"] = path_utf8(archive_path);
        metadata["schema"] = merged.at("schema");
        metadata["summary"] = merged.at("summary");
        document["archive"] = std::move(metadata);
    }
    atomic_write_text(options.output,
                      document.dump(options.compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("rows").size()
              << " disclosure rows -> " << path_utf8(options.output) << '\n';
    return 0;
}

}  // namespace tdx::disclosure_detail
