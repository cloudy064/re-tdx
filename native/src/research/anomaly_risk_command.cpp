#include "anomaly_risk_internal.hpp"

#include "tdx/blocks.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_anomaly_risk(const std::vector<std::string>& values) {
    using namespace detail::anomaly_risk;
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market anomaly-risk [options]\n\n"
            "Views:\n"
            "  statistics       3/10/30-day security vs classification-index statistics\n"
            "  suspension-risk Possible suspension/re-suspension and triggered anomalies\n\n"
            "Options:\n"
            "  --view statistics|suspension-risk --query TEXT\n"
            "  --market sz|sh|bj --code CODE --warnings-only\n"
            "  --warning all|none|repeat-suspension|possible-suspension|triggered\n"
            "  --root PATH --limit N --refresh --cache-ttl-seconds N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    AnomalyRiskQuery query;
    query.view = args.take_option("--view", "statistics");
    query.query = args.take_option("--query");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.warning = args.take_option("--warning", "all");
    query.warnings_only = args.take_flag("--warnings-only");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"),
                          "--limit", 1, 10000);
    query.cache_ttl_seconds = bounded(
        args.take_option("--cache-ttl-seconds", "300"),
        "--cache-ttl-seconds", 0, 3600);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-anomaly-risk.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    AnomalyRiskService service(root, load_blocks(root, {}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("returned").as_number()
              << " anomaly risk rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
