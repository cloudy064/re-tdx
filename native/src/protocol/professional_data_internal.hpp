#pragma once

#include "tdx/professional_data.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::professional_data_detail {

inline constexpr std::string_view data_root = "https://data.tdx.com.cn/";
inline constexpr std::size_t finance_header_size = 20;
inline constexpr std::size_t finance_index_size = 11;
inline constexpr std::size_t trading_record_size = 13;
inline constexpr std::size_t maximum_archive_bytes = 64 * 1024 * 1024;
inline constexpr std::size_t maximum_finance_bytes = 256 * 1024 * 1024;
inline constexpr std::size_t maximum_trading_bytes = 8 * 1024 * 1024;

struct Security {
    int market_id{};
    std::string market;
    std::string code;

    std::string id() const;
};

struct ManifestEntry {
    std::string name;
    std::string md5;
    std::size_t size{};
};

struct ZipEntry {
    std::string name;
    std::uint16_t flags{};
    std::uint16_t method{};
    std::uint32_t crc{};
    std::uint32_t compressed_size{};
    std::uint32_t uncompressed_size{};
    std::uint32_t local_offset{};
};

std::filesystem::path default_cache_directory();
std::uint32_t current_local_date();
bool valid_code(std::string_view code);
int inferred_market_id(std::string_view code);
Security parse_security(std::string value);
int parse_integer(const std::string& text, std::string_view name,
                  int minimum, int maximum);
std::uint32_t parse_date(const std::string& text, std::string_view name,
                         std::uint32_t fallback);
std::string date_text(std::uint32_t value);
std::uint16_t u16_at(const Bytes& data, std::size_t offset,
                     std::string_view context);
std::uint32_t u32_at(const Bytes& data, std::size_t offset,
                     std::string_view context);
std::optional<double> finite_float(const std::uint8_t* data);
Json number_json(const std::optional<double>& value);

std::vector<ManifestEntry> parse_manifest(const Bytes& payload);
std::vector<ManifestEntry> fetch_manifest(std::string_view path,
                                          int timeout_ms, std::size_t limit);
const ManifestEntry& find_manifest_entry(
    const std::vector<ManifestEntry>& entries, const std::string& name);
Bytes load_verified_resource(const ManifestEntry& entry,
                             std::string_view remote_directory,
                             std::filesystem::path cache_directory,
                             int timeout_ms, bool refresh,
                             std::size_t maximum_bytes);

std::vector<ZipEntry> zip_entries(const Bytes& archive);
Bytes extract_zip_entry(const Bytes& archive, const ZipEntry& entry);
Bytes extract_finance_member(const Bytes& archive, const std::string& wanted);

const std::map<int, std::string>& stock_names();
const std::map<int, std::string>& market_names();
const std::map<int, std::string>& board_names();
std::string field_name(std::string_view kind, int id);
Json catalog_document(int timeout_ms);

}  // namespace tdx::professional_data_detail
