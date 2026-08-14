#include "tdx/curated_data.hpp"
#include "tdx/curated_data_internal.hpp"
#include "tdx/blocks.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using detail::curated_data::bounded;

namespace {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace

int command_market_curated_data(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market curated-data [options]\n\n"
            "Native client-curated screens, media links, capital returns and HK performance.\n\n"
            "  --view all|media-entertainment|low-valuation-smallcap|dividend-fundraising\n"
            "         |buyback-statistics|high-dividend|hk-performance\n"
            "         |high-refinancing-lending|below-book-soe\n"
            "  --query TEXT --market sz|sh|bj|hk --code CODE\n"
            "  --refresh --root PATH --input-dir PATH --limit N --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    CuratedDataQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"),
                          "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option(
        "--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-curated-data-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    CuratedDataService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " curated-data rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
