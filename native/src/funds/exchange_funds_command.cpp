#include "exchange_funds_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {
namespace detail = exchange_fund_detail;

int command_market_exchange_funds(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market exchange-funds "
                     "[--view all|etf-performance|etf-share-ranking|etf-scale-flow|commodity-etf|"
                     "cash-arbitrage|cash-yield|lof|closed-fund|"
                     "cash-management-calendar|reits-issued|reits-pipeline] "
                     "[--query TEXT] [--market MARKET --code CODE] "
                     "[--quotes] [--refresh] [--root PATH] [--input-dir PATH] "
                     "[--limit N] [--output PATH] [--compact]\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ExchangeFundQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.include_quotes = args.take_flag("--quotes");
    query.refresh = args.take_flag("--refresh");
    query.limit = detail::bounded(
        args.take_option("--limit", "5000"), "--limit", 1, 10000);
    query.timeout_ms = detail::bounded(
        args.take_option("--timeout-ms", "15000"),
        "--timeout-ms", 100, 60000);
    const auto input = detail::native_path(args.take_option(
        "--input-dir", "output/tdx-jsn"));
    const auto output = detail::native_path(args.take_option(
        "--output", "output/tdx-market-exchange-funds-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : detail::native_path(root_text));
    auto blocks = load_blocks(root, {});
    ExchangeFundService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " exchange-fund rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
