#include "leverage_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
using namespace leverage_detail;

int command_market_margin(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market margin [--view market|transfer|ranking|security|classifications|classification-history] "
                     "[--category NAME] [--market sz|sh|bj --code CODE] "
                     "[--date YYYYMMDD] [--group-id ID] [--output FILE]\n";
        return 0;
    }
    MarginQuery query; query.view = lower_ascii(args.take_option("--view", "market"));
    query.category = lower_ascii(args.take_option("--category", "balance"));
    query.query = args.take_option("--query"); query.market = args.take_option("--market");
    query.code = args.take_option("--code"); query.refresh = args.take_flag("--refresh");
    query.date = args.take_option("--date"); query.group_id = args.take_option("--group-id");
    query.limit = bounded(args.take_option("--limit", "2000"), "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 1, 600000);
    const auto root_text = args.take_option("--root"); const bool compact = args.take_flag("--compact");
    const auto output = native_path(args.take_option("--output", "output/tdx-market-margin.json"));
    args.require_empty();
    LeverageService service(load_blocks(find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text)),
                                        {"industry", "research-industry", "concept", "style", "index"}));
    const auto document = service.query_margin(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("count").as_number() << " margin rows -> " << path_utf8(output) << '\n';
    return 0;
}

int command_market_stock_connect(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market stock-connect [options]\n\n"
            "  --view flows|holdings|security|activity|industry|industry-detail|active-stocks\n"
            "  --category NAME        Flow, holding, activity or industry category\n"
            "  --market sz|sh|hk      Security history market\n"
            "  --code CODE            Security history code\n"
            "  --date YYYYMMDD        Active-stock trading date\n"
            "  --channel NAME         sh-northbound or sz-northbound\n"
            "  --group-id ID          Southbound industry detail key, for example 70HK0201\n"
            "  --query TEXT --limit N --refresh --root PATH --output FILE\n";
        return 0;
    }
    StockConnectQuery query; query.view = lower_ascii(args.take_option("--view", "flows"));
    query.category = lower_ascii(args.take_option("--category", "northbound-total"));
    query.query = args.take_option("--query"); query.market = args.take_option("--market");
    query.code = args.take_option("--code"); query.date = args.take_option("--date");
    query.channel = args.take_option("--channel"); query.refresh = args.take_flag("--refresh");
    query.group_id = args.take_option("--group-id");
    query.limit = bounded(args.take_option("--limit", "3000"), "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 1, 600000);
    const auto root_text = args.take_option("--root"); const bool compact = args.take_flag("--compact");
    const auto output = native_path(args.take_option("--output", "output/tdx-market-stock-connect.json"));
    args.require_empty();
    LeverageService service(load_blocks(find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text)),
                                        {"industry", "research-industry", "concept", "style", "index"}));
    const auto document = service.query_stock_connect(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("count").as_number() << " stock-connect rows -> " << path_utf8(output) << '\n';
    return 0;
}


} // namespace tdx

