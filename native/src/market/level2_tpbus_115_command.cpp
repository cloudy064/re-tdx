#include "level2_tpbus_115.hpp"

#include "tdx/common.hpp"
#include "tdx/level2.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace tdx::level2_detail {
namespace {

constexpr std::uintmax_t maximum_payload_size = level2_offline_payload_limit;
constexpr std::uintmax_t maximum_hex_text_size =
    maximum_payload_size * 2U + 64U * 1024U;

std::uintmax_t checked_file_size(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error)
        throw Error("cannot inspect tpbus-115 input file size: " +
                    error.message());
    return size;
}

Bytes parse_bounded_hex(std::string text) {
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), text.end());
    if (text.size() % 2)
        throw Error("tpbus-115 hex input must contain an even number of digits");
    auto digit = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    Bytes result;
    result.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2) {
        const int high = digit(text[index]);
        const int low = digit(text[index + 1]);
        if (high < 0 || low < 0)
            throw Error("tpbus-115 hex input contains a non-hex character");
        result.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return result;
}

}  // namespace

Bytes read_level2_tpbus_115_cli_input(const fs::path& path,
                                      const std::string& encoding) {
    const auto file_size = checked_file_size(path);
    if (encoding == "raw") {
        if (file_size > maximum_payload_size)
            throw Error("tpbus-115 input exceeds the 384 KiB offline safety limit");
        auto payload = read_bytes(path);
        if (payload.size() > maximum_payload_size)
            throw Error("tpbus-115 input changed while it was being read");
        return payload;
    }
    if (encoding != "hex") throw Error("encoding must be raw or hex");
    if (file_size > maximum_hex_text_size)
        throw Error("tpbus-115 hex input exceeds its bounded pre-read limit");
    auto payload = parse_bounded_hex(read_text_utf8(path));
    if (payload.size() > maximum_payload_size)
        throw Error("tpbus-115 decoded input exceeds the 384 KiB offline safety limit");
    return payload;
}

}  // namespace tdx::level2_detail
