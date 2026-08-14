#include "tdx/historical_securities.hpp"

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

int bounded(const std::string& text, std::string_view name, int low, int high) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < low || value > high)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(low) +
                    ".." + std::to_string(high));
    }
}

}  // namespace

int command_market_historical_securities(
    const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market historical-securities [options]\n\n"
            "Read the client's local compatibility names for current and historical securities.\n\n"
            "Options:\n"
            "  --market NAME     all|sz|sh|bj or 0|1|2\n"
            "  --code CODE       Exact six-digit security code\n"
            "  --query TEXT      Match code, compatibility name, or current name\n"
            "  --presence NAME   all|current|absent (default all)\n"
            "  --sort NAME       code|name|presence (default code)\n"
            "  --order NAME      asc|desc (default asc)\n"
            "  --offset N        0..1000000 (default 0)\n"
            "  --limit N         1..10000 (default 1000)\n"
            "  --root PATH       TDX installation root\n"
            "  --output PATH     Default output/tdx-market-historical-securities-native.json\n"
            "  --compact         Write compact JSON\n";
        return 0;
    }
    const HistoricalSecurityQuery defaults;
    HistoricalSecurityQuery query;
    query.market = args.take_option("--market", defaults.market);
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.presence = args.take_option("--presence", defaults.presence);
    query.sort = args.take_option("--sort", defaults.sort);
    query.order = args.take_option("--order", defaults.order);
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0,
                           1000000);
    query.limit = bounded(args.take_option("--limit", "1000"), "limit", 1,
                          10000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-historical-securities-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    const auto document = load_local_historical_securities(
        root, query, load_blocks(root, {}).securities);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " local historical-security rows -> "
              << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
