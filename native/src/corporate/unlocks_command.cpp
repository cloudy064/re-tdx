#include "unlocks_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace unlocks_detail;

int command_market_unlocks(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market unlocks [options]\n\n"
            "Native restricted-share unlock calendar and shareholder drill-down.\n\n"
            "Options:\n"
            "  --view calendar|recent-large|monthly-pressure  Default calendar\n"
            "  --root PATH             TDX installation root\n"
            "  --query TEXT            Filter by stock, code, date, status, or reason\n"
            "  --market sz|sh|bj       Select one security with --code\n"
            "  --code CODE             Six-digit security code\n"
            "  --detail-id ID          Expand YYYYMMDD + six-digit code event key\n"
            "  --start-date YYYYMMDD   Inclusive event start date\n"
            "  --end-date YYYYMMDD     Inclusive event end date\n"
            "  --progress TEXT         Exact status, for example 实施 or 未实施\n"
            "  --reason TEXT           Exact unlock reason\n"
            "  --details               Fetch shareholder details for returned events\n"
            "  --limit N               Event rows, default 500\n"
            "  --detail-limit N        Shareholder rows per event, default 1000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --input-dir PATH        Local JSN mirror for monthly pressure\n"
            "  --output PATH           Default output/tdx-market-unlocks-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    UnlockQuery query;
    query.view = args.take_option("--view", "calendar");
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.detail_id = trim(args.take_option("--detail-id"));
    query.start_date = trim(args.take_option("--start-date"));
    query.end_date = trim(args.take_option("--end-date"));
    query.progress = trim(args.take_option("--progress"));
    query.reason = trim(args.take_option("--reason"));
    query.include_details = args.take_flag("--details");
    query.limit = bounded_integer(args.take_option("--limit", "500"),
                                  "--limit", 1, 5000);
    query.detail_limit = bounded_integer(args.take_option("--detail-limit", "1000"),
                                         "--detail-limit", 1, 5000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = args.take_flag("--refresh");
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-unlocks-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {"industry"});
    UnlockService service(blocks.securities, input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("mode").as_string() << " unlock query with ";
    if (document.at("mode").as_string() == "monthly-pressure")
        std::cout << document.at("counts").at("months").as_number() << " months";
    else
        std::cout << document.at("counts").at("returned_events").as_number()
                  << " events and "
                  << document.at("counts").at("returned_shareholders").as_number()
                  << " shareholder rows";
    std::cout << " -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
