#pragma once

#include "tdx/corporate.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::corporate_detail {

inline constexpr std::uint16_t type_finance = 0x0010;
inline constexpr std::uint16_t type_capital_changes = 0x000F;
inline constexpr std::uint16_t type_special_limits = 0x0452;
inline constexpr std::size_t finance_record_size = 143;
inline constexpr std::size_t finance_info_size = 136;
inline constexpr std::size_t capital_record_size = 29;
inline constexpr std::size_t limit_record_size = 13;

struct SecurityCode {
    int market_id{};
    std::string code;

    std::pair<int, std::string> key() const { return {market_id, code}; }
    std::string market() const {
        return market_id == 0 ? "sz" : market_id == 1 ? "sh" :
               market_id == 2 ? "bj" : std::to_string(market_id);
    }
    std::string display() const {
        return market_id == 0 ? "SZ" + code : market_id == 1 ? "SH" + code :
               market_id == 2 ? "BJ" + code : std::to_string(market_id) + ":" + code;
    }
};

std::string now_text();
std::filesystem::path native_path(const std::string& value);
Json date_json(std::uint32_t raw);
Json capital_record_json(const std::uint8_t* record, bool include_raw);
SecurityCode parse_security(std::string value);
SecurityCode parse_kline_security(std::string value);
std::vector<SecurityCode> parse_securities(const std::vector<std::string>& values);
Bytes finance_request(const std::vector<SecurityCode>& securities);
Bytes capital_request(const SecurityCode& security);
Bytes limits_request(int start_index);

std::vector<Endpoint> effective_endpoints(const std::vector<Endpoint>& values);
Json quote_transport_metadata(
    int connection_attempts,
    int transient_retries,
    int endpoints_attempted,
    const std::vector<Endpoint>& candidates);
int bounded_integer(
    const std::string& text,
    std::string_view name,
    int minimum,
    int maximum);
std::vector<Endpoint> command_endpoints(
    Args& args,
    const std::filesystem::path& root);
void write_document(
    const std::filesystem::path& output,
    const Json& document,
    bool compact);

const Json* object_value(const Json& object, std::string_view key);
std::string normalized_date(std::string value, bool required);

}  // namespace tdx::corporate_detail
