#include "tdx/intelligence.hpp"

#include "intelligence_internal.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace intelligence_detail;
int command_market_intelligence(const std::vector<std::string> &values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market intelligence [--view "
                     "attention|value-attention|risks|highlights|events|event|graph|security|"
                     "topics|topic|news|market-anomalies] "
                     "[--category NAME] [--type TEXT] [--sort "
                     "highlight-count|safety-score|highlight-score|code] "
                     "[--order asc|desc] [--topic-id ID] [--event-id ID] [--category-id ID] "
                     "[--market sz|sh|bj --code CODE] [--output FILE]\n";
        return 0;
    }
    IntelligenceQuery query;
    query.view = lower_ascii(args.take_option("--view", "attention"));
    query.category = lower_ascii(args.take_option("--category", "all"));
    query.highlight_type = args.take_option("--type");
    query.sort = lower_ascii(args.take_option("--sort"));
    query.order = lower_ascii(args.take_option("--order", "desc"));
    query.query = args.take_option("--query");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.topic_id = args.take_option("--topic-id");
    query.event_id = args.take_option("--event-id");
    query.category_id = args.take_option("--category-id");
    query.refresh = args.take_flag("--refresh");
    query.offset = bounded(args.take_option("--offset", "0"), "--offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"), "--limit", 1, 10000);
    query.member_limit =
        bounded(args.take_option("--member-limit", "3000"), "--member-limit", 1, 20000);
    query.timeout_ms =
        bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 1, 600000);
    const auto root_text = args.take_option("--root");
    const bool compact = args.take_flag("--compact");
    const auto output =
        native_path(args.take_option("--output", "output/tdx-market-intelligence.json"));
    args.require_empty();
    IntelligenceService service(
        load_blocks(find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text)),
                    {"industry", "research-industry", "concept", "style", "index"}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("records").as_number() << " records, "
              << document.at("counts").at("risks").as_number() << " risks and "
              << document.at("counts").at("events").as_number() << " events -> "
              << path_utf8(output) << '\n';
    return 0;
}

} // namespace tdx
