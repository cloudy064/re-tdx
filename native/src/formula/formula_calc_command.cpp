#include "formula_calc_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/minute.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {
namespace {

std::map<std::string, int> parse_parameters(const std::vector<std::string>& raw) {
    std::map<std::string, int> result;
    for (const auto& item : raw) {
        const auto separator = item.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 == item.size())
            throw Error("--param must use NAME=INTEGER");
        const auto name = lower_ascii(trim(item.substr(0, separator)));
        const auto value_text = trim(item.substr(separator + 1));
        std::size_t consumed = 0;
        int value = 0;
        try { value = std::stoi(value_text, &consumed); }
        catch (...) { throw Error("--param value must be an integer: " + item); }
        if (consumed != value_text.size()) throw Error("--param value must be an integer: " + item);
        if (!result.emplace(name, value).second) throw Error("duplicate formula parameter: " + name);
    }
    return result;
}

int integer_option(Args& args, std::string_view name, int fallback, int minimum, int maximum) {
    const auto value_text = args.take_option(name, std::to_string(fallback));
    std::size_t consumed = 0;
    int value = 0;
    try { value = std::stoi(value_text, &consumed); }
    catch (...) { throw Error(std::string(name) + " must be an integer"); }
    if (consumed != value_text.size() || value < minimum || value > maximum)
        throw Error(std::string(name) + " is outside the safe range");
    return value;
}

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

void print_help() {
    std::cout <<
        "Usage: tdx-tool formulas calculate --code CODE [options]\n\n"
        "Fetch K-lines and calculate a high-value TDX-compatible indicator in native C++.\n\n"
        "Options:\n"
        "  --market sz|sh|bj      Auto-detected from code when omitted\n"
        "  --formula NAME         MA/MACD/KDJ/RSI/BOLL/CCI/WR/BIAS/DMA/MTM/ROC/TRIX/ATR\n"
        "                         VOL/OBV/PSY/VR/BRAR/BBI/EXPMA\n"
        "                         DMI/WVAD/EMV/CHO/ADTM/DKX\n"
        "  --period PERIOD        time, 1m, 5m, 15m, 30m, 60m, day, week, month\n"
        "  --kind auto|stock|index (default auto)\n"
        "  --pages N              1..20 (default 1)\n"
        "  --page-size N          1..800 (default 800)\n"
        "  --start N              Historical offset (default 0)\n"
        "  --date latest|all|YYYY-MM-DD (default all)\n"
        "  --param NAME=INTEGER   Repeatable formula parameter override\n"
        "  --output PATH          Write JSON to a file\n"
        "  --compact              Compact JSON\n";
}

}  // namespace

int command_formulas_calculate(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { print_help(); return 0; }
    const auto code = trim(args.take_option("--code"));
    if (code.empty()) throw Error("formulas calculate requires --code");
    const auto market = lower_ascii(trim(args.take_option("--market")));
    const auto formula = lower_ascii(trim(args.take_option("--formula", "macd")));
    const auto period = lower_ascii(trim(args.take_option("--period", "day")));
    const auto kind = lower_ascii(trim(args.take_option("--kind", "auto")));
    const int pages = integer_option(args, "--pages", 1, 1, 20);
    const int page_size = integer_option(args, "--page-size", 800, 1, 800);
    const int start = integer_option(args, "--start", 0, 0, 65535);
    const int timeout = integer_option(args, "--timeout-ms", 10000, 1, 600000);
    const auto date = trim(args.take_option("--date", "all"));
    const auto supplied = parse_parameters(args.take_options("--param"));
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    auto kline = fetch_kline_document(market, code, kind, period, pages, page_size,
                                      start, date, timeout);
    const auto report = calculate_formula_document(std::move(kline), formula, supplied)
                            .dump(compact ? -1 : 2) + "\n";
    if (output_text.empty()) std::cout << report;
    else atomic_write_text(from_utf8(output_text), report);
    return 0;
}

}  // namespace tdx

