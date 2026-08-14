#include "minute_download_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/security_identity.hpp"
#include "tdx/session_audit.hpp"
#include "tdx/transport.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {

using namespace minute_download_detail;

int command_minute_download(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        print_help();
        return 0;
    }
    const auto code = args.take_option("--code");
    const auto market_option = args.take_option("--market");
    const auto kind = lower_ascii(args.take_option("--kind", "auto"));
    const auto hosts = args.take_options("--host");
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "10000"),
                                         "--timeout-ms", 1, 600000);
    const int pages = parse_integer(args.take_option("--pages", "1"), "--pages", 1, 8192);
    const int page_size = parse_integer(args.take_option("--page-size", "800"),
                                        "--page-size", 1, 800);
    const int start = parse_integer(args.take_option("--start", "0"), "--start", 0, 65535);
    const auto root_text = args.take_option("--root");
    const bool no_merge = args.take_flag("--no-merge-existing");
    const auto lc1_output_text = args.take_option("--lc1-output");
    const auto format = lower_ascii(args.take_option("--format", "json"));
    const auto date = args.take_option("--date", "latest");
    const auto output_text = args.take_option("--output");
    args.require_empty();

    if (kind != "auto" && kind != "stock" && kind != "index")
        throw Error("--kind must be auto, stock, or index");
    if (format != "json" && format != "csv" && format != "none")
        throw Error("--format must be json, csv, or none");
    if (start + (pages - 1LL) * page_size > 65535)
        throw Error("last K-line page start exceeds 65535");

    const auto [market, market_id] = normalize_market(market_option, code);
    if (market_id > 2)
        throw Error("minute download writes LC1 and cannot preserve expansion fields; "
                    "use `tdx-tool market kline --security MARKET:CODE` instead");
    if (!valid_kline_code(code, 6))
        throw Error("--code must contain 1..6 ASCII letters or digits");
    const bool index_mode = kind == "index" ||
        (kind == "auto" && market_id == 1 && is_tdx_block_index_code(code));
    const fs::path lc1_output = lc1_output_text.empty()
        ? fs::path("output") / "minute" / (market + code + ".lc1")
        : from_utf8(lc1_output_text);

    fs::path endpoint_root;
    try {
        endpoint_root = find_tdx_root(
            root_text.empty() ? fs::path{} : from_utf8(root_text));
    } catch (const Error&) {
        if (!root_text.empty()) throw;
    }
    const auto endpoint_selection = select_public_quote_endpoints(endpoint_root, hosts);

    std::vector<MinuteBar> downloaded;
    std::vector<std::string> failures;
    std::size_t page_index = 0;
    bool reached_end = false;
    std::string used_endpoint;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : endpoint_selection.endpoints) {
        if (page_index == static_cast<std::size_t>(pages) || reached_end) break;
        ++endpoints_attempted;
        int attempts = 0;
        try {
            detail::retry_quote_transport([&] {
                QuoteConnection connection(endpoint, timeout_ms);
                used_endpoint = endpoint.address();
                std::cerr << "connected " << endpoint.address() << " ("
                          << connection.server_name() << ")\n";
                while (page_index < static_cast<std::size_t>(pages)) {
                    const auto current_start = static_cast<std::uint16_t>(
                        start + static_cast<int>(page_index) * page_size);
                    const auto response = connection.call(
                        type_klines,
                        build_kline_request_data(market_id, code, current_start,
                                                 static_cast<std::uint16_t>(page_size)));
                    auto page = parse_kline_payload(response.data, index_mode);
                    std::cerr << "page " << (page_index + 1) << '/' << pages
                              << ": start=" << current_start << ", bars="
                              << page.size() << '\n';
                    downloaded.insert(downloaded.end(), page.begin(), page.end());
                    ++page_index;
                    if (page.size() < static_cast<std::size_t>(page_size)) {
                        reached_end = true;
                        break;
                    }
                }
                return true;
            }, attempts);
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
        connection_attempts += attempts;
        transient_retries += std::max(0, attempts - 1);
    }
    if (page_index == 0) {
        std::string detail = "all 7709 endpoints failed";
        for (const auto& failure : failures) detail += "\n  " + failure;
        throw Error(detail);
    }
    if (page_index < static_cast<std::size_t>(pages) && !reached_end)
        throw Error("only completed " + std::to_string(page_index) + "/" +
                    std::to_string(pages) + " K-line pages");
    std::cerr << "transport attempts=" << connection_attempts
              << ", retries=" << transient_retries
              << ", endpoints=" << endpoints_attempted
              << ", source=" << endpoint_selection.source << '\n';

    std::vector<MinuteBar> existing;
    if (!no_merge) {
        std::set<std::string> seen_paths;
        std::vector<fs::path> candidates{lc1_output};
        try {
            const auto local_root = endpoint_root.empty()
                ? find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text))
                : endpoint_root;
            candidates.push_back(locate_lc1(local_root, market, code, index_mode));
        } catch (const Error&) {
            if (!root_text.empty()) throw;
        }
        for (const auto& candidate : candidates) {
            std::error_code error;
            const auto absolute = fs::absolute(candidate, error);
            const auto key = lower_ascii(path_utf8(error ? candidate : absolute));
            if (seen_paths.insert(key).second && fs::is_regular_file(candidate)) {
                const auto bars = parse_lc1(read_bytes(candidate));
                existing.insert(existing.end(), bars.begin(), bars.end());
            }
        }
    }
    auto merged = merge_bars(existing, downloaded);
    atomic_write_bytes(lc1_output, pack_lc1(merged));

    fs::path render_output;
    std::size_t rendered_count = 0;
    if (format != "none") {
        render_output = output_text.empty()
            ? fs::path("output") / ("tdx-" + code + "-1m." + format)
            : from_utf8(output_text);
        auto selected = select_date(merged, date);
        rendered_count = selected.size();
        MinuteSeries series{market, code, "", from_utf8("7709-" + used_endpoint),
                            std::move(selected)};
        atomic_write_text(render_output, format == "csv" ? render_minute_csv(series)
                                                           : render_minute_json(series));
    }
    std::cout << "downloaded " << downloaded.size() << " bars; snapshot " << merged.size()
              << " bars -> " << path_utf8(lc1_output);
    if (format != "none")
        std::cout << "; exported " << rendered_count << " bars -> " << path_utf8(render_output);
    std::cout << '\n';
    return 0;
}

