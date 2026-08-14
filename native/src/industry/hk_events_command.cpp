#include "hk_events_internal.hpp"

#include "tdx/common.hpp"

#include <iostream>

namespace tdx {
namespace {

struct HkEventCommandDefaults {
    static constexpr const char* view = "all";
    static constexpr const char* market = "31";
    static constexpr const char* limit = "10000";
    static constexpr const char* pages = "1";
    static constexpr const char* page_size = "800";
    static constexpr const char* start = "0";
    static constexpr const char* timeout_ms = "15000";
    static constexpr const char* input_dir = "output/tdx-jsn";
    static constexpr const char* events_output =
        "output/tdx-market-hk-events-native.json";
    static constexpr const char* short_history_output =
        "output/tdx-market-hk-short-history-native.json";
};

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

int command_market_hk_events(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market hk-events "
                     "[--view all|dividends|holdings|short-selling|applications] "
                     "[--query TEXT] [--code HKCODE] [--from DATE] [--to DATE] "
                     "[--input-dir PATH] [--refresh] [--limit N] "
                     "[--output PATH] [--compact]\n";
        return 0;
    }
    HkEventQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", HkEventCommandDefaults::view)));
    query.query = trim(args.take_option("--query"));
    query.code = trim(args.take_option("--code"));
    query.date_from = trim(args.take_option("--from"));
    query.date_to = trim(args.take_option("--to"));
    query.limit = bounded(args.take_option("--limit", HkEventCommandDefaults::limit),
                          "--limit", 1, 20000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", HkEventCommandDefaults::timeout_ms),
                               "--timeout-ms", 100, 60000);
    query.refresh = args.take_flag("--refresh");
    const auto input = detail::native_utf8_path(args.take_option(
        "--input-dir", HkEventCommandDefaults::input_dir));
    const auto output = detail::native_utf8_path(args.take_option(
        "--output", HkEventCommandDefaults::events_output));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    HkEventService service(input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " HK event rows -> " << path_utf8(output) << '\n';
    return 0;
}

int command_market_hk_short_history(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market hk-short-history --code HKCODE [options]\n\n"
                     "Fetch 7727 daily HK short-selling volume and reconcile overlapping GGRL104 rows.\n\n"
                     "  --market 31|48       Expansion market (default 31)\n"
                     "  --code HKCODE        Required five-digit HK security code\n"
                     "  --pages N             1..20 K-line pages (default 1)\n"
                     "  --page-size N          1..800 bars per page (default 800)\n"
                     "  --start N              History offset (default 0)\n"
                     "  --input-dir PATH       JSN mirror for GGRL104 reconciliation\n"
                     "  --refresh              Refresh GGRL event sources\n"
                     "  --timeout-ms N          Network timeout (default 15000)\n"
                     "  --output FILE           Write JSON instead of stdout\n"
                     "  --compact\n";
        return 0;
    }
    HkShortHistoryQuery query;
    query.market = trim(args.take_option("--market", HkEventCommandDefaults::market));
    query.code = trim(args.take_option("--code"));
    query.pages = bounded(args.take_option("--pages", HkEventCommandDefaults::pages), "--pages", 1, 20);
    query.page_size = bounded(args.take_option("--page-size", HkEventCommandDefaults::page_size),
                              "--page-size", 1, 800);
    query.start = bounded(args.take_option("--start", HkEventCommandDefaults::start), "--start", 0, 65535);
    query.timeout_ms = bounded(args.take_option(
        "--timeout-ms", HkEventCommandDefaults::timeout_ms),
                               "--timeout-ms", 100, 60000);
    query.refresh = args.take_flag("--refresh");
    const auto input = detail::native_utf8_path(args.take_option(
        "--input-dir", HkEventCommandDefaults::input_dir));
    const auto output = detail::native_utf8_path(args.take_option(
        "--output", HkEventCommandDefaults::short_history_output));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (query.code.empty()) throw Error("--code is required");
    HkEventService service(input);
    const auto document = service.query_short_history(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("summary").at("history_count").as_number()
              << " HK short-history rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
