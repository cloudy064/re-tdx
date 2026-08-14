#include "tdx/trades.hpp"
#include "tdx/trades_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_trades(const std::vector<std::string>& raw_args) {
    using detail::trades::maximum_pages;
    using detail::trades::parse_integer;

    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market trades --security [MARKET:]CODE [options]\n\n"
            "Native public 7709 commands 0x0FC5/0x0FC6 L1 trade details.\n\n"
            "Options:\n"
            "  --security CODE        Repeatable, e.g. sz:000001\n"
            "  --date YYYYMMDD        Historical date; omit for current session\n"
            "  --page-size N          Default 1800 today, 2000 history\n"
            "  --max-pages N          Safety limit 1..100 (default 100)\n"
            "  --host HOST[:PORT]     Repeatable; default connect.cfg HQHOST primary-first\n"
            "  --timeout-ms N         Default 10000\n"
            "  --root PATH            TDX installation root\n"
            "  --output PATH          Default output/tdx-trades-native.json\n"
            "  --compact              Compact JSON\n";
        return 0;
    }
    const auto securities = args.take_options("--security");
    const auto trading_date = args.take_option("--date");
    const int page_size = parse_integer(args.take_option("--page-size", "0"),
                                        "--page-size", 0, 65535);
    const int max_page_count = parse_integer(args.take_option("--max-pages", "100"),
                                             "--max-pages", 1, maximum_pages);
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "10000"),
                                         "--timeout-ms", 1, 600000);
    const auto hosts = args.take_options("--host");
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output", "output/tdx-trades-native.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
    const auto document = fetch_market_trades_document(root, securities, trading_date,
        page_size, max_page_count, timeout_ms, nullptr, hosts);
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("received").as_number() << " securities, "
              << document.at("tick_count").as_number() << " trade ticks -> "
              << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
