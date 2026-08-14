#pragma once

#include "tdx/stats.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::stats_detail {

inline constexpr std::uint16_t type_file_content = 0x06B9;
inline constexpr std::size_t file_path_size = 300;
inline constexpr std::uint32_t maximum_chunk_size = 60000;
inline constexpr std::size_t maximum_archive_bytes = 32 * 1024 * 1024;
inline constexpr std::size_t maximum_entry_bytes = 32 * 1024 * 1024;
inline constexpr std::size_t maximum_uncompressed_bytes = 64 * 1024 * 1024;
inline constexpr std::size_t maximum_archive_entries = 128;

struct SecurityCode {
    int market_id{};
    std::string code;

    std::pair<int, std::string> key() const { return {market_id, code}; }
};

std::string now_text();
int parse_integer(const std::string& text, std::string_view name,
                  int minimum, int maximum);
void validate_security(int market_id, std::string_view code);
SecurityCode parse_security(std::string value);
std::string security_id(int market_id, const std::string& code);

const Json* record_for(const Json* document, int market_id,
                       const std::string& code);

}  // namespace tdx::stats_detail

