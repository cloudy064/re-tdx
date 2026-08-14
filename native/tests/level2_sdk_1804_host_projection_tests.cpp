#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <cmath>
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

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_number(const tdx::Json& value, std::uint64_t expected,
                    const char* message) {
    require(value.is_number() &&
                value.as_number() == static_cast<double>(expected),
            message);
}

void require_near(const tdx::Json& value, float expected,
                  const char* message) {
    require(value.is_number() &&
                std::fabs(value.as_number() - static_cast<double>(expected)) <
                    1.0e-7,
            message);
}

void require_throws(const std::function<void()>& action,
                    const char* expected_fragment) {
    try {
        action();
    } catch (const tdx::Error& error) {
        require(std::string(error.what()).find(expected_fragment) !=
                    std::string::npos,
                "error includes expected contract detail");
        return;
    }
    throw tdx::Error("expected contract validation failure");
}

void put_u32(tdx::Bytes& body, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index)
        body[offset + index] =
            static_cast<std::uint8_t>(value >> (8U * index));
}

void put_u64(tdx::Bytes& body, std::size_t offset, std::uint64_t value) {
    put_u32(body, offset, static_cast<std::uint32_t>(value));
    put_u32(body, offset + 4, static_cast<std::uint32_t>(value >> 32U));
}

std::uint32_t f32_bits(float value) {
    std::uint32_t result{};
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

void put_f32_bits(tdx::Bytes& body, std::size_t offset,
                  std::uint32_t value) {
    put_u32(body, offset, value);
}

void put_f32(tdx::Bytes& body, std::size_t offset, float value) {
    put_f32_bits(body, offset, f32_bits(value));
}

void put_f64(tdx::Bytes& body, std::size_t offset, double value) {
    std::uint64_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    put_u64(body, offset, bits);
}

tdx::Bytes body(std::uint32_t first_count = 0,
                std::uint32_t second_count = 0,
                double first_price = 10.125,
                double second_price = 10.25) {
    tdx::Bytes result(432, 0);
    put_f64(result, 8, first_price);
    put_f64(result, 216, second_price);
    put_u32(result, 424, first_count);
    put_u32(result, 428, second_count);
    return result;
}

tdx::Json project(tdx::Bytes value) {
    tdx::Level2Sdk1804HostProjectionRequest request;
    request.body = std::move(value);
    return tdx::project_level2_sdk_1804_host_projection(request);
}

const tdx::Json& state(const tdx::Json& result) {
    return result.at("projected_state");
}

std::uint64_t slot(const tdx::Json& result, std::size_t index) {
    const auto& value = state(result).at("slots_raw_u32").as_array().at(index);
    require(value.is_number(), "host slot is emitted as raw u32");
    return static_cast<std::uint64_t>(value.as_number());
}

void test_zero_initialization_and_float_narrowing() {
    auto input = body(0, 0, 16777217.0, 1.234567890123);
    put_f32(input, 16, 99.0F);
    put_f32(input, 224, 88.0F);
    const auto result = project(std::move(input));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-1804-host-projection-v1" &&
                result.at("format").as_string() ==
                    "sdk-1804-host-projection",
            "1804 host projection schema is stable");
    require_number(state(result).at("slot_count"), 105,
                   "host state contains 105 f32 slots");
    require_number(state(result).at("byte_size"), 420,
                   "host state occupies 420 bytes");
    require(state(result).at("slots_raw_u32").size() == 105,
            "all host slots are emitted");
    require(slot(result, 0) == 0,
            "slot zero remains zero initialized");
    require(slot(result, 1) == f32_bits(static_cast<float>(16777217.0)),
            "first f64 price narrows to host f32 in slot one");
    require(slot(result, 2) ==
                f32_bits(static_cast<float>(1.234567890123)),
            "second f64 price narrows to host f32 in slot two");
    require_near(state(result).at("first").at("price"),
                 static_cast<float>(16777217.0),
                 "semantic first price reports the narrowed f32");
    require_number(state(result).at("first").at("copied_count"), 0,
                   "zero first count copies no source quantities");
    require_number(state(result).at("second").at("copied_count"), 0,
                   "zero second count copies no source quantities");
    for (std::size_t index = 3; index < 105; ++index)
        require(slot(result, index) == 0,
                "uncopied host slots remain zero initialized");
}

