#include "special_situations_internal.hpp"

namespace tdx {
namespace fs = std::filesystem;
using namespace special_situations_detail;

int command_market_special_situations(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market special-situations "
                     "[--view all|legacy|mergers|b-to-h|market-cap-risk|"
                     "corporate-actions|neeq-transfers|neeq-regulation] "
                     "[--query TEXT] [--market MARKET --code CODE] "
                     "[--root PATH] [--input-dir PATH] [--no-quotes] [--refresh] "
                     "[--limit N] [--output PATH] [--compact]\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    SpecialSituationQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.include_quotes = !args.take_flag("--no-quotes");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"),
                          "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option(
        "--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-special-situations-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    SpecialSituationService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " special-situation rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
