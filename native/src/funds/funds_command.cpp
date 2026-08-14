#include "funds_internal.hpp"

#include "tdx/blocks.hpp"

#include <iostream>

namespace fs = std::filesystem;
namespace tdx {
int command_market_funds(const std::vector<std::string>& raw_args) {
    using namespace detail::funds;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market funds [options]\n\n"
            "Native ReqId 200340 -> 200341 segmented main-fund aggregation.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --market sz|sh|bj       Security market (requires --code)\n"
            "  --code CODE             Security code; local research industry is resolved\n"
            "  --industry 881xxx       Expand one level-1 research industry\n"
            "  --all-industries        Expand all 30 industries and build reverse index\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-intraday-funds-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    IntradayFundsQuery query;
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.industry = trim(args.take_option("--industry"));
    query.all_industries = args.take_flag("--all-industries");
    query.refresh = true;
    query.timeout_ms = bounded_integer(
        args.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000);
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-intraday-funds-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    const auto blocks = load_blocks(root, {"research-industry"});
    IntradayFundsService service(root, blocks);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("mode").as_string()
              << " intraday funds -> " << path_utf8(output) << '\n';
    return 0;
}
}  // namespace tdx
