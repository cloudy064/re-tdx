#include "options_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace tdx {

using namespace option_detail;

int command_market_options(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market options [options]\n\n"
            "Download and normalize active commodity/index-option contracts.\n\n"
            "Options:\n"
            "  --market MARKET        CZCE/DCE/SHFE/CFFEX/GFEX or numeric ID\n"
            "  --underlying TEXT      Filter normalized underlying security\n"
            "  --contract TEXT        Filter visible option contract\n"
            "  --type TYPE            all/call/put (default all)\n"
            "  --query TEXT           Filter wire code/name/underlying\n"
            "  --limit N              Return 1..200000 rows (default 20000)\n"
            "  --refresh              Bypass the in-process catalog cache\n"
            "  --cache-ttl N          Cache lifetime in seconds (default 300)\n"
            "  --timeout-ms N         Socket timeout (default 30000)\n"
            "  --compact              Write compact JSON\n"
            "  --output FILE          Write JSON instead of stdout\n";
        return 0;
    }
    const auto market_text = args.take_option("--market");
    const int market_filter = trim(market_text).empty() ? -1 : option_market_id(market_text);
    const auto underlying = trim(args.take_option("--underlying"));
    const auto contract = trim(args.take_option("--contract"));
    const auto type = trim(args.take_option("--type", "all"));
    const auto query = trim(args.take_option("--query"));
    const int limit = parse_integer(args.take_option("--limit", "20000"), "--limit", 1, 200000);
    const bool refresh = args.take_flag("--refresh");
    const int cache_ttl = parse_integer(args.take_option("--cache-ttl", "300"),
                                        "--cache-ttl", 0, 86400);
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "30000"),
                                         "--timeout-ms", 1, 600000);
    const bool compact = args.take_flag("--compact");
    const auto output = args.take_option("--output");
    args.require_empty();
    const auto document = fetch_option_catalog_document(
        market_filter, underlying, contract, type, query, limit,
        refresh, cache_ttl, timeout_ms);
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << rendered;
    else atomic_write_text(fs::path(utf8_to_wide(output)), rendered);
    return 0;
}

int command_market_option_expiry(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market option-expiry --security MARKET:WIRE_CODE [options]\n\n"
            "Resolve an option expiry from TDX code2name_qq.ini and holiday metadata.\n\n"
            "Options:\n"
            "  --security MARKET:CODE Required TDX option wire security\n"
            "  --name NAME            Visible option name; avoids a catalog lookup\n"
            "  --root PATH            TDX installation root\n"
            "  --refresh              Bypass the option catalog cache\n"
            "  --cache-ttl N          Cache lifetime in seconds (default 300)\n"
            "  --timeout-ms N         Socket timeout (default 30000)\n"
            "  --compact              Write compact JSON\n"
            "  --output FILE          Write JSON instead of stdout\n";
        return 0;
    }
    const auto security = args.take_option("--security");
    const auto separator = security.find(':');
    if (separator == std::string::npos) throw Error("--security must use MARKET:WIRE_CODE");
    const int market_id = option_market_id(trim(security.substr(0, separator)));
    const auto code = upper_ascii(security.substr(separator + 1));
    const auto name = trim(args.take_option("--name"));
    const auto root = args.take_option("--root");
    const bool refresh = args.take_flag("--refresh");
    const int cache_ttl = parse_integer(args.take_option("--cache-ttl", "300"),
                                        "--cache-ttl", 0, 86400);
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "30000"),
                                         "--timeout-ms", 1, 600000);
    const bool compact = args.take_flag("--compact");
    const auto output = args.take_option("--output");
    args.require_empty();
    std::optional<OptionInstrument> option;
    if (!name.empty()) option = parse_option_instrument(market_id, code, name);
    else {
        const auto catalog = cached_full_option_catalog(refresh, cache_ttl, timeout_ms);
        option = find_option_in_catalog(catalog, market_id, code);
    }
    if (!option) throw Error("selected security is absent from the active TDX option catalog");
    const auto document = resolve_option_expiry_document(
        root.empty() ? fs::path{} : fs::path(utf8_to_wide(root)), *option);
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << rendered;
    else atomic_write_text(fs::path(utf8_to_wide(output)), rendered);
    return 0;
}

