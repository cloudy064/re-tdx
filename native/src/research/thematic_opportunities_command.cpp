#include "thematic_opportunities_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

int bounded(const std::string& text, const std::string& name, int low, int high) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < low || value > high)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(low) + ".." +
                    std::to_string(high));
    }
}

}  // namespace

int command_market_thematic_opportunities(const std::vector<std::string>& raw_args) {
    using detail::thematic_opportunities::native_utf8_path;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market thematic-opportunities [options]\n\n"
            "  --view catalog|groups|group|security|hype-completed|hype-active\n"
            "  --type all|industry|region|legacy-client-theme --group-id ID\n"
            "  --market sz|sh|bj --code CODE --query TEXT\n"
            "  --sort name|members|id|type --order asc|desc\n"
            "  --no-detail --offset N --limit N --refresh\n"
            "  --master-cache-ttl N --detail-cache-ttl N --timeout-ms N\n"
            "  --root PATH --output FILE --compact\n";
        return 0;
    }
    const ThematicOpportunityQuery defaults;
    ThematicOpportunityQuery query;
    query.view = args.take_option("--view", defaults.view);
    query.type = args.take_option("--type", defaults.type);
    query.group_id = args.take_option("--group-id");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.sort = args.take_option("--sort", defaults.sort);
    query.order = args.take_option("--order", defaults.order);
    query.include_detail = !args.take_flag("--no-detail");
    query.offset = bounded(args.take_option("--offset", std::to_string(defaults.offset)),
                           "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", std::to_string(defaults.limit)),
                          "limit", 1, 5000);
    query.refresh = args.take_flag("--refresh");
    query.master_cache_ttl_seconds = bounded(
        args.take_option("--master-cache-ttl",
                         std::to_string(defaults.master_cache_ttl_seconds)),
        "master-cache-ttl", 0, 86400);
    query.detail_cache_ttl_seconds = bounded(
        args.take_option("--detail-cache-ttl",
                         std::to_string(defaults.detail_cache_ttl_seconds)),
        "detail-cache-ttl", 0, 86400);
    query.timeout_ms = bounded(
        args.take_option("--timeout-ms", std::to_string(defaults.timeout_ms)),
        "timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_utf8_path(root_text));
    auto blocks = load_blocks(root, {});
    ThematicOpportunityService service(std::move(blocks.securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + "\n";
    if (output_text.empty()) std::cout << rendered;
    else {
        const auto output = native_utf8_path(output_text);
        atomic_write_text(output, rendered);
        std::cout << "completed thematic opportunity query -> " <<
            path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
