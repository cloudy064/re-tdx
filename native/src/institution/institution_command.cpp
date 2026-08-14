#include "institution_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace institution_detail;

int command_market_institution(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market institution [options]\n\n"
            "Native security institution and shareholder cross-stock aggregation.\n\n"
            "Security mode:\n"
            "  --market sz|sh|bj       Security market\n"
            "  --code CODE             Fetch institution history and top holders\n\n"
            "Holder mode:\n"
            "  --holder-id ID          Holder identifier from gdjc URL\n"
            "  --variant-id ID         Optional tdxid variant\n"
            "  --holder-name NAME      Optional display name\n"
            "  --reference-code CODE   Seed stock used by gdjc\n"
            "  --stock-code CODE       Expand report periods for one related stock\n"
            "  --offset N              Cross-stock offset, default 0\n"
            "  --limit N               Default 100, maximum 500\n\n"
            "Cache migration mode:\n"
            "  --cache-import FILE     Import one complete paginated API snapshot set; repeat\n"
            "  --cache-root PATH       Persistent holder cache directory\n\n"
            "Common options:\n"
            "  --root PATH             TDX installation root\n"
            "  --timeout-ms N          Default 15000\n"
            "  --cache-root PATH       Optional persistent holder cache directory\n"
            "  --output PATH           Default output/tdx-institution-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto cache_imports = args.take_options("--cache-import");
    const auto market = trim(args.take_option("--market"));
    const auto code = trim(args.take_option("--code"));
    HolderQuery holder;
    holder.holder_id = trim(args.take_option("--holder-id"));
    holder.variant_id = trim(args.take_option("--variant-id"));
    holder.holder_name = trim(args.take_option("--holder-name"));
    holder.reference_code = trim(args.take_option("--reference-code"));
    holder.stock_code = trim(args.take_option("--stock-code"));
    holder.offset = bounded_integer(args.take_option("--offset", "0"), "--offset", 0, 1000000);
    holder.limit = bounded_integer(args.take_option("--limit", "100"), "--limit", 1, 500);
    holder.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                        "--timeout-ms", 100, 60000);
    const auto cache_root_text = args.take_option("--cache-root");
    holder.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-institution-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const bool holder_mode = !holder.holder_id.empty();
    if (!cache_imports.empty()) {
        if (holder_mode || !market.empty() || !code.empty())
            throw Error("cache migration cannot be combined with a live query");
        const auto document = import_holder_cache_snapshots(
            cache_imports, cache_root_text.empty() ? fs::path{} : native_path(cache_root_text));
        atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
        std::cout << "imported holder cache snapshots -> " << path_utf8(output) << '\n';
        return 0;
    }
    if (holder_mode && (!market.empty() || !code.empty()))
        throw Error("choose security mode or holder mode, not both");
    Json document;
    if (holder_mode) {
        InstitutionService service({}, {}, cache_root_text.empty()
            ? fs::path{} : native_path(cache_root_text));
        document = service.query_holder(holder);
    } else {
        if (market.empty() || code.empty()) throw Error("security mode requires --market and --code");
        const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
        InstitutionService service(load_blocks(root, {}).securities);
        InstitutionQuery query{market, code, true, 300, holder.timeout_ms};
        document = service.query_security(query);
    }
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed native institution query -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
