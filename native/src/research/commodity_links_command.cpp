#include "commodity_links_internal.hpp"

namespace tdx {
namespace fs = std::filesystem;
using namespace commodity_links_detail;

int command_market_commodity_links(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market commodity-links [options]\n\n"
            "Typed TDX commodity quotes, price-rise themes and stock linkage chains.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             commodities|commodity|themes|theme|driver|security|catalog\n"
            "  --commodity-id ID       Required by commodity\n"
            "  --theme-id ID           Required by theme and driver\n"
            "  --driver-id ID          Required by driver\n"
            "  --market sz|sh|bj --code CODE   Required by security\n"
            "  --query TEXT            Search normalized content\n"
            "  --sort NAME             Commodity: quote-date|name|price|day-change|5d|10d|30d|60d\n"
            "                          Theme: latest-driver-date|trigger-date|name|stocks\n"
            "  --order asc|desc --offset N --limit N\n"
            "  --refresh --cache-ttl N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    CommodityLinksQuery query;
    query.view = args.take_option("--view", "commodities");
    query.commodity_id = args.take_option("--commodity-id");
    query.theme_id = args.take_option("--theme-id");
    query.driver_id = args.take_option("--driver-id");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
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
    CommodityLinksService service(std::move(securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed commodity-links query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
