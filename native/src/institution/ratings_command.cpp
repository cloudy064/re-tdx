#include "ratings_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace ratings_detail;

int command_market_ratings(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market ratings [options]\n\n"
            "Native Hong Kong, United States, and research-industry ratings.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             hong-kong|us|industries\n"
            "  --stance NAME           all|positive|neutral|negative|unknown\n"
            "  --query TEXT            Filter returned rows\n"
            "  --market NAME           hk/31, us/74, or sz/sh/bj with --code\n"
            "  --code CODE             HK code, US symbol, or six-digit A-share code\n"
            "  --industry CODE         Select a six-digit research-industry code\n"
            "  --details               Fetch selected rating reports\n"
            "  --limit N               Catalog rows, default 2000\n"
            "  --detail-limit N        Report rows, default 1000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-ratings-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    RatingQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "hong-kong")));
    query.stance = lower_ascii(trim(args.take_option("--stance", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.industry = trim(args.take_option("--industry"));
    query.include_details = args.take_flag("--details");
    query.limit = bounded_integer(args.take_option("--limit", "2000"),
                                  "--limit", 1, 5000);
    query.detail_limit = bounded_integer(args.take_option("--detail-limit", "1000"),
                                         "--detail-limit", 1, 10000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-ratings-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    RatingService service(root, load_blocks(root, {"research-industry"}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("view").as_string()
              << " rating query -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
