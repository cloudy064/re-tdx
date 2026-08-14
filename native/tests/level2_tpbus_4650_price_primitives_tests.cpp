#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

namespace {

constexpr std::size_t header_size = 96;
constexpr std::size_t snapshot_size = 120;

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_throws(const std::function<void()>& action,
                    const char* expected_fragment) {
    try {
        action();
    } catch (const tdx::Error& error) {
        require(std::string(error.what()).find(expected_fragment) !=
                    std::string::npos,
                "failure contains the expected contract detail");
        return;
    }
    throw tdx::Error("expected contract validation failure");
}

void write_u32(tdx::Bytes& bytes, std::size_t offset,
               std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24U);
}

void write_f32(tdx::Bytes& bytes, std::size_t offset, float value) {
    std::uint32_t raw{};
    std::memcpy(&raw, &value, sizeof(raw));
    write_u32(bytes, offset, raw);
}

tdx::Bytes fixture(std::int16_t market, const std::string& code,
                   std::uint32_t base, std::uint32_t fraction,
                   float auxiliary) {
    tdx::Bytes raw(header_size + snapshot_size, 0);
    raw[0] = 1;
    const auto market_raw = static_cast<std::uint16_t>(market);
    raw[8] = static_cast<std::uint8_t>(market_raw & 0xffU);
    raw[9] = static_cast<std::uint8_t>(market_raw >> 8U);
    if (code.size() >= header_size - 10)
        throw tdx::Error("invalid fixture code");
    for (std::size_t index = 0; index < code.size(); ++index)
        raw[10 + index] = static_cast<std::uint8_t>(code[index]);
    write_u32(raw, header_size + 36, base);
    write_u32(raw, header_size + 54, fraction);
    write_f32(raw, header_size + 114, auxiliary);
    return raw;
}

tdx::Json project(tdx::Bytes raw, std::uint32_t mode) {
    tdx::Level2Tpbus4650PricePrimitivesRequest request;
    request.raw = std::move(raw);
    request.target_market_or_mode_raw = mode;
    return tdx::project_level2_tpbus_4650_price_primitives(request);
}

void require_offline(const tdx::Json& result) {
    require(!result.at("input_body_retained").as_bool() &&
                !result.at("raw_body_emitted").as_bool() &&
                !result.at("host_identity_accessed").as_bool() &&
                !result.at("host_identity_comparison_executed").as_bool() &&
                !result.at("previous_host_state_accessed").as_bool() &&
                !result.at("host_clock_accessed").as_bool() &&
                !result.at("handler_executed").as_bool() &&
                !result.at("host_state_write_performed").as_bool() &&
                !result.at("sdk_called").as_bool() &&
                !result.at("callback_executed").as_bool() &&
                !result.at("host_message_dispatch_attempted").as_bool() &&
                result.at("host_messages_sent").as_number() == 0.0 &&
                !result.at("wire_bytes_built").as_bool() &&
                result.at("network_requests").as_number() == 0.0 &&
                !result.at("request_sent").as_bool() &&
                !result.at("subscription_sent").as_bool() &&
                !result.at("authorization_attempted").as_bool() &&
                !result.at("credentials_accessed").as_bool() &&
                !result.at("entitlement_bypass").as_bool() &&
                result.at("offline").as_bool(),
            "4650 price projection remains pure offline analysis");
}

void test_generic_mode_and_fraction() {
    auto raw = fixture(0, "000001", 100, 25, 50.0F);
    std::fill(raw.begin() + header_size + 60,
              raw.begin() + header_size + 100,
              static_cast<std::uint8_t>('Z'));
    auto result = project(std::move(raw), 0);
    require(result.at("schema").as_string() ==
                "tdx-level2-tpbus-4650-price-primitives-v1" &&
                result.at("format").as_string() ==
                    "tpbus-4650-price-primitives" &&
                result.at("function_id").as_number() == 4650.0 &&
                result.at("dispatch_shape_qualified").as_bool() &&
                !result.at("full_dispatcher_qualified").as_bool() &&
                !result.at("full_handler_state_projection_performed")
                     .as_bool(),
            "4650 price schema separates the closed subprojection");
    require(!result.at("identity")
                 .at("fractional_security_branch")
                 .as_bool() &&
                result.at("projected_float_price").at("value").as_number() ==
                    100.25 &&
                result.at("projected_integer_price_raw_u32").as_number() ==
                    100.0,
            "generic mode zero consumes the native 1..99 fraction");
    require(result.at("quote_snapshot").at("offset").as_number() == 96.0 &&
                result.at("quote_snapshot").at("byte_size").as_number() ==
                    120.0 &&
                result.at("quote_snapshot").at("sha256").as_string().size() ==
                    64 &&
                !result.at("quote_snapshot").at("body_emitted").as_bool(),
            "4650 fingerprints but does not emit the quote snapshot");
    require(result.dump(-1).find(std::string(40, 'Z')) == std::string::npos,
            "4650 price projection never echoes unrelated snapshot bytes");
    require_offline(result);

    result = project(fixture(0, "000001", 100, 25, 50.0F), 1);
    require(result.at("projected_float_price").at("value").as_number() ==
                100.0,
            "nonzero raw mode suppresses the generic fraction");
}

