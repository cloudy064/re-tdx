#include "flow_followup_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace flow_followup_detail;

int command_market_flow_followup(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market flow-followup [options]\n\n"
            "Views:\n"
            "  margin                    Margin daily history (default)\n"
            "  northbound                Northbound daily history\n"
            "  financing-model           Financing-rate buckets and current summary\n"
            "  lending-model             Securities-lending-rate model\n"
            "  northbound-inflow-model   Net-inflow buckets and current summary\n"
            "  northbound-purchase-model Net-purchase buckets and current summary\n\n"
            "Options:\n"
            "  --view VIEW --start DATE --end DATE\n"
            "  --available-only             History views only; exclude placeholders\n"
            "  --limit N --refresh --cache-ttl-seconds N --timeout-ms N\n"
            "  --root PATH --output FILE --compact\n";
        return 0;
    }
    FlowFollowupQuery query;
    query.view = args.take_option("--view", "margin");
    query.start_date = trim(args.take_option("--start"));
    query.end_date = trim(args.take_option("--end"));
    query.available_only = args.take_flag("--available-only");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"), "--limit", 1, 20000);
    query.cache_ttl_seconds = bounded(
        args.take_option("--cache-ttl-seconds", "300"),
        "--cache-ttl-seconds", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "30000"),
                               "--timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-flow-followup.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    FlowFollowupService service(root);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("returned").as_number()
              << " flow follow-up rows -> " << path_utf8(output) << '\n';
    return 0;
}


}  // namespace tdx

