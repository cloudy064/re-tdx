#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {

using namespace convertible_bond_detail;

int command_market_convertible_bonds(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market convertible-bonds [options]\n"
                     "Options: --root PATH, --view listed|pending|subscriptions|pricing, --query TEXT, "
                     "--market sz|sh|bj, --code CODE, --sort progress-date|issue-size|"
                     "stock-rights|conversion-price|subscription-date|double-low|premium|"
                     "ytm|pure-bond|price|remaining-years|return|listing-date|premium|lottery-rate, --order asc|desc, "
                     "--limit N, --timeout-ms N, --no-details, --no-quotes, "
                     "--include-expired, --output PATH, --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ConvertibleBondQuery query;
    query.view = trim(args.take_option("--view", "listed"));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.sort = trim(args.take_option("--sort"));
    query.order = trim(args.take_option("--order", "desc"));
    query.limit = bounded(args.take_option("--limit", "2000"), "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000);
    query.include_details = !args.take_flag("--no-details");
    query.include_quotes = !args.take_flag("--no-quotes");
    query.active_only = !args.take_flag("--include-expired");
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-convertible-bonds-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    ConvertibleBondService service(root, load_blocks(root, {}).securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << static_cast<std::uint64_t>(document.at("returned").as_number())
              << (lower_ascii(query.view) == "pending" ? " pending convertible-bond issues -> "
                  : lower_ascii(query.view) == "subscriptions" ? " convertible-bond subscriptions -> "
                  : lower_ascii(query.view) == "pricing" ? " convertible-bond valuations -> "
                                                         : " convertible bonds -> ")
              << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