void test_one_and_fifty_quantity_copies() {
    auto one = body(1, 1);
    put_f32(one, 16, 1.5F);
    put_f32(one, 20, 99.0F);
    put_f32(one, 224, 2.5F);
    put_f32(one, 228, 88.0F);
    auto result = project(std::move(one));
    require(slot(result, 3) == 1 && slot(result, 4) == 1,
            "count one is bit-preserved in slots three and four");
    require(slot(result, 5) == f32_bits(1.5F) && slot(result, 6) == 0,
            "first side copies exactly one quantity");
    require(slot(result, 55) == f32_bits(2.5F) && slot(result, 56) == 0,
            "second side copies exactly one quantity");
    require(state(result).at("first").at("quantity_f32").size() == 1 &&
                state(result).at("second").at("quantity_f32").size() == 1,
            "semantic arrays contain only copied quantities");

    auto fifty = body(50, 50);
    for (std::size_t index = 0; index < 50; ++index) {
        put_f32(fifty, 16 + 4 * index,
                static_cast<float>(index + 1));
        put_f32(fifty, 224 + 4 * index,
                static_cast<float>(index + 101));
    }
    result = project(std::move(fifty));
    require(slot(result, 54) == f32_bits(50.0F) &&
                slot(result, 104) == f32_bits(150.0F),
            "count fifty fills the final slot on both sides");
    require_number(state(result).at("first").at("copied_count"), 50,
                   "first side copies all fifty quantities");
    require_number(state(result).at("second").at("copied_count"), 50,
                   "second side copies all fifty quantities");
    require(!state(result).at("first").at("count_clamped_to_50").as_bool() &&
                !state(result).at("second").at("count_clamped_to_50").as_bool(),
            "count exactly fifty is not reported as clamped");
}

void test_over_fifty_clamp_and_raw_bit_preservation() {
    constexpr std::uint32_t second_count_raw = 0xdeadbeefU;
    constexpr std::uint32_t last_quantity_bits = 0x7fc01234U;
    auto input = body(51, second_count_raw);
    for (std::size_t index = 0; index < 50; ++index) {
        put_f32(input, 16 + 4 * index, static_cast<float>(index + 1));
        put_f32(input, 224 + 4 * index, static_cast<float>(index + 101));
    }
    put_f32_bits(input, 16 + 4 * 49, last_quantity_bits);
    const auto result = project(std::move(input));
    require(slot(result, 3) == 51 && slot(result, 4) == second_count_raw,
            "raw counts remain bit-exact in slots three and four");
    require_number(state(result).at("first").at("count_raw"), 51,
                   "first raw count is preserved outside the slot array");
    require_number(state(result).at("second").at("count_raw"),
                   second_count_raw,
                   "second raw count preserves all u32 bits");
    require_number(state(result).at("first").at("copied_count"), 50,
                   "first count above fifty clamps only the copy loop");
    require_number(state(result).at("second").at("copied_count"), 50,
                   "large second count clamps only the copy loop");
    require(state(result).at("first").at("count_clamped_to_50").as_bool() &&
                state(result).at("second").at("count_clamped_to_50").as_bool(),
            "both oversized counts report host clamping");
    require(slot(result, 54) == last_quantity_bits,
            "copied source f32 quantity retains its exact raw bits");
    require(state(result).at("first").at("quantity_f32").as_array()[49]
                .is_null(),
            "non-finite copied quantity is nullable without losing raw bits");
    require(state(result).at("side_semantics").as_string().find(
                "not inferred") != std::string::npos,
            "projection does not guess buy/sell side semantics");
}

