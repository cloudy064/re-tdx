#include "minute_download_internal.hpp"

#include "tdx/common.hpp"
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

Json fetch_expansion_instruments_document(int start, int count, int market_filter,
                                          const std::string& query, int timeout_ms) {
    if (start < 0 || start > 1000000 || count < 1 || count > 200000 ||
        market_filter < -1 || market_filter > 255 || timeout_ms < 1 || timeout_ms > 600000)
        throw Error("expansion instrument request is outside the safe range");
    static const std::array<std::string_view, 5> default_hosts{
        "116.205.143.214:7727", "124.71.223.19:7727", "113.45.175.47:7727",
        "112.74.214.43:7727", "119.97.185.5:7727"
    };
    std::vector<std::string> failures;
    for (const auto host : default_hosts) {
        const auto endpoint = parse_endpoint(host);
        try {
            QuoteConnection connection(endpoint, timeout_ms, QuoteProtocol::expansion);
            const auto total = parse_expansion_instrument_count_payload(
                connection.call(type_expansion_instrument_count).data);
            const auto available = start < static_cast<int>(total)
                ? static_cast<int>(total) - start : 0;
            const int wanted = std::min(count, available);
            std::vector<ExpansionInstrument> instruments;
            instruments.reserve(static_cast<std::size_t>(wanted));
            int offset = start;
            bool directory_exhausted = false;
            while (static_cast<int>(instruments.size()) < wanted) {
                const auto page_size = static_cast<std::uint16_t>(
                    std::min(100, wanted - static_cast<int>(instruments.size())));
                Bytes request;
                append_u32(request, static_cast<std::uint32_t>(offset));
                append_u16(request, page_size);
                auto page = parse_expansion_instrument_info_payload(
                    connection.call(type_expansion_instrument_info, request).data);
                if (page.empty()) {
                    directory_exhausted = true;
                    break;
                }
                offset += static_cast<int>(page.size());
                instruments.insert(instruments.end(),
                                   std::make_move_iterator(page.begin()),
                                   std::make_move_iterator(page.end()));
                if (page.size() < page_size) {
                    directory_exhausted = true;
                    break;
                }
            }

            const auto needle = lower_ascii(trim(query));
            Json rows = Json::array();
            std::map<int, std::uint64_t> market_counts;
            for (const auto& instrument : instruments) {
                ++market_counts[instrument.market_id];
                if (market_filter >= 0 && instrument.market_id != market_filter) continue;
                if (!needle.empty()) {
                    const auto haystack = lower_ascii(instrument.code + " " + instrument.name +
                                                      " " + instrument.description);
                    if (haystack.find(needle) == std::string::npos) continue;
                }
                Json row = Json::object();
                row["category"] = static_cast<std::uint64_t>(instrument.category);
                row["market_id"] = static_cast<std::uint64_t>(instrument.market_id);
                row["code"] = instrument.code;
                row["name"] = instrument.name;
                row["description"] = instrument.description;
                row["contract_multiplier"] =
                    static_cast<std::uint64_t>(instrument.contract_multiplier);
                row["contract_multiplier_source"] = "tdx-7727-0x23f5-offset56-u32";
                row["security"] = std::to_string(instrument.market_id) + ":" + instrument.code;
                rows.push_back(std::move(row));
            }
            Json counts = Json::object();
            for (const auto& [market, value] : market_counts)
                counts[std::to_string(market)] = value;
            Json result = Json::object();
            result["schema"] = "tdx-expansion-instruments-v1";
            result["total"] = static_cast<std::uint64_t>(total);
            result["start"] = static_cast<std::uint64_t>(start);
            result["requested"] = static_cast<std::uint64_t>(count);
            result["fetched"] = static_cast<std::uint64_t>(instruments.size());
            result["returned"] = static_cast<std::uint64_t>(rows.size());
            result["next_start"] = static_cast<std::uint64_t>(start + instruments.size());
            const auto next_start = start + static_cast<int>(instruments.size());
            result["has_more"] = !directory_exhausted && next_start < static_cast<int>(total);
            result["directory_exhausted"] = directory_exhausted;
            result["reported_total_shortfall"] = directory_exhausted && next_start < static_cast<int>(total)
                ? static_cast<std::uint64_t>(static_cast<int>(total) - next_start) : 0;
            result["market_filter"] = market_filter < 0 ? Json(nullptr) : Json(market_filter);
            result["query"] = query;
            result["endpoint"] = endpoint.address();
            result["server_name"] = connection.server_name();
            result["transport"] = "tdx-7727-0x23f0-0x23f5";
            result["market_counts_in_page"] = std::move(counts);
            result["instruments"] = std::move(rows);
            return result;
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
    }
    std::string detail = "all 7727 expansion instrument-directory endpoints failed";
    for (const auto& failure : failures) detail += "\n  " + failure;
    throw Error(detail);
}

std::optional<ExpansionInstrument> fetch_expansion_instrument_record(
    int market_id, const std::string& code_option, int timeout_ms) {
    auto code = trim(code_option);
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    if (market_id < 3 || market_id > 255 || !valid_expansion_code(code) ||
        timeout_ms < 1 || timeout_ms > 600000)
        throw Error("expansion instrument lookup is outside the safe range");
    static const std::array<std::string_view, 5> default_hosts{
        "116.205.143.214:7727", "124.71.223.19:7727", "113.45.175.47:7727",
        "112.74.214.43:7727", "119.97.185.5:7727"
    };
    std::vector<std::string> failures;
    for (const auto host : default_hosts) {
        const auto endpoint = parse_endpoint(host);
        try {
            QuoteConnection connection(endpoint, timeout_ms, QuoteProtocol::expansion);
            const auto total = parse_expansion_instrument_count_payload(
                connection.call(type_expansion_instrument_count).data);
            for (std::uint32_t offset = 0; offset < total;) {
                Bytes request;
                append_u32(request, offset);
                const auto wanted = static_cast<std::uint16_t>(
                    std::min<std::uint32_t>(100, total - offset));
                append_u16(request, wanted);
                auto page = parse_expansion_instrument_info_payload(
                    connection.call(type_expansion_instrument_info, request).data);
                if (page.empty()) break;
                for (auto& instrument : page) {
                    auto instrument_code = instrument.code;
                    std::transform(instrument_code.begin(), instrument_code.end(),
                                   instrument_code.begin(), [](unsigned char ch) {
                        return static_cast<char>(std::toupper(ch));
                    });
                    if (instrument.market_id == market_id &&
                        instrument_code == code)
                        return instrument;
                }
                offset += static_cast<std::uint32_t>(page.size());
                if (page.size() < wanted) break;
            }
            return std::nullopt;
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
    }
    std::string detail = "all 7727 exact instrument lookup endpoints failed";
    for (const auto& failure : failures) detail += "\n  " + failure;
    throw Error(detail);
}

Json fetch_expansion_quote_document(const std::string& market_option,
                                    const std::string& code_option, int timeout_ms) {
    auto code = trim(code_option);
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    const auto [market, market_id] = normalize_market(market_option, code);
    if (market_id < 3 || market_id > 255)
        throw Error("expansion quote requires a TDX expansion market ID 3..255");
    if (!valid_expansion_code(code))
        throw Error("expansion quote code must contain 1..9 ASCII letters, digits, or internal spaces");
    if (timeout_ms < 1 || timeout_ms > 600000)
        throw Error("expansion quote timeout is outside the safe range");
    static const std::array<std::string_view, 5> default_hosts{
        "116.205.143.214:7727", "124.71.223.19:7727", "113.45.175.47:7727",
        "112.74.214.43:7727", "119.97.185.5:7727"
    };
    std::vector<std::string> failures;
    for (const auto host : default_hosts) {
        const auto endpoint = parse_endpoint(host);
        try {
            QuoteConnection connection(endpoint, timeout_ms, QuoteProtocol::expansion);
            Bytes request{static_cast<std::uint8_t>(market_id)};
            request.resize(10, 0);
            std::copy(code.begin(), code.end(), request.begin() + 1);
            auto result = parse_expansion_quote_payload(
                connection.call(type_expansion_quote, request).data);
            result["endpoint"] = endpoint.address();
            result["server_name"] = connection.server_name();
            result["transport"] = "tdx-7727-0x23fa";
            result["volume_unit"] = "contract";
            return result;
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
    }
    std::string detail = "all 7727 expansion quote endpoints failed";
    for (const auto& failure : failures) detail += "\n  " + failure;
    throw Error(detail);
}

Json fetch_expansion_quotes_document(
    const std::vector<std::pair<int, std::string>>& securities,
    int timeout_ms) {
    if (securities.empty()) throw Error("expansion quote batch requires at least one security");
    if (securities.size() > 2000)
        throw Error("expansion quote batch exceeds the 2000-security safety limit");
    if (timeout_ms < 1 || timeout_ms > 600000)
        throw Error("expansion quote batch timeout is outside the safe range");
    std::vector<std::pair<int, std::string>> normalized;
    std::set<std::pair<int, std::string>> seen;
    for (auto [market_id, code] : securities) {
        code = trim(code);
        std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        if (market_id < 3 || market_id > 255)
            throw Error("expansion quote batch market must be in the range 3..255");
        if (!valid_expansion_code(code))
            throw Error("expansion quote batch contains an invalid wire code");
        if (seen.insert({market_id, code}).second)
            normalized.push_back({market_id, std::move(code)});
    }
    static const std::array<std::string_view, 5> default_hosts{
        "116.205.143.214:7727", "124.71.223.19:7727", "113.45.175.47:7727",
        "112.74.214.43:7727", "119.97.185.5:7727"
    };
    std::vector<std::string> failures;
    for (const auto host : default_hosts) {
        const auto endpoint = parse_endpoint(host);
        try {
            QuoteConnection connection(endpoint, timeout_ms, QuoteProtocol::expansion);
            Json rows = Json::array();
            for (const auto& [market_id, code] : normalized) {
                Bytes request{static_cast<std::uint8_t>(market_id)};
                request.resize(10, 0);
                std::copy(code.begin(), code.end(), request.begin() + 1);
                auto quote = parse_expansion_quote_payload(
                    connection.call(type_expansion_quote, request).data);
                quote["volume_unit"] = "contract";
                rows.push_back(std::move(quote));
            }
            Json result = Json::object();
            result["schema"] = "tdx-expansion-quotes-v1";
            result["requested"] = static_cast<std::uint64_t>(securities.size());
            result["returned"] = static_cast<std::uint64_t>(rows.size());
            result["deduplicated"] = securities.size() != normalized.size();
            result["quotes"] = std::move(rows);
            result["endpoint"] = endpoint.address();
            result["server_name"] = connection.server_name();
            result["transport"] = "tdx-7727-0x23fa-persistent-batch";
            return result;
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
    }
    std::string detail = "all 7727 expansion quote batch endpoints failed";
    for (const auto& failure : failures) detail += "\n  " + failure;
    throw Error(detail);
}

Json fetch_expansion_timeline_document(const std::string& market_option,
                                       const std::string& code_option,
                                       const std::string& date_option,
                                       int timeout_ms) {
    auto code = trim(code_option);
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    const auto [market, market_id] = normalize_market(market_option, code);
    if (market_id < 3 || market_id > 255)
        throw Error("expansion timeline requires a TDX expansion market ID 3..255");
    if (!valid_expansion_code(code))
        throw Error("expansion timeline code must contain 1..9 ASCII letters, digits, or internal spaces");
    if (timeout_ms < 1 || timeout_ms > 600000)
        throw Error("expansion timeline timeout is outside the safe range");
    const auto date = normalize_expansion_date(date_option);
    static const std::array<std::string_view, 5> default_hosts{
        "116.205.143.214:7727", "124.71.223.19:7727", "113.45.175.47:7727",
        "112.74.214.43:7727", "119.97.185.5:7727"
    };
    std::vector<std::string> failures;
    for (const auto host : default_hosts) {
        const auto endpoint = parse_endpoint(host);
        try {
            QuoteConnection connection(endpoint, timeout_ms, QuoteProtocol::expansion);
            const auto response = date
                ? connection.call(type_expansion_history_minute,
                    build_expansion_history_minute_request_data(
                        date, static_cast<std::uint8_t>(market_id), code))
                : connection.call(type_expansion_minute,
                    build_expansion_minute_request_data(
                        static_cast<std::uint8_t>(market_id), code));
            auto result = parse_expansion_minute_payload(response.data, date ? 20 : 12);
            result["market"] = market;
            result["market_id"] = static_cast<std::uint64_t>(market_id);
            result["code"] = code;
            result["security"] = std::to_string(market_id) + ":" + code;
            result["mode"] = date ? "historical" : "current";
            result["date"] = date ? Json(static_cast<std::uint64_t>(date)) : Json(nullptr);
            result["date_semantics"] = date
                ? "explicit trading date supplied to 0x240C"
                : "current 0x240B response does not carry a trading date";
            result["endpoint"] = endpoint.address();
            result["server_name"] = connection.server_name();
            result["transport"] = date ? "tdx-7727-0x240c" : "tdx-7727-0x240b";
            result["volume_unit"] = "contract_or_security_native_unit";
            return result;
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
    }
    std::string detail = "all 7727 expansion timeline endpoints failed";
    for (const auto& failure : failures) detail += "\n  " + failure;
    throw Error(detail);
}

Json fetch_expansion_trades_document(const std::string& market_option,
                                     const std::string& code_option,
                                     const std::string& date_option,
                                     int start, int page_size, int pages,
                                     int timeout_ms) {
    auto code = trim(code_option);
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    const auto [market, market_id] = normalize_market(market_option, code);
    if (market_id < 3 || market_id > 255)
        throw Error("expansion trades require a TDX expansion market ID 3..255");
    if (!valid_expansion_code(code))
        throw Error("expansion trade code must contain 1..9 ASCII letters, digits, or internal spaces");
    if (start < 0 || page_size < 1 || page_size > 1800 || pages < 1 || pages > 20 ||
        timeout_ms < 1 || timeout_ms > 600000)
        throw Error("expansion trade request is outside the safe range");
    if (static_cast<std::uint64_t>(start) +
        static_cast<std::uint64_t>(page_size) * static_cast<std::uint64_t>(pages - 1) >
        std::numeric_limits<std::uint32_t>::max())
        throw Error("last expansion trade page start exceeds uint32");
    const auto date = normalize_expansion_date(date_option);
    static const std::array<std::string_view, 5> default_hosts{
        "116.205.143.214:7727", "124.71.223.19:7727", "113.45.175.47:7727",
        "112.74.214.43:7727", "119.97.185.5:7727"
    };
    std::vector<Json::Array> downloaded_pages;
    std::vector<std::string> failures;
    std::size_t page_index = 0;
    bool reached_end = false;
    std::string used_endpoint, server_name;
    for (const auto host : default_hosts) {
        if (page_index == static_cast<std::size_t>(pages) || reached_end) break;
        const auto endpoint = parse_endpoint(host);
        try {
            QuoteConnection connection(endpoint, timeout_ms, QuoteProtocol::expansion);
            used_endpoint = endpoint.address();
            server_name = connection.server_name();
            while (page_index < static_cast<std::size_t>(pages)) {
                const auto page_start = static_cast<std::uint32_t>(
                    static_cast<std::uint64_t>(start) + page_index * page_size);
                const auto response = date
                    ? connection.call(type_expansion_history_trade,
                        build_expansion_history_trade_request_data(
                            date, static_cast<std::uint8_t>(market_id), code, page_start,
                            static_cast<std::uint16_t>(page_size)))
                    : connection.call(type_expansion_trade,
                        build_expansion_trade_request_data(
                            static_cast<std::uint8_t>(market_id), code, page_start,
                            static_cast<std::uint16_t>(page_size)));
                const auto parsed = parse_expansion_trade_payload(
                    response.data, static_cast<std::uint8_t>(market_id), page_start);
                const auto& page = parsed.at("trades").as_array();
                downloaded_pages.push_back(page);
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
        std::string detail = "all 7727 expansion trade endpoints failed";
        for (const auto& failure : failures) detail += "\n  " + failure;
        throw Error(detail);
    }
    if (page_index < static_cast<std::size_t>(pages) && !reached_end)
        throw Error("only completed " + std::to_string(page_index) + "/" +
                    std::to_string(pages) + " expansion trade pages");

    Json trades = Json::array();
    for (auto page = downloaded_pages.rbegin(); page != downloaded_pages.rend(); ++page)
        for (const auto& row : *page) trades.push_back(row);
    std::map<std::string, std::uint64_t> nature_counts;
    for (const auto& row : trades.as_array()) ++nature_counts[row.at("nature").as_string()];
    Json nature_summary = Json::object();
    for (const auto& [name, count] : nature_counts)
        nature_summary[name.empty() ? "unknown" : name] = count;
    const auto downloaded = trades.size();
    const auto next_start = static_cast<std::uint64_t>(start) + downloaded;
    Json result = Json::object();
    result["schema"] = "tdx-expansion-trades-v1";
    result["market"] = market;
    result["market_id"] = static_cast<std::uint64_t>(market_id);
    result["code"] = code;
    result["security"] = std::to_string(market_id) + ":" + code;
    result["mode"] = date ? "historical" : "current";
    result["date"] = date ? Json(static_cast<std::uint64_t>(date)) : Json(nullptr);
    result["date_semantics"] = date
        ? "explicit trading date supplied to 0x2406"
        : "current 0x23FC response does not carry a trading date";
    result["start"] = static_cast<std::uint64_t>(start);
    result["page_size"] = static_cast<std::uint64_t>(page_size);
    result["pages_completed"] = static_cast<std::uint64_t>(page_index);
    result["downloaded"] = static_cast<std::uint64_t>(downloaded);
    result["next_start"] = next_start;
    result["has_more"] = !reached_end && page_index == static_cast<std::size_t>(pages);
    result["price_divisor"] = 1000.0;
    result["price_encoding"] = "wire uint32 divided by 1000";
    result["ordering_note"] =
        "trades are chronological; absolute_index is the server offset counting backward from the newest record";
    result["volume_unit"] = "contract_or_security_native_unit";
    result["nature_summary"] = std::move(nature_summary);
    result["trades"] = std::move(trades);
    result["endpoint"] = used_endpoint;
    result["server_name"] = server_name;
    result["transport"] = date ? "tdx-7727-0x2406" : "tdx-7727-0x23fc";
    return result;
}

}  // namespace tdx
