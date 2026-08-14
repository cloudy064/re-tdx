#include "level2_sdk_180x_host_projection_command.hpp"

#include "tdx/level2.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::level2_detail {
namespace {

constexpr std::uintmax_t maximum_body_size = level2_offline_payload_limit;
constexpr std::uintmax_t maximum_hex_file_size = maximum_body_size * 2U;

fs::path projection_path_from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::uintmax_t checked_file_size(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error)
        throw Error(
            "level2 SDK 1801/1802 host projection cannot inspect --input: " +
            error.message());
    return size;
}

int signed_integer(const std::string& value, const char* option) {
    int result{};
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size())
        throw Error(std::string(option) + " must be a signed integer");
    return result;
}

bool ascii_space(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

int hex_digit(unsigned char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Bytes parse_projection_hex(std::string_view text) {
    std::string digits;
    digits.reserve(text.size());
    for (const auto value : text) {
        const auto byte = static_cast<unsigned char>(value);
        if (ascii_space(byte)) continue;
        if (hex_digit(byte) < 0)
            throw Error(
                "level2 SDK 1801/1802 host projection hex input contains a "
                "non-hexadecimal character");
        digits.push_back(value);
    }
    if (digits.empty() || digits.size() % 2 != 0)
        throw Error(
            "level2 SDK 1801/1802 host projection hex input must contain a "
            "non-empty even number of digits");
    if (digits.size() / 2 > maximum_body_size)
        throw Error(
            "level2 SDK 1801/1802 host projection decoded body exceeds the "
            "384 KiB limit");

    Bytes result;
    result.reserve(digits.size() / 2);
    for (std::size_t index = 0; index < digits.size(); index += 2) {
        const auto high = hex_digit(
            static_cast<unsigned char>(digits[index]));
        const auto low = hex_digit(
            static_cast<unsigned char>(digits[index + 1]));
        result.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return result;
}

Bytes read_projection_body(const fs::path& path,
                           const std::string& encoding) {
    const auto size = checked_file_size(path);
    if (encoding == "raw") {
        if (size > maximum_body_size)
            throw Error(
                "level2 SDK 1801/1802 host projection raw body exceeds the "
                "384 KiB limit");
        return read_bytes(path);
    }
    if (encoding != "hex")
        throw Error(
            "level2 SDK 1801/1802 host projection encoding must be raw or hex");
    if (size > maximum_hex_file_size)
        throw Error(
            "level2 SDK 1801/1802 host projection hex input exceeds the "
            "768 KiB text limit");
    return parse_projection_hex(read_text_utf8(path));
}

}  // namespace

int command_level2_sdk_180x_host_projection(Args& args, int data_type) {
    if (data_type != 1801 && data_type != 1802)
        throw Error("internal SDK 1801/1802 host projection data type is invalid");
    const auto type = std::to_string(data_type);
    const auto input_name = args.take_option("--input");
    if (input_name.empty())
        throw Error("level2 SDK " + type +
                    " host projection requires --input BODY_FILE");
    if (!args.has("--encoding"))
        throw Error("level2 SDK " + type +
                    " host projection requires --encoding raw or hex");
    if (!args.has("--market"))
        throw Error("level2 SDK " + type +
                    " host projection requires --market 0..2");
    const auto code = trim(args.take_option("--code"));
    if (code.empty())
        throw Error("level2 SDK " + type +
                    " host projection requires --code 6DIGITS");

    const auto encoding = lower_ascii(trim(args.take_option("--encoding")));
    const auto market = signed_integer(
        args.take_option("--market"), "--market");
    if (market < 0 || market > 2)
        throw Error("--market must be 0, 1, or 2");
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    auto body = read_projection_body(
        projection_path_from_utf8(input_name), encoding);
    Json result;
    if (data_type == 1801) {
        Level2Sdk1801HostProjectionRequest request;
        request.market_id = static_cast<std::uint16_t>(market);
        request.code = code;
        request.body = std::move(body);
        result = project_level2_sdk_1801_host_projection(request);
    } else {
        Level2Sdk1802HostProjectionRequest request;
        request.market_id = static_cast<std::uint16_t>(market);
        request.code = code;
        request.body = std::move(body);
        result = project_level2_sdk_1802_host_projection(request);
    }

    const auto rendered = result.dump(compact ? -1 : 2) + "\n";
    if (output_name.empty()) std::cout << rendered;
    else atomic_write_text(projection_path_from_utf8(output_name), rendered);
    return 0;
}

}  // namespace tdx::level2_detail
