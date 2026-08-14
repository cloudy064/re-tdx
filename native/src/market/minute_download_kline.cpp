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

Json fetch_minute_document(const std::string& market_option, const std::string& code,
                           const std::string& kind_option, int pages, int page_size,
                           int start, const std::string& date, int timeout_ms,
                           const fs::path& root,
                           const std::vector<std::string>& hosts) {
    return fetch_kline_document(market_option, code, kind_option, "1m", pages,
                                page_size, start, date, timeout_ms, root, hosts);
}

Json fetch_kline_document(const std::string& market_option, const std::string& code,
                          const std::string& kind_option, const std::string& period_option,
                          int pages, int page_size, int start,
                          const std::string& date, int timeout_ms,
                          const fs::path& root,
                          const std::vector<std::string>& hosts) {
    const auto [market, market_id] = normalize_market(market_option, code);
    const bool expansion_market = market_id > 2;
    if (expansion_market ? !valid_expansion_code(code) : !valid_kline_code(code, 6))
        throw Error(expansion_market
            ? "expansion-market code must contain 1..9 ASCII letters, digits, or internal spaces"
            : "code must contain 1..6 ASCII letters or digits");
    const auto kind = lower_ascii(trim(kind_option));
    if (kind != "auto" && kind != "stock" && kind != "index")
        throw Error("kind must be auto, stock, or index");
    if (pages < 1 || pages > 20 || page_size < 1 || page_size > 800 ||
        start < 0 || start > 65535 || timeout_ms < 1 || timeout_ms > 600000)
        throw Error("minute request is outside the safe range");
    if (start + (pages - 1LL) * page_size > 65535)
        throw Error("last K-line page start exceeds 65535");

    const auto period = normalize_period(period_option);
    const bool index_mode = kind == "index" ||
        (kind == "auto" && market_id == 1 && is_tdx_block_index_code(code));

    if (expansion_market) {
        if (market_id > 0xFF)
            throw Error("expansion K-line wire protocol only supports market IDs 0..255");
        static const std::array<std::string_view, 5> default_hosts{
            "116.205.143.214:7727", "124.71.223.19:7727", "113.45.175.47:7727",
            "112.74.214.43:7727", "119.97.185.5:7727"
        };
        std::vector<MinuteBar> downloaded;
        std::vector<std::string> failures;
        std::size_t page_index = 0;
        bool reached_end = false;
        std::string used_endpoint;
        std::string server_name;
        for (const auto host : default_hosts) {
            if (page_index == static_cast<std::size_t>(pages) || reached_end) break;
            const auto endpoint = parse_endpoint(host);
            try {
                QuoteConnection connection(endpoint, timeout_ms, QuoteProtocol::expansion);
                used_endpoint = endpoint.address();
                server_name = connection.server_name();
                while (page_index < static_cast<std::size_t>(pages)) {
                    const auto current_start = static_cast<std::uint32_t>(
                        start + static_cast<int>(page_index) * page_size);
                    const auto response = connection.call(type_expansion_klines,
                        build_expansion_kline_request_data(
                            static_cast<std::uint8_t>(market_id), code, current_start,
                            static_cast<std::uint16_t>(page_size), period.id));
                    auto page = parse_expansion_kline_payload(response.data, period.id);
                    downloaded.insert(downloaded.end(), page.begin(), page.end());
                    ++page_index;
                    if (page.size() < static_cast<std::size_t>(page_size)) {
                        reached_end = true;
                        break;
                    }
                }
            } catch (const std::exception& error) {
                failures.push_back(endpoint.address() + ": " + error.what());
            }
        }
        if (page_index == 0) {
            std::string detail = "all 7727 expansion-market endpoints failed";
            for (const auto& failure : failures) detail += "\n  " + failure;
            throw Error(detail);
        }
        if (page_index < static_cast<std::size_t>(pages) && !reached_end)
            throw Error("only completed " + std::to_string(page_index) + "/" +
                        std::to_string(pages) + " expansion K-line pages");

        sort_bars_chronologically(downloaded);
        auto selected = period.intraday ? select_date(downloaded, date) : downloaded;
        MinuteSeries series{market, code, "", from_utf8("7727-" + used_endpoint),
                            std::move(selected)};
        auto document = Json::parse(render_minute_json(series, false));
        const auto downloaded_count = static_cast<std::uint64_t>(downloaded.size());
        const auto requested_count = static_cast<std::uint64_t>(pages) *
                                     static_cast<std::uint64_t>(page_size);
        const auto next_start = static_cast<std::uint64_t>(start) + downloaded_count;
        document["downloaded"] = downloaded_count;
        document["start"] = static_cast<std::uint64_t>(start);
        document["page_size"] = static_cast<std::uint64_t>(page_size);
        document["next_start"] = next_start;
        document["has_more"] = downloaded_count == requested_count && next_start <= 65535;
        document["endpoint"] = used_endpoint;
        document["server_name"] = server_name;
        document["index_mode"] = false;
        document["period"] = period.name;
        document["period_id"] = static_cast<std::uint64_t>(period.id);
        document["expansion_market"] = true;
        document["transport"] = "tdx-7727-0x23ff";
        document["volume_unit"] = "contract";
        document["open_interest_field"] = "position";
        const auto normalized_market = lower_ascii(market);
        document["auxiliary_field"] =
            (normalized_market == "31" || normalized_market == "48" ||
             normalized_market == "kh" || normalized_market == "kg")
                ? "hk_short_volume" : "settlement_price";
        return document;
    }

    const auto endpoint_selection = select_public_quote_endpoints(root, hosts);
    std::vector<MinuteBar> downloaded;
    std::vector<std::string> failures;
    std::size_t page_index = 0;
    bool reached_end = false;
    std::string used_endpoint, server_name;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : endpoint_selection.endpoints) {
        if (page_index == static_cast<std::size_t>(pages) || reached_end) break;
        ++endpoints_attempted;
        int attempts = 0;
        try {
            detail::retry_quote_transport([&] {
                QuoteConnection connection(endpoint, timeout_ms);
                while (page_index < static_cast<std::size_t>(pages)) {
                    const auto current_start = static_cast<std::uint16_t>(
                        start + static_cast<int>(page_index) * page_size);
                    const auto response = connection.call(type_klines,
                        build_kline_request_data(
                            market_id, code, current_start,
                            static_cast<std::uint16_t>(page_size), period.id,
                            default_period_parameter));
                    auto page = parse_kline_payload(response.data, index_mode, period.id);
                    downloaded.insert(downloaded.end(), page.begin(), page.end());
                    ++page_index;
                    used_endpoint = endpoint.address();
                    server_name = connection.server_name();
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
        std::string detail = "all 7709 K-line endpoints failed";
        for (const auto& failure : failures) detail += "\n  " + failure;
        throw Error(detail);
    }
    if (page_index < static_cast<std::size_t>(pages) && !reached_end)
        throw Error("only completed " + std::to_string(page_index) + "/" +
                    std::to_string(pages) + " K-line pages");
    sort_bars_chronologically(downloaded);
    auto selected = period.intraday ? select_date(downloaded, date) : downloaded;
    MinuteSeries series{market, code, "", from_utf8("7709-" + used_endpoint),
                        std::move(selected)};
    auto document = Json::parse(render_minute_json(series, false));
    const auto downloaded_count = static_cast<std::uint64_t>(downloaded.size());
    const auto requested_count = static_cast<std::uint64_t>(pages) *
                                 static_cast<std::uint64_t>(page_size);
    const auto next_start = static_cast<std::uint64_t>(start) + downloaded_count;
    document["downloaded"] = downloaded_count;
    document["start"] = static_cast<std::uint64_t>(start);
    document["page_size"] = static_cast<std::uint64_t>(page_size);
    document["next_start"] = next_start;
    document["has_more"] = downloaded_count == requested_count && next_start <= 65535;
    document["endpoint"] = used_endpoint;
    document["server_name"] = server_name;
    document["index_mode"] = index_mode;
    document["period"] = period.name;
    document["period_id"] = static_cast<std::uint64_t>(period.id);
    document["expansion_market"] = false;
    document["transport"] = "tdx-7709-0x052d";
    document["transport_detail"] = public_quote_transport_document(
        endpoint_selection, connection_attempts, transient_retries,
        endpoints_attempted);
    return document;
}

}  // namespace tdx
