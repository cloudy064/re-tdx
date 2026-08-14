#include "equity_valuation_internal.hpp"

#include "tdx/blocks.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_equity_valuation(const std::vector<std::string>& values) {
    using namespace detail::equity_valuation;
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market equity-valuation [options]\n\n"
            "Views:\n"
            "  pe-industries             Industry PE overview (default)\n"
            "  pe-security-history       One security's PE history and percentiles\n"
            "  pe-industry-members       Industry members' PE and consensus valuation\n"
            "  pb-roe-industries         Industry PB/ROE overview\n"
            "  pb-roe-members            Industry members' PB/ROE valuation\n\n"
            "Options:\n"
            "  --view NAME --market sz|sh|bj --code CODE\n"
            "  --industry CODE --industry-market N\n"
            "  --start DATE --end DATE --pe-type ttm|annual\n"
            "  --required-return-rate PCT --limit N\n"
            "  --refresh --cache-ttl-seconds N --timeout-ms N\n"
            "  --root PATH --output FILE --compact\n";
        return 0;
    }
    EquityValuationQuery query;
    query.view = args.take_option("--view", "pe-industries");
    query.market = args.take_option("--market");
    query.code = trim(args.take_option("--code"));
    query.industry_code = trim(args.take_option("--industry"));
    query.industry_market = trim(args.take_option("--industry-market", "1"));
    query.start_date = trim(args.take_option("--start"));
    query.end_date = trim(args.take_option("--end"));
    query.pe_type = args.take_option("--pe-type", "ttm");
    query.required_return_rate_pct = decimal_value(
        args.take_option("--required-return-rate", "3"),
        "--required-return-rate", 0, 100);
    query.refresh = args.take_flag("--refresh");
    query.limit = integer_value(args.take_option("--limit", "4000"),
                                "--limit", 1, 20000);
    query.cache_ttl_seconds = integer_value(
        args.take_option("--cache-ttl-seconds", "300"),
        "--cache-ttl-seconds", 0, 86400);
    query.timeout_ms = integer_value(args.take_option("--timeout-ms", "15000"),
                                     "--timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-equity-valuation.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    EquityValuationService service(root, load_blocks(
        root, {"industry", "research-industry", "concept", "style", "index"}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("returned").as_number()
              << " equity valuation rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
