#include "disclosures_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <set>
#include <utility>

namespace tdx::disclosure_detail {
namespace fs = std::filesystem;

int run_disclosure_backfill_preview(
    const DisclosureCommandOptions& options,
    const std::vector<DisclosureBackfillSecurity>& securities) {
    Json document = Json::object();
    document["schema"] = "tdx-disclosure-announcement-backfill-preview-native-v1";
    document["generated_at"] = now_text();
    document["requested"] = static_cast<std::uint64_t>(securities.size());
    Json items = Json::array();
    for (const auto& security : securities) {
        Json item = Json::object();
        item["market"] = security.market;
        item["code"] = security.code;
        item["security_id"] = market_prefix(
            mainland_market_id(security.market)) + security.code;
        item["name"] = security.name;
        items.push_back(std::move(item));
    }
    document["items"] = std::move(items);
    document["network_requests"] = 0;
    document["archive_or_state_writes_performed"] = false;
    atomic_write_text(options.output,
                      document.dump(options.compact ? -1 : 2) + "\n");
    std::cout << "previewed " << securities.size()
              << " announcement backfill securities -> "
              << path_utf8(options.output) << '\n';
    return 0;
}

int run_disclosure_backfill_batch(
    const DisclosureCommandOptions& options,
    const std::vector<DisclosureBackfillSecurity>& securities,
    const fs::path& root, const BlockData& block_data) {
    const auto archive_path = resolve_disclosure_archive_path(
        root, options.archive_path_text);
    const auto state_path = resolve_disclosure_state_path(
        root, options.state_path_text);
    Json existing_archive;
    const Json* archive_pointer = nullptr;
    if (fs::is_regular_file(archive_path)) {
        existing_archive = Json::parse(read_text_utf8(archive_path));
        archive_pointer = &existing_archive;
    }
    auto existing_state =
        load_disclosure_backfill_state(state_path, archive_path);
    std::size_t request_number = 0;
    auto result = run_disclosure_announcement_backfill_batch(
        securities, archive_pointer, &existing_state, options.batch,
        make_disclosure_fetcher(block_data.securities, "request",
                                request_number),
        make_disclosure_checkpoint(archive_path, state_path));
    atomic_write_text(archive_path, result.archive.dump(2) + "\n");
    atomic_write_text(state_path, result.state.dump(2) + "\n");
    Json document = Json::object();
    document["schema"] = "tdx-disclosure-announcement-backfill-batch-native-v1";
    document["generated_at"] = now_text();
    document["archive_path"] = path_utf8(archive_path);
    document["state_path"] = path_utf8(state_path);
    document["summary"] = result.summary;
    Json items = Json::array();
    std::set<std::string> requested_ids;
    for (const auto& security : securities)
        requested_ids.insert(market_prefix(mainland_market_id(security.market)) +
                             security.code);
    for (const auto& entry : result.state.at("entries").as_array())
        if (requested_ids.count(text_value(entry, "security_id")))
            items.push_back(entry);
    document["items"] = std::move(items);
    document["options"] = disclosure_backfill_options_document(
        options.batch, options.max_securities);
    atomic_write_text(options.output,
                      document.dump(options.compact ? -1 : 2) + "\n");
    std::cout << "batch requested " << result.summary.at("requested").as_number()
              << ", succeeded " << result.summary.at("succeeded").as_number()
              << ", skipped " << result.summary.at("skipped_completed").as_number()
              << ", failed " << result.summary.at("failed").as_number()
              << " -> " << path_utf8(options.output) << '\n';
    return result.summary.at("failed").as_number() > 0 ? 3 : 0;
}

}  // namespace tdx::disclosure_detail
