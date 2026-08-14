// CLI adapter for market economic-indicators.
#include "tdx/economic_indicators_internal.hpp"

#include "tdx/common.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace tdx::economic_indicator_detail;

int command_market_economic_indicators(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market economic-indicators [options]\n\n"
            "  --view catalog|indicator --indicator-id ID --query TEXT\n"
            "  --sort update-date|name|value|mom|yoy --order asc|desc\n"
            "  --no-history --no-related --no-quotes --offset N --limit N\n"
            "  --history-limit N --refresh --master-cache-ttl N\n"
            "  --detail-cache-ttl N --quote-cache-ttl N --timeout-ms N\n"
            "  --root PATH --output FILE --compact\n";
        return 0;
    }
    EconomicIndicatorQuery query;
    query.view = args.take_option("--view", "catalog");
    query.indicator_id = args.take_option("--indicator-id");
    query.query = args.take_option("--query");
    query.sort = args.take_option("--sort", "update-date");
    query.order = args.take_option("--order", "desc");
    query.include_history = !args.take_flag("--no-history");
    query.include_related = !args.take_flag("--no-related");
    query.include_quotes = !args.take_flag("--no-quotes");
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"), "limit", 1, 5000);
    query.history_limit = bounded(args.take_option("--history-limit", "2000"),
                                  "history-limit", 1, 10000);
    query.refresh = args.take_flag("--refresh");
    query.master_cache_ttl_seconds = bounded(
        args.take_option("--master-cache-ttl", "300"), "master-cache-ttl", 0, 86400);
    query.detail_cache_ttl_seconds = bounded(
        args.take_option("--detail-cache-ttl", "300"), "detail-cache-ttl", 0, 86400);
    query.quote_cache_ttl_seconds = bounded(
        args.take_option("--quote-cache-ttl", "5"), "quote-cache-ttl", 0, 3600);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    EconomicIndicatorService service(root, std::move(blocks.securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_text.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_text);
        atomic_write_text(output, rendered);
        std::cout << "completed economic indicator query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
