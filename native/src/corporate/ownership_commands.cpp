#include "ownership_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace ownership_detail;
int command_market_ownership(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market ownership [options]\n\n"
            "Native shareholder and insider changes, commitments, pledges, "
            "statistics, six aggregate rankings, shareholder counts, and institutions.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             changes|plans|insiders|commitments|pledges|statistics|institutions|rankings|shareholder-counts\n"
            "  --category NAME         View-specific category, default all\n"
            "  --query TEXT            Filter the selected master view\n"
            "  --market sz|sh|bj       Select one security with --code\n"
            "  --code CODE             Six-digit mainland security code\n"
            "  --institution-id ID     Expand one pledge institution\n"
            "  --details               Include selected dynamic details\n"
            "  --limit N               Master rows, default 1000\n"
            "  --detail-limit N        Detail rows, default 2000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-ownership-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    OwnershipQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "changes")));
    query.category = lower_ascii(trim(args.take_option("--category", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.institution_id = trim(args.take_option("--institution-id"));
    query.include_details = args.take_flag("--details");
    query.limit = bounded_integer(args.take_option("--limit", "1000"),
                                  "--limit", 1, 5000);
    query.detail_limit = bounded_integer(args.take_option("--detail-limit", "2000"),
                                         "--detail-limit", 1, 5000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-ownership-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {"industry"});
    OwnershipService service(blocks.securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("view").as_string()
              << " ownership query -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx