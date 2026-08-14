#include "state_owned_reform_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_state_owned_reform(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market state-owned-reform [options]\n\n"
            "  --view groups|group|security|restructuring|catalog\n"
            "  --dimension industry|region|integration|company --group-id ID\n"
            "  --market sz|sh|bj --code CODE --query TEXT\n"
            "  --sort count|name|id (groups), date|profit|control|code (restructuring)\n"
            "  --order asc|desc --no-details --no-quotes --offset N --limit N\n"
            "  --detail-limit N --refresh --master-cache-ttl N --detail-cache-ttl N\n"
            "  --quote-cache-ttl N --timeout-ms N --root PATH --output FILE --compact\n";
        return 0;
    }
    using namespace state_owned_detail;
    StateOwnedReformQuery query;
    query.view = args.take_option("--view", "groups");
    query.dimension = args.take_option("--dimension", "industry");
    query.group_id = args.take_option("--group-id");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.sort = args.take_option(
        "--sort", query.view == "restructuring" ? "date" : "count");
    query.order = args.take_option("--order", "desc");
    query.include_details = !args.take_flag("--no-details");
    query.include_quotes = !args.take_flag("--no-quotes");
    query.offset = bounded(
        args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(
        args.take_option("--limit", "500"), "limit", 1, 5000);
    query.detail_limit = bounded(
        args.take_option("--detail-limit", "500"),
        "detail-limit", 1, 5000);
    query.refresh = args.take_flag("--refresh");
    query.master_cache_ttl_seconds = bounded(
        args.take_option("--master-cache-ttl", "300"),
        "master-cache-ttl", 0, 86400);
    query.detail_cache_ttl_seconds = bounded(
        args.take_option("--detail-cache-ttl", "300"),
        "detail-cache-ttl", 0, 86400);
    query.quote_cache_ttl_seconds = bounded(
        args.take_option("--quote-cache-ttl", "5"),
        "quote-cache-ttl", 0, 3600);
    query.timeout_ms = bounded(
        args.take_option("--timeout-ms", "15000"),
        "timeout-ms", 100, 60000);
    const auto root_name = args.take_option("--root");
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_name.empty() ? fs::path{} : native_path(root_name));
    auto blocks = load_blocks(root, {});
    StateOwnedReformService service(root, std::move(blocks.securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) {
        std::cout << rendered;
    } else {
        const auto path = native_path(output_name);
        atomic_write_text(path, rendered);
        std::cout << "completed state-owned reform query -> "
                  << path_utf8(path) << '\n';
    }
    return 0;
}

}  // namespace tdx
