#include "capital_strength_internal.hpp"

#include "tdx/blocks.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_capital_strength(const std::vector<std::string>& raw_args) {
    using namespace detail::capital_strength;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market capital-strength [options]\n\n"
            "Typed TDX QSZJ five-period DDX capital-strength rankings.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             ranking|confluence|security|catalog\n"
            "  --period NAME           5d|10d|20d|30d|3m (ranking, default 5d)\n"
            "  --market sz|sh|bj       Use with --code\n"
            "  --code CODE             Optional security; required by security view\n"
            "  --query TEXT            Filter normalized content\n"
            "  --min-periods N         Confluence threshold, default 2\n"
            "  --sort NAME             Ranking: ddx|return|total-net|main-net|float-shares|source-rank|period|code\n"
            "                          Confluence: period-count|average-ddx|maximum-ddx|minimum-ddx|best-rank|code\n"
            "  --order asc|desc        Default desc; security defaults asc\n"
            "  --offset N --limit N    Default 0 / 500\n"
            "  --refresh --cache-ttl N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    CapitalStrengthQuery query;
    query.view = args.take_option("--view", "ranking");
    query.period = args.take_option("--period", "5d");
    query.market = args.take_option("--market");
    query.code = trim(args.take_option("--code"));
    query.query = trim(args.take_option("--query"));
    query.min_periods = bounded(
        args.take_option("--min-periods", "2"), "min-periods", 1, 5);
    query.sort = args.take_option("--sort");
    query.order = args.take_option("--order");
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

    std::map<std::pair<int, std::string>, Security> securities;
    if (lower_ascii(trim(query.view)) != "catalog") {
        const auto root = find_tdx_root(
            root_text.empty() ? fs::path{} : native_path(root_text));
        securities = load_blocks(root, {}).securities;
    }
    CapitalStrengthService service(std::move(securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed capital-strength query -> "
                  << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
