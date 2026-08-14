#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tdx {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

using Bytes = std::vector<std::uint8_t>;

std::wstring utf8_to_wide(std::string_view value);
std::string wide_to_utf8(std::wstring_view value);
Bytes encode_gbk(std::string_view value);
std::string decode_gbk(const Bytes& value);
std::string lower_ascii(std::string value);
std::string trim(std::string value);
std::vector<std::string> split(std::string_view value, char delimiter);

Bytes read_bytes(const std::filesystem::path& path);
std::string read_text_utf8(const std::filesystem::path& path);
void atomic_write_bytes(const std::filesystem::path& path, const Bytes& value);
void atomic_write_text(const std::filesystem::path& path, std::string_view value);
std::string sha256_file(const std::filesystem::path& path);
std::string md5_file(const std::filesystem::path& path);
std::string md5_bytes(const Bytes& value);

std::filesystem::path running_executable_path();
std::filesystem::path find_tdx_root(const std::filesystem::path& explicit_root = {});
std::string path_utf8(const std::filesystem::path& path);

class Args {
public:
    explicit Args(std::vector<std::string> values);

    bool take_flag(std::string_view name);
    std::string take_option(std::string_view name, std::string fallback = {});
    std::vector<std::string> take_options(std::string_view name);
    bool has(std::string_view name) const;
    const std::vector<std::string>& remaining() const noexcept;
    void require_empty() const;

private:
    std::vector<std::string> values_;
};

std::uint16_t read_u16_le(const std::uint8_t* data);
std::uint32_t read_u32_le(const std::uint8_t* data);
std::int32_t read_i32_le(const std::uint8_t* data);
float read_f32_le(const std::uint8_t* data);

}  // namespace tdx
