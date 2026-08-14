#include "tdx/index_events.hpp"

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

int command_market_index_events(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market index-events [options]\n\n"
            "Read local major-event annotations used by domestic, Hang Seng, and US index charts.\n\n"
            "Options:\n"
            "  --benchmark NAME  all|shanghai-composite|hang-seng|nasdaq-composite\n"
            "  --event-id ID     Exact native article/event id\n"
            "  --query TEXT      Match title, benchmark, or event id\n"
            "  --from YYYYMMDD   First included date\n"
            "  --to YYYYMMDD     Last included date\n"
            "  --date-basis NAME chart|occurrence (default chart)\n"
            "  --sort NAME       date|event-id|title (default date)\n"
            "  --order NAME      asc|desc (default desc)\n"
            "  --offset N        0..1000000 (default 0)\n"
            "  --limit N         1..10000 (default 500)\n"
            "  --root PATH       TDX installation root\n"
            "  --output PATH     Default output/tdx-market-index-events-native.json\n"
            "  --compact         Write compact JSON\n";
        return 0;
    }
    const IndexEventQuery defaults;
    IndexEventQuery query;
    query.benchmark = args.take_option("--benchmark", defaults.benchmark);
    query.event_id = args.take_option("--event-id");
    query.query = args.take_option("--query");
    query.date_from = args.take_option("--from");
    query.date_to = args.take_option("--to");
    query.date_basis = args.take_option("--date-basis", defaults.date_basis);
    query.sort = args.take_option("--sort", defaults.sort);
    query.order = args.take_option("--order", defaults.order);
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0,
                           1000000);
    query.limit = bounded(args.take_option("--limit", "500"), "limit", 1,
                          10000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-index-events-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    const auto document = load_local_index_events(root, query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " local index-event rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
