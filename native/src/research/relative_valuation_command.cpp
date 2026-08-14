#include "relative_valuation_internal.hpp"

#include "tdx/blocks.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_relative_valuation(const std::vector<std::string>& values) {
    using namespace detail::relative_valuation;
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market relative-valuation [options]\n\n"
            "Index valuation ratio relative to a selected benchmark.\n\n"
            "Options:\n"
            "  --code CODE             Expand one index's daily ratio history\n"
            "  --index-type TYPE       broad, industry, composite, theme, scale\n"
            "  --benchmark CODE        000001, 000300, 000016, 000905, 399006\n"
            "  --method METHOD         pe-ttm, pb-mrq, ps-ttm\n"
            "  --start DATE --end DATE Default latest two years\n"
            "  --limit N --refresh --cache-ttl-seconds N --timeout-ms N\n"
            "  --root PATH --output FILE --compact\n";
        return 0;
    }
    RelativeValuationQuery query;
    query.code = trim(args.take_option("--code"));
    query.index_type = trim(args.take_option("--index-type", "broad"));
    query.benchmark = trim(args.take_option("--benchmark", "000001"));
    query.method = trim(args.take_option("--method", "pe-ttm"));
    query.start_date = trim(args.take_option("--start"));
    query.end_date = trim(args.take_option("--end"));
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "4000"),
                          "--limit", 1, 20000);
    query.cache_ttl_seconds = bounded(
        args.take_option("--cache-ttl-seconds", "300"),
        "--cache-ttl-seconds", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-relative-valuation.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    RelativeValuationService service(root, load_blocks(root, {}).securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("indices").as_number()
              << " relative-valuation indices and "
              << document.at("counts").at("returned_history_points").as_number()
              << " history points -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
