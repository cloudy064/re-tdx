#include "strategic_themes_internal.hpp"

#include "tdx/blocks.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_strategic_themes(const std::vector<std::string>& raw_args) {
    using namespace detail::strategic_themes;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market strategic-themes [options]\n\n"
            "  --view catalog|categories|themes|theme|security\n"
            "  --category ID_OR_NAME --theme-id ID --market sz|sh|bj --code CODE\n"
            "  --query TEXT --sort name|members|id --order asc|desc\n"
            "  --no-detail --offset N --limit N --refresh\n"
            "  --master-cache-ttl N --detail-cache-ttl N --timeout-ms N\n"
            "  --root PATH --output FILE --compact\n";
        return 0;
    }
    StrategicThemeQuery query;
    query.view = args.take_option("--view", "categories");
    query.category = args.take_option("--category");
    query.theme_id = args.take_option("--theme-id");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.sort = args.take_option("--sort", "name");
    query.order = args.take_option("--order", "asc");
    query.include_detail = !args.take_flag("--no-detail");
    query.offset = bounded(args.take_option("--offset", "0"),
                           "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"),
                          "limit", 1, 5000);
    query.refresh = args.take_flag("--refresh");
    query.master_cache_ttl_seconds = bounded(
        args.take_option("--master-cache-ttl", "900"),
        "master-cache-ttl", 0, 86400);
    query.detail_cache_ttl_seconds = bounded(
        args.take_option("--detail-cache-ttl", "300"),
        "detail-cache-ttl", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    StrategicThemeService service(root, std::move(blocks.securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_text.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_text);
        atomic_write_text(output, rendered);
        std::cout << "completed strategic theme query -> "
                  << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
