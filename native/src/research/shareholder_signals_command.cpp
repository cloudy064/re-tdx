#include "shareholder_signals_internal.hpp"

#include "tdx/blocks.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_shareholder_signals(const std::vector<std::string>& raw_args) {
    using namespace detail::shareholder_signals;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market shareholder-signals [options]\n\n"
            "  --view all|notable-investors|institution-accumulation|research-growth|small-cap-institution|investor-directory\n"
            "  --sort signal|holding-value|institution-growth|holder-change|research-6m|profit-growth|return-6m|code\n"
            "  --order asc|desc --query TEXT --market sz|sh|bj --code CODE\n"
            "  --investor ID --investor-query TEXT\n"
            "  --without-raw --refresh --root PATH --input-dir PATH --limit N\n"
            "  --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ShareholderSignalsQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.sort = lower_ascii(trim(args.take_option("--sort", "signal")));
    query.order = lower_ascii(trim(args.take_option("--order", "desc")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.investor_id = trim(args.take_option("--investor"));
    query.investor_query = trim(args.take_option("--investor-query"));
    query.include_raw = !args.take_flag("--without-raw");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"),
                          "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(
        args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-shareholder-signals-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    ShareholderSignalsService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " shareholder-signal rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
