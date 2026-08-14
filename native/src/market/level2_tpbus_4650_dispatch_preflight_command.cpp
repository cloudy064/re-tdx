#include "level2_tpbus_4650_dispatch_preflight.hpp"

#include "tdx/level2.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::level2_detail {
namespace {

constexpr std::uintmax_t maximum_raw_size = level2_offline_payload_limit;
constexpr std::uintmax_t maximum_hex_text_size =
    maximum_raw_size * 2U + 64U * 1024U;

fs::path path_from_utf8(const std::string& value) {
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
        throw Error("cannot inspect tpbus-4650 input file size: " +
                    error.message());
    return size;
}

Bytes parse_hex(std::string text) {
    text.erase(std::remove_if(text.begin(), text.end(),
                              [](unsigned char value) {
                                  return std::isspace(value) != 0;
                              }),
               text.end());
    if (text.size() % 2 != 0)
        throw Error(
            "tpbus-4650 hex input must contain an even number of digits");
    auto digit = [](unsigned char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    Bytes result;
    result.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2) {
        const int high = digit(static_cast<unsigned char>(text[index]));
        const int low = digit(static_cast<unsigned char>(text[index + 1]));
        if (high < 0 || low < 0)
            throw Error(
                "tpbus-4650 hex input contains a non-hex character");
        result.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return result;
}

Bytes read_input(const fs::path& path, const std::string& encoding) {
    const auto size = checked_file_size(path);
    if (encoding == "raw") {
        if (size > maximum_raw_size)
            throw Error(
                "tpbus-4650 input exceeds the 384 KiB offline safety limit");
        auto result = read_bytes(path);
        if (result.size() != size)
            throw Error("tpbus-4650 input changed while it was being read");
        return result;
    }
    if (encoding != "hex")
        throw Error("tpbus-4650 encoding must be raw or hex");
    if (size > maximum_hex_text_size)
        throw Error(
            "tpbus-4650 hex input exceeds its bounded pre-read limit");
    auto result = parse_hex(read_text_utf8(path));
    if (result.size() > maximum_raw_size)
        throw Error(
            "tpbus-4650 decoded input exceeds the 384 KiB offline safety limit");
    return result;
}

std::uint32_t raw_u32(const std::string& value, const char* option) {
    std::uint64_t parsed_value{};
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), parsed_value);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size() ||
        parsed_value > std::numeric_limits<std::uint32_t>::max())
        throw Error(std::string(option) + " must be a u32 integer");
    return static_cast<std::uint32_t>(parsed_value);
}

}  // namespace

int command_level2_tpbus_4650_dispatch_preflight(Args& args) {
    const auto input_name = args.take_option("--input");
    if (input_name.empty())
        throw Error(
            "tpbus-4650 dispatch preflight requires --input FILE");
    if (!args.has("--encoding"))
        throw Error(
            "tpbus-4650 dispatch preflight requires --encoding raw or hex");
    const auto encoding = lower_ascii(trim(args.take_option("--encoding")));
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    Level2Tpbus4650DispatchPreflightRequest request;
    request.raw = read_input(path_from_utf8(input_name), encoding);
    const auto rendered =
        level2_tpbus_4650_dispatch_preflight_document(request)
            .dump(compact ? -1 : 2) +
        "\n";
    if (output_name.empty()) std::cout << rendered;
    else atomic_write_text(path_from_utf8(output_name), rendered);
    return 0;
}

int command_level2_tpbus_4650_price_primitives(Args& args) {
    const auto input_name = args.take_option("--input");
    if (input_name.empty())
        throw Error("tpbus-4650 price primitives require --input FILE");
    if (!args.has("--encoding"))
        throw Error(
            "tpbus-4650 price primitives require --encoding raw or hex");
    constexpr const char* mode_option = "--target-market-or-mode-raw";
    if (!args.has(mode_option))
        throw Error(
            "tpbus-4650 price primitives require "
            "--target-market-or-mode-raw U32");
    const auto encoding = lower_ascii(trim(args.take_option("--encoding")));
    const auto mode = raw_u32(args.take_option(mode_option), mode_option);
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    Level2Tpbus4650PricePrimitivesRequest request;
    request.raw = read_input(path_from_utf8(input_name), encoding);
    request.target_market_or_mode_raw = mode;
    const auto rendered = project_level2_tpbus_4650_price_primitives(request)
                              .dump(compact ? -1 : 2) +
                          "\n";
    if (output_name.empty()) std::cout << rendered;
    else atomic_write_text(path_from_utf8(output_name), rendered);
    return 0;
}

}  // namespace tdx::level2_detail
