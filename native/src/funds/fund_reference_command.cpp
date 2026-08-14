#include "tdx/fund_reference.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

int bounded_limit(const std::string& value) {
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < 1 || parsed > 10000)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error("--limit must be in 1..10000");
    }
}

}  // namespace

int command_market_fund_reference(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market fund-reference [options]\n\n"
            "Read the client's local fund-unit/reference/NAV snapshot and ETF/LOF reference maps.\n\n"
            "Options:\n"
            "  --view NAME       all|snapshot|etf-mapping|lof-mapping (default all)\n"
            "  --market NAME     all|sz|sh or 0|1\n"
            "  --code CODE       Exact six-digit exchange security code\n"
            "  --query TEXT      Match code, name, reference code, or catalog id\n"
            "  --as-of-date DATE Recompute native lifecycle status at YYYYMMDD\n"
            "  --root PATH       TDX installation root\n"
            "  --limit N         1..10000 (default 5000)\n"
            "  --output PATH     Default output/tdx-market-fund-reference-native.json\n"
            "  --compact         Write compact JSON\n";
        return 0;
    }
    FundReferenceQuery query;
    query.view = args.take_option("--view", "all");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.as_of_date = args.take_option("--as-of-date");
    query.limit = bounded_limit(args.take_option("--limit", "5000"));
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-fund-reference-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    const auto document = load_local_fund_reference(root, query, blocks.securities);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " local fund-reference rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
