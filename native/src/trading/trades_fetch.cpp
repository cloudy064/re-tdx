#include "tdx/trades.hpp"
#include "tdx/trades_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/session_audit.hpp"
#include "tdx/transport.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <set>

namespace fs = std::filesystem;

namespace tdx {

namespace detail {
namespace trades {

TradeSeries download_one(QuoteConnection& connection, const SecurityCode& code,
                         const std::string& trading_date,
                         const std::string& server_trade_date,
                         std::uint16_t page_size, int max_page_count) {
    std::vector<std::vector<TradeTick>> pages;
    std::uint32_t start = 0;
    std::optional<float> price_base;
    while (true) {
        if (start > 0xFFFF) throw Error(security_id(code.market_id, code.code) +
                                       " trade cursor exceeds uint16");
        const auto cursor = static_cast<std::uint16_t>(start);
        ResponseFrame response;
        TradePage page;
        if (trading_date.empty()) {
            response = connection.call(type_today_trades,
                build_today_trades_request_data(code.market_id, code.code, cursor, page_size));
            page = parse_today_trades_payload(response.data, code.market_id, code.code,
                                               cursor, page_size, server_trade_date);
        } else {
            response = connection.call(type_history_trades,
                build_history_trades_request_data(code.market_id, code.code, trading_date,
                                                  cursor, page_size));
            page = parse_history_trades_payload(response.data, code.market_id, code.code,
                                                 trading_date, cursor, page_size);
        }
        if (!price_base && page.price_base_raw) price_base = page.price_base_raw;
        if (page.ticks.empty()) break;
        if (static_cast<int>(pages.size()) >= max_page_count)
            throw Error(security_id(code.market_id, code.code) + " trades exceed " +
                        std::to_string(max_page_count) + " page safety limit");
        start += static_cast<std::uint32_t>(page.ticks.size());
        if (start > 0xFFFF)
            throw Error(security_id(code.market_id, code.code) +
                        " trade paging exceeds uint16 cursor");
        pages.push_back(std::move(page.ticks));
    }
    TradeSeries series;
    series.market_id = code.market_id;
    series.code = code.code;
    series.trading_date = trading_date.empty() ? server_trade_date : trading_date;
    series.source_mode = trading_date.empty() ? "today" : "history";
    series.pages = pages.size();
    series.page_size = page_size;
    series.price_divisor = trade_price_divisor(code.code);
    series.price_base_raw = price_base;
    std::size_t total = 0;
    for (const auto& page : pages) total += page.size();
    series.ticks.reserve(total);
    for (auto page = pages.rbegin(); page != pages.rend(); ++page)
        series.ticks.insert(series.ticks.end(), page->begin(), page->end());
    return series;
}

}  // namespace trades
}  // namespace detail

Json fetch_market_trades_document(const fs::path& root,
                                  const std::vector<std::string>& securities,
                                  const std::string& raw_trading_date,
                                  int raw_page_size, int max_page_count,
                                  int timeout_ms, const BlockData* block_data,
                                  const std::vector<std::string>& hosts) {
    using detail::trades::SecurityCode;
    using detail::trades::download_one;
    using detail::trades::maximum_pages;
    using detail::trades::normalize_date;
    using detail::trades::now_text;
    using detail::trades::parse_security;
    using detail::trades::series_json;
    using detail::trades::type_heartbeat;

    if (securities.empty()) throw Error("at least one trade security is required");
    const auto trading_date = raw_trading_date.empty() ? std::string{} :
        normalize_date(raw_trading_date);
    const int page_size_value = raw_page_size ? raw_page_size :
        (trading_date.empty() ? 1800 : 2000);
    if (page_size_value < 1 || page_size_value > 0xFFFF)
        throw Error("trade page size must be in 1..65535");
    if (max_page_count < 1 || max_page_count > maximum_pages)
        throw Error("trade max pages must be in 1..100");
    if (timeout_ms < 1 || timeout_ms > 600000) throw Error("timeout must be positive");

    std::vector<SecurityCode> codes;
    std::set<std::pair<int, std::string>> seen;
    for (const auto& item : securities) {
        auto code = parse_security(item);
        if (seen.insert(code.key()).second) codes.push_back(std::move(code));
    }
    const auto endpoint_selection = select_public_quote_endpoints(root, hosts);

    std::vector<TradeSeries> series;
    std::vector<std::string> failures;
    std::size_t next = 0;
    std::string endpoint_used, server_name, server_trade_date;
    int connection_attempts = 0, transient_retries = 0, endpoints_attempted = 0;
    for (const auto& endpoint : endpoint_selection.endpoints) {
        if (next >= codes.size()) break;
        ++endpoints_attempted;
        int attempts = 0;
        try {
            detail::retry_quote_transport([&] {
                QuoteConnection connection(endpoint, timeout_ms);
                const auto heartbeat = connection.call(type_heartbeat);
                if (heartbeat.data.size() < 10)
                    throw Error("heartbeat has no server trade date");
                server_trade_date = normalize_date(
                    std::to_string(read_u32_le(heartbeat.data.data() + 6)));
                while (next < codes.size()) {
                    series.push_back(download_one(
                        connection, codes[next], trading_date, server_trade_date,
                        static_cast<std::uint16_t>(page_size_value), max_page_count));
                    ++next;
                    endpoint_used = endpoint.address();
                    server_name = connection.server_name();
                }
                return true;
            }, attempts);
        } catch (const std::exception& error) {
            failures.push_back(endpoint.address() + ": " + error.what());
        }
        connection_attempts += attempts;
        transient_retries += std::max(0, attempts - 1);
    }
    if (next < codes.size()) {
        std::string detail = "trade details completed " + std::to_string(next) + "/" +
                             std::to_string(codes.size());
        for (const auto& failure : failures) detail += "\n  " + failure;
        throw Error(detail);
    }
    const auto loaded_blocks = block_data ? BlockData{} : load_blocks(root, {});
    const auto& names = block_data ? block_data->securities : loaded_blocks.securities;
    Json records = Json::array();
    std::uint64_t tick_count = 0;
    for (const auto& item : series) {
        tick_count += static_cast<std::uint64_t>(item.ticks.size());
        records.push_back(series_json(item, names));
    }
    Json document = Json::object();
    document["schema"] = "tdx-trades-native-v1";
    document["generated_at"] = now_text();
    document["command"] = trading_date.empty() ? "0x0FC5" : "0x0FC6";
    document["endpoint"] = endpoint_used;
    document["server_name"] = server_name;
    document["transport"] = public_quote_transport_document(
        endpoint_selection, connection_attempts, transient_retries,
        endpoints_attempted);
    document["server_trade_date"] = server_trade_date;
    document["requested"] = static_cast<std::uint64_t>(codes.size());
    document["received"] = static_cast<std::uint64_t>(series.size());
    document["tick_count"] = tick_count;
    document["data_scope_note"] =
        "公开 L1 成交明细，时间精度为分钟；不是 Level2 秒级逐笔成交或委托";
    document["volume_note"] = "个股成交量单位为手，金额=价格×成交量×100";
    document["ordering_note"] =
        "ticks 按时间正序；absolute_index 是服务端从最新记录向历史回溯的偏移";
    document["status_note"] =
        "status=0/1/2 为买/卖/中性；深市 status=5 可表示 15:05—15:30 盘后定价成交";
    document["records"] = std::move(records);
    return document;
}

}  // namespace tdx
