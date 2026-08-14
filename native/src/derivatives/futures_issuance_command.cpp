#include "futures_issuance_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using namespace futures_issuance_detail;

int command_market_futures_issuance(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market futures-issuance [options]\n\n"
            "Native futures statistics, IPO/bond issuers, placements, rights and preferred shares.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --section all|futures|ipo|placements|rights|preferred-shares (default all)\n"
            "  --contract-key KEY      Expand qhtj1/qhtj2 contract details\n"
            "  --year YYYY             Expand IPO industry/month series\n"
            "  --industry-key KEY      Expand IPO stocks for an industry key\n"
            "  --placement-status NAME all|locked|unlocked|active|stopped|implemented|registered\n"
            "  --market sz|sh|bj       Filter an issuance record by underlying security\n"
            "  --code CODE             Filter an issuance record by underlying security\n"
            "  --q TEXT                Search issuance code, name, stage, method, or details\n"
            "  --limit N               Rows per section, default 500\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-futures-issuance-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    FuturesIssuanceQuery query;
    query.section = lower_ascii(trim(args.take_option("--section", "all")));
    query.contract_key = trim(args.take_option("--contract-key"));
    query.year = trim(args.take_option("--year"));
    query.industry_key = trim(args.take_option("--industry-key"));
    query.placement_status = lower_ascii(trim(args.take_option("--placement-status", "all")));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.search = trim(args.take_option("--q"));
    query.limit = bounded_integer(args.take_option("--limit", "500"), "--limit", 1, 5000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-futures-issuance-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    FuturesIssuanceService service(load_blocks(root, {}).securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed futures/issuance query with "
              << document.at("sources").size() << " resources -> " << path_utf8(output) << '\n';
    return 0;
}


}  // namespace tdx

