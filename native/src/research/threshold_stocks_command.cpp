#include "threshold_stocks_internal.hpp"

#include "tdx/blocks.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_threshold_stocks(const std::vector<std::string>& raw_args) {
    using namespace detail::threshold_stocks;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market threshold-stocks [options]\n\n"
            "Typed TDX QYSZ 百元股 / 千亿市值 history and dated members.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             history|members|security|catalog\n"
            "  --universe NAME         high-price|mega-cap\n"
            "  --date YYYYMMDD         Default latest resource date\n"
            "  --status NAME           all|continuing|entered|exited\n"
            "  --market sz|sh|bj       Use with --code\n"
            "  --code CODE             Optional security; required by security view\n"
            "  --query TEXT            Filter normalized member content\n"
            "  --sort NAME             History: date|count|market-cap|market-share|entered|exited\n"
            "                          Members: code|day-return|threshold-value|net-increase|status\n"
            "  --order asc|desc        Default desc\n"
            "  --offset N --limit N    Default 0 / 500\n"
            "  --refresh --cache-ttl N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ThresholdStocksQuery query;
    query.view = args.take_option("--view", "history");
    query.universe = args.take_option("--universe", "high-price");
    query.date = trim(args.take_option("--date"));
    query.status = args.take_option("--status", "all");
    query.market = args.take_option("--market");
    query.code = trim(args.take_option("--code"));
    query.query = trim(args.take_option("--query"));
    const auto default_sort = lower_ascii(trim(query.view)) == "history"
        ? "date" : "threshold-value";
    query.sort = args.take_option("--sort", default_sort);
    query.order = args.take_option("--order", "desc");
    query.offset = bounded(args.take_option("--offset", "0"),
                           "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"),
                          "limit", 1, 10000);
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(
        args.take_option("--cache-ttl", "300"), "cache-ttl", 0, 86400);
    query.timeout_ms = bounded(
        args.take_option("--timeout-ms", "15000"), "timeout-ms", 100, 60000);
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    ThresholdStocksService service(load_blocks(root, {}).securities);
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) {
        std::cout << rendered;
    } else {
        const auto output = native_path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed threshold-stock query -> "
                  << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
