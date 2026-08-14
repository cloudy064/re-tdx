#include "disclosures_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <set>
#include <string>

namespace fs = std::filesystem;

namespace tdx {
using namespace disclosure_detail;

int command_market_disclosures(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        print_disclosure_command_help();
        return 0;
    }
    const auto options = parse_disclosure_command_options(args);
    const auto root = find_tdx_root(
        options.root_text.empty() ? fs::path{} : native_path(options.root_text));
    // Block families are only needed to expand --backfill-block selectors;
    // every other mode just wants the security directory.
    const std::set<std::string> all_block_families{
        "industry", "research-industry", "concept", "style", "index"};
    const auto block_data = load_blocks(root, options.block_values.empty()
        ? std::set<std::string>{} : all_block_families);
    DisclosureService service(block_data.securities);
    if (!options.universe_mode)
        return run_disclosure_master_query(options, root, service);
    const auto securities =
        resolve_disclosure_universe(options, block_data, root);
    if (options.audit_coverage)
        return run_disclosure_coverage_audit(options, securities, root);
    if (options.maintain)
        return run_disclosure_maintenance(options, securities, root, block_data,
                                          service);
    if (options.dry_run)
        return run_disclosure_backfill_preview(options, securities);
    return run_disclosure_backfill_batch(options, securities, root, block_data);
}

}  // namespace tdx
