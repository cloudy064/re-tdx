#include "tdx/theme_library_internal.hpp"

#include "tdx/common.hpp"

#include <iostream>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {

using namespace tdx::theme_library_detail;

int command_market_theme_library(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market theme-library [options]\n\n"
            "  --view catalog|theme|security --source general|region|state-owned|company|holdings|all\n"
            "  --theme-id ID --market sz|sh|bj --code CODE --query TEXT\n"
            "  --sort created|name|members|id --order asc|desc\n"
            "  --no-detail --no-chart --offset N --limit N --refresh\n"
            "  --master-cache-ttl N --detail-cache-ttl N --timeout-ms N\n"
            "  --root PATH --output FILE --compact\n";
        return 0;
    }
    ThemeLibraryQuery query;
    query.view = args.take_option("--view", "catalog");
    query.source = args.take_option("--source", "general");
    query.theme_id = args.take_option("--theme-id");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.sort = args.take_option("--sort", "created");
    query.order = args.take_option("--order", "desc");
    query.include_detail = !args.take_flag("--no-detail");
    query.include_chart = !args.take_flag("--no-chart");
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"), "limit", 1, 5000);
    query.refresh = args.take_flag("--refresh");
    query.master_cache_ttl_seconds = bounded(
        args.take_option("--master-cache-ttl", "900"), "master-cache-ttl", 0, 86400);
    query.detail_cache_ttl_seconds = bounded(
        args.take_option("--detail-cache-ttl", "300"), "detail-cache-ttl", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    ThemeLibraryService service(root, std::move(blocks.securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_text.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_text);
        atomic_write_text(output, rendered);
        std::cout << "completed theme-library query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
