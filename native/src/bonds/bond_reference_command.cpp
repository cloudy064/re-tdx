#include "bond_reference_internal.hpp"

namespace tdx {
namespace fs = std::filesystem;
using namespace bond_reference_detail;

int command_market_bond_reference(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market bond-reference [options]\n\n"
            "  --group rating|rate|category --bucket ID\n"
            "  --market sz|sh|ID --code CODE --query TEXT\n"
            "  --sort maturity|rate|remaining|name|code --order asc|desc\n"
            "  --offset N --limit N --refresh --include-projections\n"
            "  --cache-ttl N --timeout-ms N\n"
            "  --root PATH --jsn-root PATH --output FILE --compact\n";
        return 0;
    }
    BondReferenceQuery query;
    query.group = args.take_option("--group", "rating");
    query.bucket = args.take_option("--bucket", "aa-plus");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.sort = args.take_option("--sort", "maturity");
    query.order = args.take_option("--order", "asc");
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"), "limit", 1, 5000);
    query.refresh = args.take_flag("--refresh");
    query.include_projections = args.take_flag("--include-projections");
    query.cache_ttl_seconds = bounded(args.take_option("--cache-ttl", "900"),
                                      "cache-ttl", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "30000"),
                               "timeout-ms", 100, 120000);
    const auto root_text = args.take_option("--root");
    const auto jsn_root_text = args.take_option("--jsn-root", "output/tdx-jsn");
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = root_text.empty() ? fs::path{} : native_path(root_text);
    const auto jsn_root = jsn_root_text.empty()
        ? fs::path{} : native_path(jsn_root_text);
    BondReferenceService service(root, jsn_root);
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_text.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_text);
        atomic_write_text(output, rendered);
        std::cout << "completed bond-reference query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
