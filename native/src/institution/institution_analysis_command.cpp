#include "institution_analysis_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace institution_analysis_detail;

int command_market_institution_analysis(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market institution-analysis [options]\n\n"
            "Typed TDX institution holdings, float structure and special holding views.\n\n"
            "  --root PATH           TDX installation root\n"
            "  --view ID             catalog, security, all, brokers, insurers,\n"
            "                        social-security, private-funds, public-funds,\n"
            "                        banks, finance-companies, annuities,\n"
            "                        general-corporates, qfii, trusts,\n"
            "                        special-corporates, pensions, float-structure,\n"
            "                        crowded-oversold, northbound,\n"
            "                        exclusive-funds, notable-private-funds, national-team,\n"
            "                        social-security-summary, development-bank-holdings,\n"
            "                        wutong-holdings, zhongke-huitong-holdings, stake-building\n"
            "  --market sz|sh|bj     Use with --code; required by security view\n"
            "  --code CODE           Six-digit security code\n"
            "  --query TEXT          Filter code, resolved name or normalized data\n"
            "  --offset N            Default 0\n"
            "  --limit N             Default 200; maximum 5000\n"
            "  --refresh             Bypass service cache\n"
            "  --cache-ttl N         Default 300 seconds\n"
            "  --timeout-ms N        Default 15000\n"
            "  --output FILE         Write JSON instead of stdout\n"
            "  --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    InstitutionAnalysisQuery query;
    query.view = args.take_option("--view", "catalog");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1'000'000);
    query.limit = bounded(args.take_option("--limit", "200"), "limit", 1, 5000);
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
        const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
        securities = load_blocks(root, {}).securities;
    }
    InstitutionAnalysisService service(std::move(securities));
    const auto result = service.query(query);
    const auto rendered = result.dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = from_utf8(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed institution analysis -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