void test_star_chinext_and_auxiliary_ftol() {
    auto result = project(fixture(0, "300001", 100, 25, 250.0F), 0xffffffffU);
    require(result.at("identity").at("sz_30").as_bool() &&
                result.at("identity")
                    .at("fractional_security_branch")
                    .as_bool() &&
                result.at("projected_float_price").at("value").as_number() ==
                    102.75 &&
                result.at("projected_integer_price_raw_u32").as_number() ==
                    102.0,
            "ChiNext uses fraction plus auxiliary/100 and ftol integer part");
    require(result.at("target_market_or_mode_raw_plus_72").as_number() ==
                4294967295.0,
            "4650 retains the complete raw u32 mode");

    result = project(fixture(1, "688001", 400, 100, 99.0F), 0);
    require(result.at("identity").at("sh_688_or_689").as_bool() &&
                !result.at("source_fields")
                     .at("fraction_in_native_range_1_to_99")
                     .as_bool() &&
                result.at("projected_float_price").at("value").as_number() ==
                    static_cast<double>(static_cast<float>(400.99)) &&
                result.at("projected_integer_price_raw_u32").as_number() ==
                    400.0,
            "STAR ignores fraction 100 and truncates auxiliary/100");
}

void test_signed_identity_wrap_and_safe_subset() {
    auto result = project(
        fixture(2, "688001", 0xffffffffU, 99, 0.0F), 0);
    require(!result.at("identity")
                 .at("fractional_security_branch")
                 .as_bool() &&
                result.at("projected_float_price").at("value").as_number() ==
                    static_cast<double>(static_cast<float>(4294967295.0)) &&
                result.at("projected_integer_price_raw_u32").as_number() ==
                    4294967295.0,
            "4650 uses unsigned source u32 values and exact market branch");

    auto no_data = fixture(0, "000001", 1, 0, 0.0F);
    no_data[0] = 0;
    no_data[5] = 6;
    require_throws([&] { (void)project(no_data, 0); }, "raw_u8_at_0 == 1");

    auto short_snapshot = fixture(0, "000001", 1, 0, 0.0F);
    short_snapshot[5] = static_cast<std::uint8_t>(-6);
    short_snapshot.resize(header_size);
    require_throws([&] { (void)project(short_snapshot, 0); },
                   "at least 120 bytes");

    auto wrong_size = fixture(0, "000001", 1, 0, 0.0F);
    wrong_size.push_back(0);
    require_throws([&] { (void)project(wrong_size, 0); }, "must equal");
}

std::string hex_text(const tdx::Bytes& bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const auto value : bytes) {
        result.push_back(digits[value >> 4U]);
        result.push_back(digits[value & 0x0fU]);
    }
    return result;
}

void test_cli_raw_hex_u32_and_strict_arguments() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-tpbus-4650-price-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    const auto raw = fixture(0, "300001", 100, 25, 250.0F);
    const auto raw_path = directory / "capture.bin";
    const auto hex_path = directory / "capture.hex";
    const auto output_path = directory / "result.json";
    tdx::atomic_write_bytes(raw_path, raw);
    tdx::atomic_write_text(hex_path, hex_text(raw));

    require(tdx::command_level2_preflight({
                "--format", "tpbus-4650-price-primitives", "--input",
                tdx::path_utf8(raw_path), "--encoding", "raw",
                "--target-market-or-mode-raw", "4294967295", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "4650 price CLI accepts raw input and u32 max");
    auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("projected_float_price").at("value").as_number() ==
                102.75,
            "4650 raw CLI routes through the typed projection");

    require(tdx::command_level2_preflight({
                "--format", "tpbus-4650-price-primitives", "--input",
                tdx::path_utf8(hex_path), "--encoding", "hex",
                "--target-market-or-mode-raw", "0", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "4650 price CLI accepts bounded hex input");
    result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("identity").at("sz_30").as_bool(),
            "4650 hex CLI preserves raw identity classification");

    for (const auto& invalid : {std::string("-1"),
                                std::string("4294967296"),
                                std::string("1.0")}) {
        require_throws([&] {
            (void)tdx::command_level2_preflight({
                "--format", "tpbus-4650-price-primitives", "--input",
                tdx::path_utf8(raw_path), "--encoding", "raw",
                "--target-market-or-mode-raw", invalid});
        }, "must be a u32 integer");
    }
    require_throws([&] {
        (void)tdx::command_level2_preflight({
            "--format", "tpbus-4650-price-primitives", "--input",
            tdx::path_utf8(raw_path), "--encoding", "raw",
            "--target-market-or-mode-raw", "0", "--url",
            "https://invalid.example"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_generic_mode_and_fraction();
        test_star_chinext_and_auxiliary_ftol();
        test_signed_identity_wrap_and_safe_subset();
        test_cli_raw_hex_u32_and_strict_arguments();
        std::cout << "level2 tpbus 4650 price primitive tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 tpbus 4650 price primitive tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
