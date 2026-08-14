#include "block_trades_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
namespace bt_detail = block_trade_detail;

int command_market_block_trades(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market block-trades [options]\n\n"
            "Native block-trade, intention, broker, and industry analysis.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             trades|intentions|brokers|industries\n"
            "  --query TEXT            Filter the selected view\n"
            "  --market sz|sh|bj       Select one security with --code\n"
            "  --code CODE             Six-digit security code\n"
            "  --broker-id ID          Expand one broker's transaction history\n"
            "  --period 1m|3m|6m|1y    Broker ranking period, default 1m\n"
            "  --month YYYY-MM         Industry summary month, default latest\n"
            "  --industry CODE         Expand one six-digit industry\n"
            "  --details               Include selected dynamic details\n"
            "  --limit N               Master rows, default 500\n"
            "  --detail-limit N        Dynamic detail rows, default 1000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-block-trades-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    BlockTradeQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "trades")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.broker_id = trim(args.take_option("--broker-id"));
    query.period = lower_ascii(trim(args.take_option("--period", "1m")));
    query.month = trim(args.take_option("--month"));
    query.industry = trim(args.take_option("--industry"));
    query.include_details = args.take_flag("--details");
    query.limit = bt_detail::bounded_integer(
        args.take_option("--limit", "500"), "--limit", 1, 5000);
    query.detail_limit = bt_detail::bounded_integer(
        args.take_option("--detail-limit", "1000"),
        "--detail-limit", 1, 5000);
    query.timeout_ms = bt_detail::bounded_integer(
        args.take_option("--timeout-ms", "15000"),
        "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = bt_detail::native_path(args.take_option(
        "--output", "output/tdx-market-block-trades-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : bt_detail::native_path(root_text));
    auto blocks = load_blocks(root, {"industry"});
    BlockTradeService service(blocks.securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("view").as_string()
              << " block-trade query -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
