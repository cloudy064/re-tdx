#include "factors_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace factor_detail;
int command_market_factors(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market factors --view VIEW [options]\n\n"
            "TDX factor catalog, factor-to-security membership, dashboard and intraday radar.\n\n"
            "Views: catalog, members, dashboard, patterns, pattern-members, intraday-radar,\n"
            "       security, standard-matrix, pattern-matrix, overview\n\n"
            "Options:\n"
            "  --factor-id ID         Required by members and pattern-members\n"
            "  --market MARKET --code CODE  Required by security\n"
            "  --include-patterns      Build all 57 pattern lists for security reverse lookup\n"
            "  --query TEXT           Filter normalized rows\n"
            "  --first-page           Reproduce the TDX UI first-page limit\n"
            "  --quotes               Add public 0x054C L1 snapshots to security rows\n"
            "  --snapshot FILE        Compare and atomically update this result snapshot\n"
            "  --limit N --max-pages N --refresh\n"
            "  --cache-ttl-seconds N --timeout-ms N\n"
            "  --root PATH --output FILE --compact\n";
        return 0;
    }
    FactorQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "catalog")));
    query.factor_id = trim(args.take_option("--factor-id"));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.include_patterns = args.take_flag("--include-patterns");
    query.all_pages = !args.take_flag("--first-page");
    query.enrich_quotes = args.take_flag("--quotes");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "10000"), "--limit", 1, 30000);
    query.max_pages = bounded(args.take_option("--max-pages", "100"), "--max-pages", 1, 1000);
    query.cache_ttl_seconds = bounded(
        args.take_option("--cache-ttl-seconds", "60"),
        "--cache-ttl-seconds", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto snapshot_text = args.take_option("--snapshot");
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-factors.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    FactorService service(root, load_blocks(root, {}));
    auto document = service.query(query);
    if (!snapshot_text.empty()) {
        const auto snapshot = native_path(snapshot_text);
        if (fs::absolute(snapshot).lexically_normal() ==
            fs::absolute(output).lexically_normal())
            throw Error("--snapshot and --output must use different files");
        document["snapshot"] = update_factor_snapshot(snapshot, document);
    }
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("returned").as_number()
              << " factor rows for " << query.view << " -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx