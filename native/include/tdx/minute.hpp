#pragma once

#include "tdx/json.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct MinuteBar {
    int date{};
    int hour{};
    int minute{};
    float open{};
    float high{};
    float low{};
    float close{};
    float amount{};
    bool amount_available{true};
    // Aggregated online daily/weekly volumes can exceed either 32-bit range.
    std::int64_t volume{};
    std::uint16_t extra_1{};
    std::uint16_t extra_2{};
    std::uint32_t open_interest{};
    float auxiliary_price{};
    bool has_expansion_fields{};
};

struct MinuteSeries {
    std::string market;
    std::string code;
    std::string name;
    std::filesystem::path source;
    std::vector<MinuteBar> bars;
};

struct ExpansionInstrument {
    std::uint8_t category{};
    std::uint8_t market_id{};
    std::string code;
    std::string name;
    std::string description;
    // 0x23F5 record offset 56.  For derivative categories this is the
    // underlying-unit count per contract exposed by TCalc as MULTIPLIER.
    std::uint32_t contract_multiplier{};
};

// Expansion-market option identifiers are fixed-width wire codes.  TDX uses
// an internal ASCII space in some of those identifiers (for example
// "A 8X06SH"), so their validation differs from ordinary stock/futures codes.
bool valid_expansion_code(std::string_view code);

int decode_lc1_date(std::uint16_t value);
std::vector<MinuteBar> parse_lc1(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> build_kline_request_data(
    std::uint16_t market_id, std::string_view code,
    std::uint16_t start, std::uint16_t count,
    std::uint16_t period = 7,
    std::uint16_t period_parameter = 1);
std::vector<MinuteBar> parse_kline_payload(
    const std::vector<std::uint8_t>& payload, bool index_mode,
    std::uint16_t period = 7);
std::vector<std::uint8_t> build_expansion_kline_request_data(
    std::uint8_t market_id, std::string_view code,
    std::uint32_t start, std::uint16_t count,
    std::uint16_t period = 7);
std::vector<MinuteBar> parse_expansion_kline_payload(
    const std::vector<std::uint8_t>& payload, std::uint16_t period = 7);
std::uint32_t parse_expansion_instrument_count_payload(
    const std::vector<std::uint8_t>& payload);
std::vector<ExpansionInstrument> parse_expansion_instrument_info_payload(
    const std::vector<std::uint8_t>& payload);
Json parse_expansion_quote_payload(const std::vector<std::uint8_t>& payload);
std::vector<std::uint8_t> build_expansion_minute_request_data(
    std::uint8_t market_id, std::string_view code);
std::vector<std::uint8_t> build_expansion_history_minute_request_data(
    std::uint32_t date, std::uint8_t market_id, std::string_view code);
Json parse_expansion_minute_payload(const std::vector<std::uint8_t>& payload,
                                    int header_bytes = 12);
std::vector<std::uint8_t> build_expansion_trade_request_data(
    std::uint8_t market_id, std::string_view code,
    std::uint32_t start, std::uint16_t count);
std::vector<std::uint8_t> build_expansion_history_trade_request_data(
    std::uint32_t date, std::uint8_t market_id, std::string_view code,
    std::uint32_t start, std::uint16_t count);
Json parse_expansion_trade_payload(const std::vector<std::uint8_t>& payload,
                                   std::uint8_t market_id,
                                   std::uint32_t start = 0);
std::vector<std::uint8_t> pack_lc1(const std::vector<MinuteBar>& bars);
std::filesystem::path locate_lc1(
    const std::filesystem::path& root,
    std::string_view market,
    std::string_view code,
    bool index_mode
);
std::string render_minute_csv(const MinuteSeries& series);
std::string render_minute_json(const MinuteSeries& series, bool pretty = true);
Json load_minute_document(const std::filesystem::path& root,
                          const std::string& market,
                          const std::string& code,
                          bool index_mode,
                          const std::string& date = "latest");
Json load_local_kline_document(const std::filesystem::path& root,
                               const std::string& market,
                               const std::string& code,
                               const std::string& kind,
                               const std::string& period,
                               int pages = 1,
                               int page_size = 800,
                               int start = 0,
                               const std::string& date = "all");
Json fetch_minute_document(const std::string& market,
                           const std::string& code,
                           const std::string& kind = "auto",
                           int pages = 1,
                           int page_size = 800,
                           int start = 0,
                           const std::string& date = "latest",
                           int timeout_ms = 10000,
                           const std::filesystem::path& root = {},
                           const std::vector<std::string>& hosts = {});
Json fetch_kline_document(const std::string& market,
                          const std::string& code,
                          const std::string& kind,
                          const std::string& period,
                          int pages = 1,
                          int page_size = 800,
                          int start = 0,
                          const std::string& date = "latest",
                          int timeout_ms = 10000,
                          const std::filesystem::path& root = {},
                          const std::vector<std::string>& hosts = {});
Json fetch_expansion_instruments_document(int start = 0,
                                          int count = 100,
                                          int market_filter = -1,
                                          const std::string& query = {},
                                          int timeout_ms = 10000);
// Scan the ordered 7727 directory until an exact market/code record is found.
// Unlike the bulk JSON endpoint this stops immediately and is suitable for
// resolving the category and contract-multiplier fields consumed by the
// formula host context (ISQHQQCODE/MULTIPLIER).
std::optional<ExpansionInstrument> fetch_expansion_instrument_record(
    int market_id,
    const std::string& code,
    int timeout_ms = 10000);
Json fetch_expansion_quote_document(const std::string& market,
                                    const std::string& code,
                                    int timeout_ms = 10000);
Json fetch_expansion_quotes_document(
    const std::vector<std::pair<int, std::string>>& securities,
    int timeout_ms = 10000);
Json fetch_expansion_timeline_document(const std::string& market,
                                       const std::string& code,
                                       const std::string& date = {},
                                       int timeout_ms = 10000);
Json fetch_expansion_trades_document(const std::string& market,
                                     const std::string& code,
                                     const std::string& date = {},
                                     int start = 0,
                                     int page_size = 1800,
                                     int pages = 1,
                                     int timeout_ms = 10000);
int command_minute_extract(const std::vector<std::string>& args);
int command_minute_download(const std::vector<std::string>& args);
int command_market_instruments(const std::vector<std::string>& args);
int command_market_expansion_quote(const std::vector<std::string>& args);
int command_market_expansion_timeline(const std::vector<std::string>& args);
int command_market_expansion_trades(const std::vector<std::string>& args);

}  // namespace tdx
