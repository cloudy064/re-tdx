#include "research_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace detail::research;

int command_market_research(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market research [options]\n\n"
            "Native institution research, interaction, regulatory, industry, and institution drill-downs.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --category NAME         institution-research/interaction/featured-interaction/\n"
            "                          industry/post-st/exchange-inquiry/regulatory/\n"
            "                          market-ban/notable-institution\n"
            "  --query TEXT            Filter the selected master list\n"
            "  --market sz|sh|bj       Select one security with --code\n"
            "  --code CODE             Six-digit security code\n"
            "  --detail-id ID          Expand an industry or institution dynamic key\n"
            "  --details               Expand the first filtered row\n"
            "  --limit N               Master rows, default 500\n"
            "  --detail-limit N        Detail rows, default 100\n"
            "  --no-text               Omit long Q&A and event bodies\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-research-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ResearchQuery query;
    query.category = lower_ascii(trim(args.take_option("--category", "institution-research")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.detail_id = trim(args.take_option("--detail-id"));
    query.include_details = args.take_flag("--details");
    query.include_text = !args.take_flag("--no-text");
    query.limit = bounded_integer(args.take_option("--limit", "500"), "--limit", 1, 5000);
    query.detail_limit = bounded_integer(args.take_option("--detail-limit", "100"),
                                         "--detail-limit", 1, 1000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-research-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {"industry"});
    ResearchService service(blocks.securities, block_names(blocks));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("mode").as_string()
              << " research query with "
              << document.at("counts").at("returned_records").as_number()
              << " master rows and " << document.at("counts").at("activities").as_number()
              << " activities -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
