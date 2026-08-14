#include "tdx/hk_actions.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

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

int bounded(const std::string& value, std::string_view name,
            int low, int high) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < low || parsed > high)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(low) + ".." + std::to_string(high));
    }
}

}  // namespace

int command_market_hk_actions(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market hk-actions [options]\n\n"
            "Decode the client's long-history HK corporate-action and native adjustment factors.\n\n"
            "Options:\n"
            "  --code HKCODE     Exact five-digit HK security code\n"
            "  --query TEXT      Match code, date, description, or event kind\n"
            "  --kind NAME       all|dividend|bonus|rights|split|consolidation|mixed|adjustment|other\n"
            "  --from DATE       Inclusive YYYYMMDD lower bound\n"
            "  --to DATE         Inclusive YYYYMMDD upper bound\n"
            "  --order NAME      asc|desc (default desc)\n"
            "  --offset N        Result offset, 0..1000000\n"
            "  --limit N         1..2000 (default 200)\n"
            "  --root PATH       TDX installation root\n"
            "  --output PATH     Default output/tdx-market-hk-actions-native.json\n"
            "  --compact         Write compact JSON\n";
        return 0;
    }
    HkActionQuery query;
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.kind = args.take_option("--kind", "all");
    query.date_from = args.take_option("--from");
    query.date_to = args.take_option("--to");
    query.order = args.take_option("--order", "desc");
    query.offset = bounded(args.take_option("--offset", "0"),
                           "--offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "200"),
                          "--limit", 1, 2000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-hk-actions-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    const auto document = load_local_hk_actions(root, query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " local HK action rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
