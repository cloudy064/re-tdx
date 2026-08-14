#include "limit_review_internal.hpp"

#include "tdx/blocks.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_limit_review(const std::vector<std::string>& raw_args) {
    using namespace detail::limit_review;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market limit-review [options]\n\n"
            "Typed TDX non-realtime limit-up/down review and history.\n\n"
            "  --root PATH           TDX installation root\n"
            "  --view ID             catalog|current|annual|history|daily|security\n"
            "  --category ID         View-specific category, default all\n"
            "  --market sz|sh|bj     Optional filter; required by security\n"
            "  --code CODE           Optional six-digit filter\n"
            "  --date YYYYMMDD        Required by daily\n"
            "  --query TEXT          Filter normalized row content\n"
            "  --offset N            Default 0\n"
            "  --limit N             Default 200, maximum 5000\n"
            "  --refresh             Bypass service cache\n"
            "  --cache-ttl N         Default 300 seconds\n"
            "  --timeout-ms N        Default 15000\n"
            "  --output FILE         Write JSON instead of stdout\n"
            "  --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    LimitReviewQuery query;
    query.view = args.take_option("--view", "catalog");
    query.category = args.take_option("--category", "all");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.date = args.take_option("--date");
    query.query = args.take_option("--query");
    query.offset = bounded(args.take_option("--offset", "0"),
                           "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "200"),
                          "limit", 1, 5000);
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
            root_text.empty() ? fs::path{} : from_utf8(root_text));
        securities = load_blocks(root, {}).securities;
    }
    LimitReviewService service(std::move(securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) {
        std::cout << rendered;
    } else {
        const auto output = from_utf8(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed limit review -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