int command_market_option_volatility(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market option-volatility --security MARKET:WIRE_CODE [options]\n\n"
            "Reproduce TDX historical/implied volatility for an active option.\n\n"
            "Options:\n"
            "  --security MARKET:CODE Required TDX option wire security\n"
            "  --name NAME            Visible name; avoids a catalog lookup\n"
            "  --root PATH            TDX root for automatic expiry rules\n"
            "  --date YYYYMMDD         Use the last bar on/before this date\n"
            "  --expiry YYYYMMDD       Override the automatically resolved expiry\n"
            "  --lookback N            Underlying daily bars, 2..800 (default 60)\n"
            "  --risk-free RATE        Decimal annual rate (TDX default 0.0187)\n"
            "  --option-price PRICE    Override the selected option daily close\n"
            "  --refresh               Bypass the option catalog cache\n"
            "  --cache-ttl N           Cache lifetime in seconds (default 300)\n"
            "  --timeout-ms N          Socket timeout (default 30000)\n"
            "  --compact               Write compact JSON\n"
            "  --output FILE           Write JSON instead of stdout\n";
        return 0;
    }
    const auto security = args.take_option("--security");
    const auto separator = security.find(':');
    if (separator == std::string::npos)
        throw Error("--security must use MARKET:WIRE_CODE");
    const auto market = trim(security.substr(0, separator));
    const auto code = security.substr(separator + 1);
    const auto name = trim(args.take_option("--name"));
    const auto root = args.take_option("--root");
    const auto date = trim(args.take_option("--date"));
    const auto expiry = trim(args.take_option("--expiry"));
    const int lookback = parse_integer(args.take_option("--lookback", "60"),
                                       "--lookback", 2, 800);
    const double risk_free = parse_number(args.take_option("--risk-free", "0.0187"),
                                          "--risk-free", -1.0, 1.0);
    std::optional<double> option_price;
    const auto price_text = trim(args.take_option("--option-price"));
    if (!price_text.empty()) option_price = parse_number(price_text, "--option-price", 0.000001, 1e12);
    const bool refresh = args.take_flag("--refresh");
    const int cache_ttl = parse_integer(args.take_option("--cache-ttl", "300"),
                                        "--cache-ttl", 0, 86400);
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "30000"),
                                         "--timeout-ms", 1, 600000);
    const bool compact = args.take_flag("--compact");
    const auto output = args.take_option("--output");
    args.require_empty();
    const auto document = fetch_option_volatility_document(
        market, code, name, date, expiry, lookback, risk_free,
        option_price, refresh, cache_ttl, timeout_ms,
        root.empty() ? fs::path{} : fs::path(utf8_to_wide(root)));
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << rendered;
    else atomic_write_text(fs::path(utf8_to_wide(output)), rendered);
    return 0;
}

int command_market_option_chain(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market option-chain --market MARKET --contract CONTRACT [options]\n\n"
            "Build a live option chain, IV smile, Greeks and open-interest summary.\n\n"
            "Options:\n"
            "  --market MARKET        CZCE/DCE/SHFE/CFFEX/GFEX or numeric ID\n"
            "  --contract CONTRACT    Exact visible contract, for example A2609\n"
            "  --root PATH            TDX root for automatic expiry rules\n"
            "  --expiry YYYYMMDD       Override the automatically resolved expiry\n"
            "  --lookback N            Underlying daily bars, 2..800 (default 60)\n"
            "  --risk-free RATE        Decimal annual rate (TDX default 0.0187)\n"
            "  --limit N               Maximum chain contracts, 2..2000 (default 500)\n"
            "  --refresh               Bypass the option catalog cache\n"
            "  --cache-ttl N           Cache lifetime in seconds (default 300)\n"
            "  --timeout-ms N          Socket timeout (default 30000)\n"
            "  --compact               Write compact JSON\n"
            "  --output FILE           Write JSON instead of stdout\n";
        return 0;
    }
    const auto market = trim(args.take_option("--market"));
    const auto contract = trim(args.take_option("--contract"));
    const auto root = args.take_option("--root");
    const auto expiry = trim(args.take_option("--expiry"));
    const int lookback = parse_integer(args.take_option("--lookback", "60"),
                                       "--lookback", 2, 800);
    const double risk_free = parse_number(args.take_option("--risk-free", "0.0187"),
                                          "--risk-free", -1.0, 1.0);
    const int limit = parse_integer(args.take_option("--limit", "500"),
                                    "--limit", 2, 2000);
    const bool refresh = args.take_flag("--refresh");
    const int cache_ttl = parse_integer(args.take_option("--cache-ttl", "300"),
                                        "--cache-ttl", 0, 86400);
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "30000"),
                                         "--timeout-ms", 1, 600000);
    const bool compact = args.take_flag("--compact");
    const auto output = args.take_option("--output");
    args.require_empty();
    if (market.empty()) throw Error("market option-chain requires --market");
    if (contract.empty()) throw Error("market option-chain requires --contract");
    const auto document = fetch_option_chain_document(
        root.empty() ? fs::path{} : fs::path(utf8_to_wide(root)),
        market, contract, expiry, lookback, risk_free, limit,
        refresh, cache_ttl, timeout_ms);
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << rendered;
    else atomic_write_text(fs::path(utf8_to_wide(output)), rendered);
    return 0;
}

}  // namespace tdx
