#include "repurchases_internal.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace detail::repurchases;

int command_market_repurchases(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market repurchases [options]\n\n"
            "Native A-share plans, monthly/annual statistics, and Hong Kong history.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --jsn-root PATH         Prefer downloaded JSN resources\n"
            "  --view NAME             plans|monthly|annual|hong-kong\n"
            "  --query TEXT            Filter the selected master view\n"
            "  --market sz|sh|bj|hk    Select one security with --code\n"
            "  --code CODE             Six mainland or five Hong Kong digits\n"
            "  --segment all|a|hk      Annual statistics segment, default a\n"
            "  --year YYYY             Expand one annual monthly trend\n"
            "  --details               Include selected dynamic details\n"
            "  --limit N               Master rows, default 1000\n"
            "  --detail-limit N        Detail rows, default 2000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-repurchases-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto jsn_root_text = args.take_option("--jsn-root");
    RepurchaseQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "plans")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.segment = lower_ascii(trim(args.take_option("--segment", "a")));
    query.year = trim(args.take_option("--year"));
    query.include_details = args.take_flag("--details");
    query.limit = bounded_integer(args.take_option("--limit", "1000"),
                                  "--limit", 1, 5000);
    query.detail_limit = bounded_integer(args.take_option("--detail-limit", "2000"),
                                         "--detail-limit", 1, 5000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-repurchases-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {"industry"});
    RepurchaseService service(
        blocks.securities,
        jsn_root_text.empty() ? fs::path{} : native_path(jsn_root_text));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("view").as_string()
              << " repurchase query -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