int command_market_instruments(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market instruments [options]\n\n"
            "Download the TDX 7727 expansion-market instrument directory.\n\n"
            "Options:\n"
            "  --start N              Global directory offset (default 0)\n"
            "  --count N              Fetch 1..200000 records (default 100)\n"
            "  --all                  Fetch all records after --start\n"
            "  --market MARKET        Filter qz/qd/qs/cz/qg or numeric market ID\n"
            "  --query TEXT           Filter code/name/description in fetched records\n"
            "  --timeout-ms N         Socket timeout (default 10000)\n"
            "  --compact              Write compact JSON\n"
            "  --output FILE          Write JSON instead of stdout\n";
        return 0;
    }
    const int start = parse_integer(args.take_option("--start", "0"), "--start", 0, 1000000);
    int count = parse_integer(args.take_option("--count", "100"), "--count", 1, 200000);
    const bool all = args.take_flag("--all");
    const auto market_text = trim(args.take_option("--market"));
    const auto query = trim(args.take_option("--query"));
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "10000"),
                                         "--timeout-ms", 1, 600000);
    const bool compact = args.take_flag("--compact");
    const auto output = args.take_option("--output");
    args.require_empty();
    int market_filter = -1;
    if (!market_text.empty()) market_filter = normalize_market(market_text, "X").second;
    if (all) count = 200000;
    const auto document = fetch_expansion_instruments_document(
        start, count, market_filter, query, timeout_ms);
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << rendered;
    else atomic_write_text(from_utf8(output), rendered);
    return 0;
}

