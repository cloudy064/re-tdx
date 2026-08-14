#include "strong_stocks_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace detail::strong_stocks;

int command_market_strong_stocks(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market strong-stocks [options]\n\n"
            "Typed TDX QSGFX strong-stock lifecycle and daily reason data.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             intervals|security|detail|catalog\n"
            "  --market sz|sh|bj       Use with --code\n"
            "  --code CODE             Required by security; optional detail cross-check\n"
            "  --interval-id ID        Required by detail (18 digits)\n"
            "  --query TEXT            Search normalized content/reasons\n"
            "  --from DATE --to DATE   Interval overlap or detail date filter\n"
            "  --min-trading-days N --min-limit-up-days N\n"
            "  --sort NAME             Interval: end-date|start-date|return|index-return|excess-return|days|limit-ups|source-rank|code\n"
            "                          Detail: date|return|amount|market-limit-ups|seal-rate\n"
            "  --order asc|desc        Default desc; detail defaults asc\n"
            "  --offset N --limit N    Default 0 / 500\n"
            "  --refresh --cache-ttl N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    StrongStocksQuery query;
    query.view = args.take_option("--view", "intervals");
    query.market = args.take_option("--market");
    query.code = trim(args.take_option("--code"));
    query.interval_id = trim(args.take_option("--interval-id"));
    query.query = trim(args.take_option("--query"));
    query.from = args.take_option("--from");
    query.to = args.take_option("--to");
    query.min_trading_days = bounded(args.take_option("--min-trading-days", "0"),
                                     "min-trading-days", 0, 1000);
    query.min_limit_up_days = bounded(args.take_option("--min-limit-up-days", "0"),
                                      "min-limit-up-days", 0, 1000);
    query.sort = args.take_option("--sort");
    query.order = args.take_option("--order");
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"), "limit", 1, 10000);
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(args.take_option("--cache-ttl", "300"),
                                      "cache-ttl", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "timeout-ms", 100, 60000);
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    std::map<std::pair<int, std::string>, Security> securities;
    if (lower_ascii(trim(query.view)) != "catalog") {
        const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
        securities = load_blocks(root, {}).securities;
    }
    StrongStocksService service(std::move(securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed strong-stocks query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx

