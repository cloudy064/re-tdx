#include "fund_analytics_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace tdx {
namespace fund_detail = fund_analytics_detail;

int command_market_fund_analytics(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market fund-analytics [options]\n\n"
            "  --view risk|risk-history|monthly-risk|monthly-history|selection-skill\n"
            "         reported-holdings|reported-holding-industries|reported-holding-securities\n"
            "         holdings-stability|holding-industries|holding-history\n"
            "         position-estimates|market-position-history\n"
            "  --fund-code CODE              Required for detail views\n"
            "  --query TEXT --style CODE --fund-size 0..6 --fund-age 0..6\n"
            "  --benchmark 0..2 --start DATE --end DATE --report-date DATE --estimate-date DATE\n"
            "  --risk-free-rate PCT --first-page --root PATH --limit N --refresh\n"
            "  --cache-ttl-seconds N --timeout-ms N --output FILE --compact\n";
        return 0;
    }
    FundAnalyticsQuery query;
    query.view = args.take_option("--view", "risk");
    query.query = args.take_option("--query");
    query.fund_code = args.take_option("--fund-code");
    query.style = args.take_option("--style", "005001");
    query.fund_size = fund_detail::integer(
        args.take_option("--fund-size", "0"), "--fund-size", 0, 6);
    query.fund_age = fund_detail::integer(
        args.take_option("--fund-age", "0"), "--fund-age", 0, 6);
    query.benchmark = fund_detail::integer(
        args.take_option("--benchmark", "0"), "--benchmark", 0, 2);
    query.start_date = args.take_option("--start");
    query.end_date = args.take_option("--end");
    query.report_date = args.take_option("--report-date");
    query.estimate_date = args.take_option("--estimate-date");
    const auto risk_free = args.take_option("--risk-free-rate", "3");
    try {
        std::size_t used = 0;
        query.risk_free_rate = std::stod(risk_free, &used);
        if (used != risk_free.size() || !std::isfinite(query.risk_free_rate))
            throw std::invalid_argument("value");
    } catch (...) {
        throw Error("--risk-free-rate must be numeric");
    }
    query.all_pages = !args.take_flag("--first-page");
    query.refresh = args.take_flag("--refresh");
    query.limit = fund_detail::integer(
        args.take_option("--limit", "10000"), "--limit", 1, 20000);
    query.max_pages = fund_detail::integer(
        args.take_option("--max-pages", "100"), "--max-pages", 1, 100);
    query.cache_ttl_seconds = fund_detail::integer(
        args.take_option("--cache-ttl-seconds", "300"),
        "--cache-ttl-seconds", 0, 3600);
    query.timeout_ms = fund_detail::integer(
        args.take_option("--timeout-ms", "15000"),
        "--timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output = fund_detail::native_path(args.take_option(
        "--output", "output/tdx-fund-analytics.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : fund_detail::native_path(root_text));
    FundAnalyticsService service(root);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received "
              << document.at("counts").at("returned").as_number()
              << " fund analytics rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
