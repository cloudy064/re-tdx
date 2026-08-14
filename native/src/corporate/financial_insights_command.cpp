#include "financial_insights_internal.hpp"

#include "tdx/blocks.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct FinancialInsightsCommandDefaults {
    static constexpr const char* view = "all";
    static constexpr const char* sort = "signal";
    static constexpr const char* order = "desc";
    static constexpr const char* limit = "5000";
    static constexpr const char* timeout_ms = "15000";
    static constexpr const char* input_dir = "output/tdx-jsn";
    static constexpr const char* output =
        "output/tdx-market-financial-insights-native.json";
};

int bounded(const std::string& value, std::string_view name, int low, int high) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < low || parsed > high)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(low) + ".." + std::to_string(high));
    }
}

}  // namespace

int command_market_financial_insights(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market financial-insights [options]\n\n"
            "  --view all|buffett-quality|high-bonus-potential|investment-property|low-price-sales\n"
            "         dividend-shortfall|equity-investment|cash-above-market-cap|high-receivables\n"
            "         profit-warning|cash-flow-quality|earnings-reversal|steady-growth\n"
            "         quality-growth|profit-breakout|dividend-plan\n"
            "  --sort signal|amount|ratio|pe|roe|code --order asc|desc\n"
            "  --query TEXT --market sz|sh|bj --code CODE --without-raw --refresh\n"
            "  --root PATH --input-dir PATH --limit N --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    FinancialInsightsQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", FinancialInsightsCommandDefaults::view)));
    query.sort = lower_ascii(trim(args.take_option("--sort", FinancialInsightsCommandDefaults::sort)));
    query.order = lower_ascii(trim(args.take_option("--order", FinancialInsightsCommandDefaults::order)));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.include_raw = !args.take_flag("--without-raw");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", FinancialInsightsCommandDefaults::limit), "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", FinancialInsightsCommandDefaults::timeout_ms),
                               "--timeout-ms", 100, 60000);
    const auto input = detail::financial_insights::native_utf8_path(
        args.take_option("--input-dir", FinancialInsightsCommandDefaults::input_dir));
    const auto output = detail::financial_insights::native_utf8_path(args.take_option(
        "--output", FinancialInsightsCommandDefaults::output));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} :
        detail::financial_insights::native_utf8_path(root_text));
    auto blocks = load_blocks(root, {});
    FinancialInsightsService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " financial-insight rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
