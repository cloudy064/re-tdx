#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

namespace {

constexpr std::size_t body_size = 32016;

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_number(const tdx::Json& value, std::uint64_t expected,
                    const char* message) {
    require(value.is_number() &&
                value.as_number() == static_cast<double>(expected),
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

void put_u16(tdx::Bytes& body, std::size_t offset, std::uint16_t value) {
    body[offset] = static_cast<std::uint8_t>(value);
    body[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
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

void put_f64(tdx::Bytes& body, std::size_t offset, double value) {
    std::uint64_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    put_u64(body, offset, bits);
}

std::uint32_t f32_bits(float value) {
    std::uint32_t result{};
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

tdx::Bytes make_body(std::uint32_t first_count = 0,
                     std::uint32_t second_count = 0) {
    tdx::Bytes result(body_size, 0);
    put_u32(result, 8, first_count);
    put_u32(result, 12, second_count);
    return result;
}

tdx::Json project(tdx::Bytes body) {
    tdx::Level2Sdk1803DepthRecordProjectionRequest request;
    request.body = std::move(body);
    return tdx::project_level2_sdk_1803_depth_record_projection(request);
}

const tdx::Json& state(const tdx::Json& result) {
    return result.at("projected_state");
}

const tdx::Json& record(const tdx::Json& result, const char* side,
                        std::size_t index) {
    return state(result).at(side).at("records").as_array().at(index);
}

void test_exact_layout_offsets_and_f32_landing() {
    auto input = make_body(1, 1);
    put_u32(input, 0, 0x12345678U);
    put_u32(input, 4, 0x90abcdefU);
    put_f64(input, 16, 16777217.0);
    put_u32(input, 8016, 0x11223344U);
    put_u16(input, 12012, 0xbeefU);
    put_u16(input, 12016, 0x6655U);
    put_f64(input, 16016, 1.25);
    put_u32(input, 24016, 7U);
    put_u16(input, 28012, 0xdeadU);
    put_u16(input, 28016, 0x8877U);

    const auto result = project(std::move(input));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-1803-depth-record-projection-v1" &&
                result.at("format").as_string() ==
                    "sdk-1803-depth-record-projection" &&
                result.at("field_projection_performed").as_bool(),
            "1803 depth projection schema is stable");
    require_number(result.at("maximum_input_body_size"), 384U * 1024U,
                   "1803 domain publishes its 384 KiB safety ceiling");
    require_number(result.at("source_header_u32_0_raw"), 0x12345678U,
                   "first source header remains raw");
    require_number(result.at("source_header_u32_1_raw"), 0x90abcdefU,
                   "second source header remains raw");
    require_number(state(result).at("projected_record_size"), 13,
                   "projected records are exactly 13 bytes");
    require_number(state(result).at("projected_record_count"), 2,
                   "both raw sides contribute to projected record count");
    require_number(state(result).at("projected_byte_size"), 26,
                   "projected state byte size is count times thirteen");

    const auto& first = record(result, "first", 0);
    require_number(first.at("price_f32_raw_u32"),
                   f32_bits(static_cast<float>(16777217.0)),
                   "source f64 price lands in host f32");
    require_number(first.at("volume_raw"), 0x11223344U,
                   "first volume uses actual +8016 array start");
    require_number(first.at("auxiliary_raw"), 0x6655U,
                   "first auxiliary uses actual +12016 array start");
    require_number(first.at("top_volume_rank_raw"), 3,
                   "single record preserves native second/third fallback overwrite");
    require(first.at("packed_hex").as_string() ==
                "0000804b443322110055660300",
            "first record has exact 13-byte little-endian layout");

    const auto& second = record(result, "second", 0);
    require_number(second.at("auxiliary_raw"), 0x8877U,
                   "second auxiliary uses actual +28016 array start");
    require(second.at("packed_hex").as_string() ==
                "0000a03f070000000077880300",
            "second side independently emits exact packed bytes");
    require(state(result).at("side_naming").as_string().find(
                "not inferred") != std::string::npos,
            "projection does not infer buy/sell side meaning");
}

void test_independent_stable_top_three_ranking() {
    auto input = make_body(5, 4);
    constexpr std::uint32_t first_volumes[] = {10, 30, 30, 0, 20};
    constexpr std::uint32_t second_volumes[] = {0, 5, 4, 0};
    for (std::size_t index = 0; index < 5; ++index) {
        put_f64(input, 16 + 8 * index, 10.0 + index);
        put_u32(input, 8016 + 4 * index, first_volumes[index]);
        put_u16(input, 12016 + 4 * index,
                static_cast<std::uint16_t>(100 + index));
    }
    for (std::size_t index = 0; index < 4; ++index) {
        put_f64(input, 16016 + 8 * index, 20.0 + index);
        put_u32(input, 24016 + 4 * index, second_volumes[index]);
        put_u16(input, 28016 + 4 * index,
                static_cast<std::uint16_t>(200 + index));
    }

    const auto result = project(std::move(input));
    constexpr std::uint64_t first_ranks[] = {0, 1, 2, 0, 3};
    constexpr std::uint64_t second_ranks[] = {3, 1, 2, 0};
    for (std::size_t index = 0; index < 5; ++index)
        require_number(record(result, "first", index)
                           .at("top_volume_rank_raw"),
                       first_ranks[index],
                       "first ranks are stable, strict, and top-three only");
    for (std::size_t index = 0; index < 4; ++index)
        require_number(record(result, "second", index)
                           .at("top_volume_rank_raw"),
                       second_ranks[index],
                       "second ranks preserve native third-candidate fallback");
    require(record(result, "first", 1).at("packed_hex").as_string()
                .substr(22, 2) == "01" &&
                record(result, "first", 2).at("packed_hex").as_string()
                .substr(22, 2) == "02",
            "rank byte is stored at packed offset eleven");
}

void test_native_missing_rank_candidate_fallback() {
    auto all_zero = make_body(2, 0);
    put_f64(all_zero, 16, 1.0);
    put_f64(all_zero, 24, 2.0);
    const auto zero_result = project(std::move(all_zero));
    require_number(record(zero_result, "first", 0)
                       .at("top_volume_rank_raw"),
                   3,
                   "native -1/0/0 candidates leave final fallback rank on record zero");
    require_number(record(zero_result, "first", 1)
                       .at("top_volume_rank_raw"),
                   0,
                   "strict unsigned greater-than never selects another zero volume");

    auto one_positive = make_body(2, 0);
    put_u32(one_positive, 8016, 5);
    const auto one_result = project(std::move(one_positive));
    require_number(record(one_result, "first", 0)
                       .at("top_volume_rank_raw"),
                   3,
                   "second and third native fallbacks overwrite rank one at record zero");
}

void test_count_and_body_boundaries() {
    auto maximum = make_body(1000, 0);
    const auto accepted = project(std::move(maximum));
    require_number(state(accepted).at("first").at("projected_record_count"),
                   1000, "count 1000 is accepted exactly");
    require_number(state(accepted).at("first").at("count_raw"), 1000,
                   "count 1000 remains the raw count");
    require_number(state(accepted).at("first").at("count_effective"), 1000,
                   "count 1000 remains the effective count");
    require(!state(accepted).at("first").at("clamped").as_bool(),
            "count exactly 1000 is not clamped");

    auto excessive_first = make_body(1001, 0);
    const auto clamped_first = project(std::move(excessive_first));
    require_number(state(clamped_first).at("first").at("count_raw"), 1001,
                   "first raw count above 1000 is retained");
    require_number(state(clamped_first).at("first").at("count_effective"),
                   1000, "first effective count clamps to 1000");
    require(state(clamped_first).at("first").at("clamped").as_bool() &&
                state(clamped_first).at("first").at("records").size() == 1000,
            "first over-limit count continues with exactly 1000 records");
    auto excessive_second = make_body(0, 1001);
    const auto clamped_second = project(std::move(excessive_second));
    require_number(state(clamped_second).at("second").at("count_raw"), 1001,
                   "second raw count above 1000 is retained");
    require_number(state(clamped_second).at("second").at("count_effective"),
                   1000, "second effective count clamps to 1000");
    require(state(clamped_second).at("second").at("clamped").as_bool() &&
                state(clamped_second).at("second").at("records").size() == 1000,
            "second over-limit count continues with exactly 1000 records");
    require_throws([] { (void)project(tdx::Bytes(body_size - 1, 0)); },
                   "exactly 32016");
    require_throws([] { (void)project(tdx::Bytes(body_size + 1, 0)); },
                   "exactly 32016");
    require_throws([] {
        (void)project(tdx::Bytes(384U * 1024U + 1U, 0));
    }, "384 KiB");
}

void test_header_independence_and_offline_boundary() {
    auto left = make_body(1, 0);
    put_f64(left, 16, 2.5);
    put_u32(left, 8016, 9);
    put_u16(left, 12016, 3);
    auto right = left;
    put_u32(right, 0, 0xffffffffU);
    put_u32(right, 4, 0x87654321U);
    const auto first = project(std::move(left));
    const auto second = project(std::move(right));
    require(record(first, "first", 0).at("packed_hex").as_string() ==
                record(second, "first", 0).at("packed_hex").as_string(),
            "raw source headers do not alter projected records");

    require(!second.at("input_body_retained").as_bool() &&
                !second.at("raw_body_emitted").as_bool() &&
                !second.at("previous_state_read").as_bool() &&
                !second.at("sdk_called").as_bool() &&
                !second.at("sdk_callback_invoked").as_bool() &&
                !second.at("callback_executed").as_bool() &&
                !second.at("host_storage_call_attempted").as_bool() &&
                !second.at("host_state_write_performed").as_bool() &&
                !second.at("host_message_dispatch_attempted").as_bool() &&
                second.at("host_messages_sent").as_number() == 0.0 &&
                !second.at("wire_bytes_built").as_bool() &&
                !second.at("network_request_bytes_built").as_bool() &&
                second.at("network_requests").as_number() == 0.0 &&
                !second.at("request_sent").as_bool() &&
                !second.at("subscription_sent").as_bool() &&
                second.at("offline").as_bool() &&
                !second.at("entitlement_bypass").as_bool(),
            "1803 typed projection preserves every zero-side-effect boundary");
    require(second.as_object().find("body") == second.as_object().end(),
            "1803 typed projection never echoes the callback body");
}

void test_cli_raw_and_strict_arguments() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-sdk-1803-record-cli-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    auto input = make_body(1, 0);
    put_f64(input, 16, 12.5);
    put_u32(input, 8016, 7);
    const auto input_path = directory / "input.bin";
    const auto output_path = directory / "output.json";
    const auto short_path = directory / "short.bin";
    tdx::atomic_write_bytes(input_path, input);
    tdx::atomic_write_bytes(short_path, tdx::Bytes(body_size - 1, 0));

    require(tdx::command_level2_project({
                "--format", "sdk-1803-depth-record-projection", "--input",
                tdx::path_utf8(input_path), "--encoding", "raw", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "raw CLI 1803 record projection succeeds");
    const auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-1803-depth-record-projection-v1" &&
                result.at("format").as_string() ==
                    "sdk-1803-depth-record-projection" &&
                record(result, "first", 0).at("packed_hex").as_string() ==
                    "00004841070000000000000300" &&
                result.at("offline").as_bool() &&
                !result.at("sdk_called").as_bool() &&
                result.at("network_requests").as_number() == 0.0,
            "raw CLI reuses the exact offline 1803 domain projection");

    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1803-depth-record-projection", "--input",
            tdx::path_utf8(short_path), "--encoding", "raw"});
    }, "exactly 32016");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1803-depth-record-projection", "--input",
            tdx::path_utf8(input_path)});
    }, "requires --encoding");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1803-depth-record-projection", "--input",
            tdx::path_utf8(input_path), "--encoding", "raw", "--market", "0"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_exact_layout_offsets_and_f32_landing();
        test_independent_stable_top_three_ranking();
        test_native_missing_rank_candidate_fallback();
        test_count_and_body_boundaries();
        test_header_independence_and_offline_boundary();
        test_cli_raw_and_strict_arguments();
        std::cout << "level2 SDK 1803 depth record projection tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr <<
            "level2 SDK 1803 depth record projection tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