void test_exact_body_contract() {
    require_throws([] {
        (void)project(tdx::Bytes(431, 0));
    }, "exactly 432");
    require_throws([] {
        (void)project(tdx::Bytes(433, 0));
    }, "exactly 432");
}

std::string body_hex(const tdx::Bytes& body) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(body.size() * 2);
    for (const auto byte : body) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

void test_cli_raw_hex_bounds_and_offline_contract() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-sdk-1804-host-projection-cli-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    auto input = body(51, 1);
    put_f32(input, 16, 7.5F);
    put_f32(input, 224, 8.5F);
    const auto raw_path = directory / "input.bin";
    const auto hex_path = directory / "input.hex";
    const auto output_path = directory / "output.json";
    tdx::atomic_write_bytes(raw_path, input);
    tdx::atomic_write_text(hex_path, body_hex(input));

    require(tdx::command_level2_project({
                "--format", "sdk-1804-host-projection", "--input",
                tdx::path_utf8(raw_path), "--encoding", "raw", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "raw CLI 1804 projection succeeds");
    auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("format").as_string() ==
                "sdk-1804-host-projection" &&
                !result.at("input_body_retained").as_bool() &&
                !result.at("sdk_called").as_bool() &&
                !result.at("sdk_callback_invoked").as_bool() &&
                !result.at("callback_executed").as_bool() &&
                !result.at("host_storage_call_attempted").as_bool() &&
                !result.at("sub_525600_called").as_bool() &&
                !result.at("host_message_dispatch_attempted").as_bool() &&
                result.at("host_messages_sent").as_number() == 0.0 &&
                !result.at("wire_bytes_built").as_bool() &&
                !result.at("network_request_bytes_built").as_bool() &&
                result.at("network_requests").as_number() == 0.0 &&
                !result.at("request_sent").as_bool() &&
                !result.at("entitlement_bypass").as_bool() &&
                result.at("offline").as_bool(),
            "CLI projection preserves all no-side-effect boundaries");
    require_number(state(result).at("first").at("count_raw"), 51,
                   "raw CLI preserves oversized count");
    require_number(state(result).at("first").at("copied_count"), 50,
                   "raw CLI applies host clamp");

    require(tdx::command_level2_project({
                "--format", "sdk-1804-host-projection", "--input",
                tdx::path_utf8(hex_path), "--encoding", "hex", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "hex CLI 1804 projection succeeds");
    result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(slot(result, 5) == f32_bits(7.5F) &&
                slot(result, 55) == f32_bits(8.5F),
            "hex CLI decodes the exact source body");

    const auto short_path = directory / "short.bin";
    const auto long_path = directory / "long.bin";
    const auto oversized_hex_path = directory / "oversized.hex";
    tdx::atomic_write_bytes(short_path, tdx::Bytes(431, 0));
    tdx::atomic_write_bytes(long_path, tdx::Bytes(433, 0));
    tdx::atomic_write_text(oversized_hex_path,
                           std::string(16U * 1024U + 1U, '0'));
    for (const auto& path : {short_path, long_path}) {
        require_throws([&] {
            (void)tdx::command_level2_project({
                "--format", "sdk-1804-host-projection", "--input",
                tdx::path_utf8(path), "--encoding", "raw"});
        }, "exactly 432");
    }
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1804-host-projection", "--input",
            tdx::path_utf8(oversized_hex_path), "--encoding", "hex"});
    }, "16 KiB");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1804-host-projection", "--input",
            tdx::path_utf8(raw_path)});
    }, "requires --encoding");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1804-host-projection", "--input",
            tdx::path_utf8(raw_path), "--encoding", "raw", "--url",
            "https://example.invalid"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_zero_initialization_and_float_narrowing();
        test_one_and_fifty_quantity_copies();
        test_over_fifty_clamp_and_raw_bit_preservation();
        test_exact_body_contract();
        test_cli_raw_hex_bounds_and_offline_contract();
        std::cout << "level2 SDK 1804 host projection tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 SDK 1804 host projection tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
