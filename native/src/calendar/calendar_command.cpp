#include "tdx/calendar.hpp"

#include "calendar_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {

using namespace calendar_detail;

int command_market_calendar(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market calendar "
                     "[--view all|macro|meetings|company|rights-issues|listings|major-events|"
                     "ipo-announcements|recent-ipos|ipo-guidance|ipo-review|ipo-subscriptions|"
                     "ipo-subscription-details|ipo-companion-news|us-ipo|us-ipo-applications|"
                     "us-ipo-calendar|us-ipo-listed|us-ipo-pending|board-news|star-news|chinext-news|"
                     "neeq-news|futures] "
                     "[--query TEXT] [--market MARKET --code CODE] [--event ID] [--from DATE] [--to DATE] "
                     "[--root PATH] [--input-dir PATH] [--refresh] [--output PATH] [--compact]\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto input_dir = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    CalendarQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.event_id = trim(args.take_option("--event"));
    query.date_from = trim(args.take_option("--from"));
    query.date_to = trim(args.take_option("--to"));
    query.limit = bounded(args.take_option("--limit", "5000"), "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000);
    query.refresh = args.take_flag("--refresh");
    const auto output = native_path(args.take_option("--output", "output/tdx-market-calendar-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    CalendarService service(load_blocks(root, {}).securities, input_dir);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("rows").size() << " calendar rows -> " << path_utf8(output) << '\n';
    return 0;
}


}  // namespace tdx
