#pragma once

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/market.hpp"
#include "tdx/market_stream.hpp"
#include "tdx/session_audit.hpp"
#include "tdx/transport.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::market_detail {

enum class QuoteSurface { Snapshot, Speed, Depth };

struct QuoteSurfaceDefinition {
    QuoteSurface surface;
    std::uint16_t command;
    std::string_view command_text;
    std::string_view schema;
};

inline constexpr std::array<QuoteSurfaceDefinition, 3> quote_surface_definitions{{
    {QuoteSurface::Snapshot, 0x054C, "0x054C", "tdx-market-snapshot-native-v1"},
    {QuoteSurface::Speed, 0x053E, "0x053E", "tdx-market-speed-native-v1"},
    {QuoteSurface::Depth, 0x0547, "0x0547", "tdx-market-depth-native-v1"},
}};

inline constexpr std::uint8_t depth_xor = 0x93;

inline constexpr std::array<std::string_view, 3> market_prefixes{"SZ", "SH", "BJ"};

struct PriceDivisorRule {
    std::string_view prefix;
    int divisor;
};

inline constexpr std::array<PriceDivisorRule, 13> price_divisor_rules{{
    {"10", 100}, {"11", 100}, {"12", 100}, {"204", 100}, {"1318", 100},
    {"15", 10},  {"16", 10},  {"50", 10},  {"51", 10},  {"52", 10},
    {"53", 10},  {"56", 10},  {"58", 10},
}};

const QuoteSurfaceDefinition &quote_surface(QuoteSurface surface);

struct QuoteCode {
    int market_id{};
    std::string code;

    std::pair<int, std::string> key() const { return {market_id, code}; }
    std::string display() const;
};

struct PriceFields {
    std::int64_t current{}, previous{}, open{}, high{}, low{};
};

struct Snapshot {
    QuoteCode security;
    std::uint16_t active{};
    double last{}, previous{}, open{}, high{}, low{};
    std::int64_t time_raw{}, auxiliary_price_delta_raw{}, total_hand{}, current_hand{};
    double amount{};
    std::int64_t inside{}, outside{}, auction_imbalance_hand{};
    double open_amount{};
    std::optional<double> fund_iopv;
};

struct Level {
    double price{};
    std::int64_t volume{};
};

struct Speed {
    Snapshot quote;
    std::array<Level, 5> buys{}, sells{};
    std::uint16_t status{};
    std::uint8_t extension_marker{};
    std::array<std::int64_t, 3> extension_values{};
    std::int16_t rise_speed_raw{};
    std::uint16_t tail_raw{};
};

struct Depth {
    Snapshot quote;
    std::uint32_t update_time{};
    std::int64_t status{}, unknown_after_outer{};
    std::array<Level, 5> buys{}, sells{};
    // Ordinary records expose price deltas. SH999997 deliberately carries
    // market-wide order-amount aggregates in the same wire slots.
    std::array<std::int64_t, 5> first_level_raw{}, second_level_raw{};
    std::string tail_hex;
};

struct CommonOptions {
    std::vector<QuoteCode> codes;
    std::vector<Endpoint> endpoints;
    std::string endpoint_source;
    std::size_t available_endpoint_count{};
    bool primary_configured{};
    int timeout_ms{}, batch_size{};
    std::filesystem::path root, output;
    bool compact{};
};

template <typename Row>
struct BatchDownload {
    std::vector<Row> rows;
    std::string endpoint;
    std::string server;
    int connection_attempts{};
    int transient_retries{};
    int endpoints_attempted{};
};

QuoteCode parse_security(std::string value);
Bytes snapshot_request(const std::vector<QuoteCode> &codes);
Bytes depth_request(const std::vector<QuoteCode> &codes);
std::vector<Snapshot> parse_snapshots(const Bytes &payload, std::size_t requested);
std::vector<Speed> parse_speeds(const Bytes &payload, std::size_t requested);
std::vector<Depth> parse_depths(Bytes payload, const std::vector<QuoteCode> &requested);

Json snapshot_json(
    const Snapshot &item,
    const std::map<std::pair<int, std::string>, Security> &names);
Json speed_json(
    const Speed &item,
    const std::map<std::pair<int, std::string>, Security> &names);
Json depth_json(
    const Depth &item,
    const std::map<std::pair<int, std::string>, Security> &names);

std::filesystem::path from_utf8(const std::string &value);
int positive_integer(const std::string &value, std::string_view option, int maximum);
int bounded_integer(const std::string &value, std::string_view option, int minimum,
                    int maximum);
std::string now_text();
CommonOptions parse_common_options(Args &args, std::string default_output);
MarketL1SessionOptions session_options(const CommonOptions &selected);
std::vector<std::string> display_codes(const CommonOptions &selected);

BatchDownload<Snapshot> download_snapshot_batches(const CommonOptions &selected);
BatchDownload<Speed> download_speed_batches(const CommonOptions &selected);
Json quote_transport_metadata(int connection_attempts, int transient_retries,
                              int endpoints_attempted, const CommonOptions &selected);
Json snapshot_document(
    const BatchDownload<Snapshot> &downloaded, const CommonOptions &selected,
    const std::map<std::pair<int, std::string>, Security> &names);
Json speed_document(
    const BatchDownload<Speed> &downloaded, const CommonOptions &selected,
    const std::map<std::pair<int, std::string>, Security> &names);

} // namespace tdx::market_detail
