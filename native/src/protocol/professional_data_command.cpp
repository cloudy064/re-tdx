#include "professional_data_internal.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace professional_data_detail;

int command_market_professional(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market professional [options]\n\n"
            "Parse the official TongDaXin professional finance/trading packages.\n\n"
            "Options:\n"
            "  --kind catalog|finance|finance-series|stock|board|market  Default catalog\n"
            "  --security [sz|sh|bj:]CODE           Required except for catalog/market\n"
            "  --period YYYYMMDD                    Required for remote finance\n"
            "  --field ID                           Repeatable field filter\n"
            "  --input PATH                         Parse a local extracted .dat file\n"
            "  --from YYYYMMDD --to YYYYMMDD        Trading history date range\n"
            "  --history                            Include trading records\n"
            "  --limit N                            History cap (5000), finance-series periods (8)\n"
            "  --cache-dir PATH                     Default output/tdx-professional-cache\n"
            "  --refresh                            Redownload and verify MD5\n"
            "  --timeout-ms N                       Default 30000\n"
            "  --output PATH                        Default output/tdx-professional.json\n"
            "  --compact                            Compact JSON\n";
        return 0;
    }
    const auto kind = lower_ascii(args.take_option("--kind", "catalog"));
    const auto security_text = args.take_option("--security");
    const auto period_text = args.take_option("--period");
    const auto input_text = args.take_option("--input");
    std::vector<int> fields;
    for (const auto& value : args.take_options("--field"))
        fields.push_back(parse_integer(value, "--field", 0, 584));
    const auto from = parse_date(args.take_option("--from"), "--from", 0);
    const auto to = parse_date(args.take_option("--to"), "--to", 99999999);
    const bool finance_series = kind == "finance-series";
    const auto limit = static_cast<std::size_t>(parse_integer(
        args.take_option("--limit", finance_series ? "8" : "5000"), "--limit", 1,
        finance_series ? 80 : 1000000));
    const bool history = args.take_flag("--history"), refresh = args.take_flag("--refresh");
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "30000"),
                                         "--timeout-ms", 1, 600000);
    const auto cache_text = args.take_option("--cache-dir");
    const auto output_text = args.take_option("--output", "output/tdx-professional.json");
    const bool compact = args.take_flag("--compact"); args.require_empty();
    const auto cache = cache_text.empty() ? fs::path{} : fs::u8path(cache_text);
    Json document;
    if (kind == "catalog") {
        if (!input_text.empty() || !security_text.empty() || !period_text.empty())
            throw Error("catalog does not accept --input, --security or --period");
        document = fetch_professional_catalog_document(timeout_ms);
    } else if (kind == "finance" || kind == "finance-series") {
        if (security_text.empty()) throw Error("finance requires --security");
        const auto security = parse_security(security_text);
        if (kind == "finance-series") {
            if (!input_text.empty() || !period_text.empty())
                throw Error("finance-series does not accept --input or --period");
            document = fetch_professional_finance_series_document(
                security.market_id, security.code, fields, from, to, limit, cache,
                timeout_ms, refresh);
        } else {
            ProfessionalFinanceData data;
            if (!input_text.empty())
                data = parse_professional_finance_data(read_bytes(fs::u8path(input_text)), input_text);
            else {
            if (period_text.empty()) throw Error("remote finance requires --period YYYYMMDD");
            data = fetch_professional_finance_data(parse_date(period_text, "--period", 0),
                                                   cache, timeout_ms, refresh);
            }
            document = professional_finance_document(data, security.market_id, security.code, fields);
        }
    } else if (kind == "stock" || kind == "board" || kind == "market") {
        if ((kind == "stock" || kind == "board") && security_text.empty())
            throw Error(kind + " requires --security");
        if (kind == "market" && !security_text.empty()) throw Error("market does not accept --security");
        std::vector<ProfessionalTradingRecord> records; std::string security_id;
        std::optional<Security> selected_security;
        if (kind != "market") {
            selected_security = parse_security(
                kind == "board" && security_text.find(':') == std::string::npos &&
                security_text.size() == 6 ? "sh:" + security_text : security_text);
            security_id = selected_security->id();
        }
        if (!input_text.empty()) records = parse_professional_trading_data(read_bytes(fs::u8path(input_text)));
        else if (kind == "market") records = fetch_professional_market_trading_data(cache, timeout_ms, refresh);
        else {
            records = fetch_professional_stock_trading_data(selected_security->market,
                                                            selected_security->code,
                                                            cache, timeout_ms, refresh);
        }
        document = professional_trading_document(records, kind,
            kind == "market" ? "SH999999" : security_id, fields, from, to, limit, history);
    } else throw Error("--kind must be catalog, finance, finance-series, stock, board or market");
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "professional " << kind << " -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