int command_market_expansion_quote(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market expansion-quote --security MARKET:CODE [options]\n\n"
            "Get a real-time TDX 7727 quote, five-level book, volume and open interest.\n\n"
            "Options:\n"
            "  --security MARKET:CODE Expansion security, for example 47:IFL9\n"
            "  --timeout-ms N          Socket timeout (default 10000)\n"
            "  --compact               Write compact JSON\n"
            "  --output FILE           Write JSON instead of stdout\n";
        return 0;
    }
    const auto security = trim(args.take_option("--security"));
    const auto colon = security.find(':');
    if (colon == std::string::npos)
        throw Error("--security must use MARKET:CODE, for example 47:IFL9");
    const auto market = trim(security.substr(0, colon));
    const auto code = trim(security.substr(colon + 1));
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "10000"),
                                         "--timeout-ms", 1, 600000);
    const bool compact = args.take_flag("--compact");
    const auto output = args.take_option("--output");
    args.require_empty();
    const auto document = fetch_expansion_quote_document(market, code, timeout_ms);
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << rendered;
    else atomic_write_text(from_utf8(output), rendered);
    return 0;
}

int command_market_expansion_timeline(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market expansion-timeline --security MARKET:CODE [options]\n\n"
            "Get current or historical TDX 7727 minute timeline data.\n\n"
            "Options:\n"
            "  --security MARKET:CODE Expansion security, for example 29:A2609\n"
            "  --date YYYYMMDD        Historical trading date; omit for current data\n"
            "  --timeout-ms N         Socket timeout (default 10000)\n"
            "  --compact              Write compact JSON\n"
            "  --output FILE          Write JSON instead of stdout\n";
        return 0;
    }
    const auto security = trim(args.take_option("--security"));
    const auto colon = security.find(':');
    if (colon == std::string::npos)
        throw Error("--security must use MARKET:CODE, for example 29:A2609");
    const auto market = trim(security.substr(0, colon));
    const auto code = trim(security.substr(colon + 1));
    const auto date = trim(args.take_option("--date"));
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "10000"),
                                         "--timeout-ms", 1, 600000);
    const bool compact = args.take_flag("--compact");
    const auto output = args.take_option("--output");
    args.require_empty();
    const auto document = fetch_expansion_timeline_document(market, code, date, timeout_ms);
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << rendered;
    else atomic_write_text(from_utf8(output), rendered);
    return 0;
}

int command_market_expansion_trades(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market expansion-trades --security MARKET:CODE [options]\n\n"
            "Get current or historical TDX 7727 trades with open/close nature.\n\n"
            "Options:\n"
            "  --security MARKET:CODE Expansion security, for example 29:A2609\n"
            "  --date YYYYMMDD        Historical trading date; omit for current data\n"
            "  --start N              Trade offset (default 0)\n"
            "  --page-size N          1..1800 trades per page (default 1800)\n"
            "  --pages N              1..20 pages (default 1)\n"
            "  --timeout-ms N         Socket timeout (default 10000)\n"
            "  --compact              Write compact JSON\n"
            "  --output FILE          Write JSON instead of stdout\n";
        return 0;
    }
    const auto security = trim(args.take_option("--security"));
    const auto colon = security.find(':');
    if (colon == std::string::npos)
        throw Error("--security must use MARKET:CODE, for example 29:A2609");
    const auto market = trim(security.substr(0, colon));
    const auto code = trim(security.substr(colon + 1));
    const auto date = trim(args.take_option("--date"));
    const int start = parse_integer(args.take_option("--start", "0"),
                                    "--start", 0, std::numeric_limits<int>::max());
    const int page_size = parse_integer(args.take_option("--page-size", "1800"),
                                        "--page-size", 1, 1800);
    const int pages = parse_integer(args.take_option("--pages", "1"),
                                    "--pages", 1, 20);
    const int timeout_ms = parse_integer(args.take_option("--timeout-ms", "10000"),
                                         "--timeout-ms", 1, 600000);
    const bool compact = args.take_flag("--compact");
    const auto output = args.take_option("--output");
    args.require_empty();
    const auto document = fetch_expansion_trades_document(
        market, code, date, start, page_size, pages, timeout_ms);
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << rendered;
    else atomic_write_text(from_utf8(output), rendered);
    return 0;
}

}  // namespace tdx
