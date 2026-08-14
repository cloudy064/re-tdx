#include "forecasts_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_market_forecasts(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market forecasts [options]\n\n"
            "Native industry and security earnings forecasts.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             industries|securities|latest|hong-kong\n"
            "  --category NAME         all|positive|negative|uncertain\n"
            "  --query TEXT            Filter returned rows\n"
            "  --industry 881XXX       Select first-level research industry\n"
            "  --market sz|sh|bj       Select a mainland security with --code\n"
            "  --code CODE             Six-digit mainland security code\n"
            "  --report-period DATE    YYYYMMDD report period\n"
            "  --details               Fetch selected industry detail\n"
            "  --limit N               Master rows, default 1000\n"
            "  --detail-limit N        Detail rows, default 5000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-forecasts-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    using namespace forecast_detail;
    const auto root_text = args.take_option("--root");
    ForecastQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "industries")));
    query.category = lower_ascii(trim(args.take_option("--category", "all")));
    query.query = trim(args.take_option("--query"));
    query.industry = trim(args.take_option("--industry"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.report_period = trim(args.take_option("--report-period"));
    query.include_details = args.take_flag("--details");
    query.limit = bounded_integer(
        args.take_option("--limit", "1000"), "--limit", 1, 5000);
    query.detail_limit = bounded_integer(
        args.take_option("--detail-limit", "5000"),
        "--detail-limit", 1, 10000);
    query.timeout_ms = bounded_integer(
        args.take_option("--timeout-ms", "15000"),
        "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-forecasts-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    ForecastService service(load_blocks(root, {"research-industry"}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("view").as_string()
              << " forecast query -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
