#include "tdx/investment.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

double positive_number(const std::string& value, std::string_view name) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        if (used != value.size() || !std::isfinite(parsed) || parsed <= 0.0)
            throw std::invalid_argument("number");
        return parsed;
    } catch (...) {
        throw Error(std::string(name) + " must be a positive finite number");
    }
}

std::uint64_t positive_integer(const std::string& value,
                               std::string_view name) {
    try {
        if (value.empty() || value.front() == '-' || value.front() == '+')
            throw std::invalid_argument("integer");
        std::size_t used = 0;
        const auto parsed = std::stoull(value, &used);
        if (used != value.size() || parsed == 0)
            throw std::invalid_argument("integer");
        return parsed;
    } catch (...) {
        throw Error(std::string(name) + " must be a positive integer");
    }
}

std::size_t bounded_integer(const std::string& value, std::string_view name,
                            std::size_t minimum, std::size_t maximum) {
    try {
        if (value.empty() || value.front() == '-' || value.front() == '+')
            throw std::invalid_argument("integer");
        std::size_t used = 0;
        const auto parsed = std::stoull(value, &used);
        if (used != value.size() || parsed < minimum || parsed > maximum)
            throw std::invalid_argument("integer");
        return static_cast<std::size_t>(parsed);
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

}  // namespace

int command_market_investment(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market investment [options]\n\n"
            "Read local invest.dll portfolio diagnostics and transaction fee rules.\n\n"
            "Options:\n"
            "  --view NAME          summary|portfolios|transactions|holdings|valuation|fee-rules\n"
            "  --root PATH          TDX installation root\n"
            "  --private-dir PATH   Override [OTHER]/INVESTPATH or T0002/invest\n"
            "  --trdpara PATH       Override T0002/trdpara.dat\n"
            "  --portfolio NAME     Exact portfolio for private portfolio views\n"
            "  --offset N           Transaction record offset (default 0)\n"
            "  --limit N            Transaction page size, 1..10000 (default 1000)\n"
            "  --include-notes      Emit private transaction note text\n"
            "  --include-closed     Include zero-quantity holdings\n"
            "  --quotes PATH        Offline market snapshot for valuation\n"
            "  --timeout-ms N       Live valuation quote timeout (default 10000)\n"
            "  --market MARKET      sz|sh|bj or 0|1|2 for fee-rule selection\n"
            "  --code CODE          Six-digit security code for rule selection\n"
            "  --price VALUE        Trade price for a fee quote\n"
            "  --quantity N         Trade quantity for a fee quote\n"
            "  --side SIDE          buy|sell for a fee quote\n"
            "  --output PATH        Default output/tdx-market-investment.json\n"
            "  --compact            Write compact JSON\n\n"
            "Valuation uses an offline --quotes snapshot or public L1 0x054C quotes.\n"
            "Passwords and unknown private bytes are never emitted. Private views require\n"
            "an explicit portfolio and remain CLI-only; transaction notes are opt-in.\n";
        return 0;
    }
    InvestmentQuery query;
    query.view = args.take_option("--view", "summary");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.side = args.take_option("--side");
    query.portfolio_name = args.take_option("--portfolio");
    query.include_notes = args.take_flag("--include-notes");
    query.include_closed = args.take_flag("--include-closed");
    if (args.has("--offset"))
        query.offset = bounded_integer(
            args.take_option("--offset"), "--offset", 0, 100000000);
    if (args.has("--limit"))
        query.limit = bounded_integer(
            args.take_option("--limit"), "--limit", 1, 10000);
    if (args.has("--price"))
        query.price = positive_number(args.take_option("--price"), "--price");
    if (args.has("--quantity"))
        query.quantity = positive_integer(
            args.take_option("--quantity"), "--quantity");
    if (args.has("--timeout-ms"))
        query.timeout_ms = static_cast<int>(bounded_integer(
            args.take_option("--timeout-ms"), "--timeout-ms", 1, 600000));
    const auto root_text = args.take_option("--root");
    const auto private_text = args.take_option("--private-dir");
    const auto fee_rules_text = args.take_option("--trdpara");
    const auto quote_snapshot_text = args.take_option("--quotes");
    if (!private_text.empty()) query.private_directory = native_path(private_text);
    if (!fee_rules_text.empty()) query.fee_rules_path = native_path(fee_rules_text);
    if (!quote_snapshot_text.empty())
        query.quote_snapshot_path = native_path(quote_snapshot_text);
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-investment.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    const auto document = load_local_investment(root, query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "wrote local investment diagnostics -> "
              << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
