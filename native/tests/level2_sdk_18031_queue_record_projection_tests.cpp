#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace {

constexpr std::size_t body_size = 20012;

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

void put_u32(tdx::Bytes& body, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index)
        body[offset + index] =
            static_cast<std::uint8_t>(value >> (8U * index));
}

tdx::Bytes make_body(std::uint32_t count = 0) {
    tdx::Bytes result(body_size, 0);
    put_u32(result, 8, count);
    return result;
}

tdx::Json project(std::uint16_t market_id, std::string code,
                  tdx::Bytes body) {
    tdx::Level2Sdk18031QueueRecordProjectionRequest request;
    request.market_id = market_id;
    request.code = std::move(code);
    request.body = std::move(body);
    return tdx::project_level2_sdk_18031_queue_record_projection(request);
}

const tdx::Json& state(const tdx::Json& result) {
    return result.at("projected_state");
}

const tdx::Json& record(const tdx::Json& result, std::size_t index) {
    return state(result).at("first_raw").at("records")
        .as_array().at(index);
}

void test_divide_by_ten_u32_boundaries_and_packing() {
    auto input = make_body(5);
    constexpr std::uint32_t quantities[] = {
        0U, 9U, 10U, 11U, std::numeric_limits<std::uint32_t>::max()};
    for (std::size_t index = 0; index < 5; ++index)
        put_u32(input, 12 + 4 * index, quantities[index]);

    const auto result = project(0, "100000", std::move(input));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-18031-queue-record-projection-v1" &&
                result.at("format").as_string() ==
                    "sdk-18031-queue-record-projection" &&
                result.at("field_projection_performed").as_bool(),
            "18031 queue projection schema is stable");
    require_number(result.at("maximum_input_body_size"), 384U * 1024U,
                   "18031 domain publishes its 384 KiB safety ceiling");
    require_number(result.at("security_class_raw"), 2,
                   "market zero 100000 recovers class two");
    require(result.at("sub_594680_predicate_raw").as_bool(),
            "class two activates the recovered predicate");
    require_number(result.at("quantity_divisor_raw"), 10,
                   "active predicate selects exact divisor ten");
    require_number(state(result).at("projected_record_size"), 6,
                   "queue records are exactly six bytes");
    require_number(state(result).at("first_raw").at("projected_byte_size"),
                   30, "projected state byte size is count times six");
    require_number(state(result).at("second_output_raw"), 0,
                   "native secondary output remains raw zero");

    constexpr std::uint32_t projected[] = {
        0U, 0U, 1U, 1U, 429496729U};
    for (std::size_t index = 0; index < 5; ++index) {
        require_number(record(result, index).at("source_quantity_raw"),
                       quantities[index],
                       "source quantity retains all unsigned bits");
        require_number(record(result, index)
                           .at("projected_quantity_raw"),
                       projected[index],
                       "quantity division is unsigned and truncating");
    }
    require(record(result, 3).at("packed_hex").as_string() ==
                "000001000000",
            "six-byte record starts with zero u16 and little-endian quotient");
    require(record(result, 4).at("packed_hex").as_string() ==
                "000099999919",
            "maximum u32 uses native unsigned divide-by-ten result");
}

void test_no_division_and_classifier_market_coverage() {
    auto input = make_body(1);
    put_u32(input, 12, std::numeric_limits<std::uint32_t>::max());
    const auto market_zero = project(0, "000001", input);
    require_number(market_zero.at("security_class_raw"), 0,
                   "market zero false-predicate class is exact");
    require(!market_zero.at("sub_594680_predicate_raw").as_bool() &&
                market_zero.at("quantity_divisor_raw").as_number() == 1.0,
            "false predicate retains quantities with divisor one");
    require(record(market_zero, 0).at("packed_hex").as_string() ==
                "0000ffffffff",
            "false predicate preserves maximum u32 in packed record");

    const auto market_one_true = project(1, "110000", input);
    require_number(market_one_true.at("security_class_raw"), 15,
                   "market one true classifier branch is reusable");
    require(market_one_true.at("sub_594680_predicate_raw").as_bool(),
            "market one class fifteen activates predicate");
    const auto market_one_false = project(1, "600000", input);
    require_number(market_one_false.at("security_class_raw"), 11,
                   "market one false classifier branch is reusable");
    require(!market_one_false.at("sub_594680_predicate_raw").as_bool(),
            "market one class eleven leaves quantity unchanged");

    const auto market_two_true = project(2, "810000", input);
    require_number(market_two_true.at("security_class_raw"), 22,
                   "market two true classifier branch is reusable");
    require(market_two_true.at("sub_594680_predicate_raw").as_bool(),
            "market two class twenty-two activates predicate");
    const auto market_two_false = project(2, "430000", input);
    require_number(market_two_false.at("security_class_raw"), 21,
                   "market two false classifier branch is reusable");
    require(!market_two_false.at("sub_594680_predicate_raw").as_bool(),
            "market two class twenty-one leaves quantity unchanged");
}

void test_body_count_and_identity_boundaries() {
    auto maximum = make_body(5000);
    const auto accepted = project(0, "000001", std::move(maximum));
    require_number(state(accepted).at("first_raw")
                       .at("projected_record_count"),
                   5000, "count 5000 is accepted exactly");
    require_number(state(accepted).at("first_raw").at("count_raw"), 5000,
                   "count 5000 remains the raw count");
    require_number(state(accepted).at("first_raw").at("count_effective"),
                   5000, "count 5000 remains the effective count");
    require(!state(accepted).at("first_raw").at("clamped").as_bool(),
            "count exactly 5000 is not clamped");

    auto excessive = make_body(5001);
    const auto clamped = project(0, "000001", std::move(excessive));
    require_number(state(clamped).at("first_raw").at("count_raw"), 5001,
                   "raw count above 5000 is retained");
    require_number(state(clamped).at("first_raw").at("count_effective"),
                   5000, "effective count clamps to 5000");
    require(state(clamped).at("first_raw").at("clamped").as_bool() &&
                state(clamped).at("first_raw").at("records").size() == 5000,
            "over-limit count continues with exactly 5000 records");
    require_throws([] {
        (void)project(0, "000001", tdx::Bytes(body_size - 1, 0));
    }, "exactly 20012");
    require_throws([] {
        (void)project(0, "000001", tdx::Bytes(body_size + 1, 0));
    }, "exactly 20012");
    require_throws([] {
        (void)project(0, "000001", tdx::Bytes(384U * 1024U + 1U, 0));
    }, "384 KiB");
    require_throws([] {
        (void)project(3, "000001", make_body());
    }, "market must be 0, 1, or 2");
    require_throws([] {
        (void)project(0, "00001", make_body());
    }, "exactly 6 digits");
    require_throws([] {
        (void)project(0, "0000A1", make_body());
    }, "exactly 6 digits");
}

void test_header_independence_and_offline_boundary() {
    auto left = make_body(1);
    put_u32(left, 12, 11);
    auto right = left;
    put_u32(right, 0, 0x12345678U);
    put_u32(right, 4, 0x90abcdefU);
    const auto first = project(0, "100000", std::move(left));
    const auto second = project(0, "100000", std::move(right));
    require(record(first, 0).at("packed_hex").as_string() ==
                record(second, 0).at("packed_hex").as_string(),
            "raw source headers do not alter queue records");
    require_number(second.at("source_header_u32_0_raw"), 0x12345678U,
                   "first source header is retained only as raw metadata");
    require_number(second.at("source_header_u32_1_raw"), 0x90abcdefU,
                   "second source header is retained only as raw metadata");

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
            "18031 typed projection preserves every zero-side-effect boundary");
    require(second.as_object().find("body") == second.as_object().end(),
            "18031 typed projection never echoes the callback body");
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

void test_cli_hex_identity_and_strict_arguments() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-sdk-18031-record-cli-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    auto input = make_body(1);
    put_u32(input, 12, 11);
    const auto input_path = directory / "input.hex";
    const auto output_path = directory / "output.json";
    tdx::atomic_write_text(input_path, body_hex(input));

    require(tdx::command_level2_project({
                "--format", "sdk-18031-queue-record-projection", "--input",
                tdx::path_utf8(input_path), "--encoding", "hex", "--market",
                "0", "--code", "100000", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "hex CLI 18031 queue projection succeeds");
    const auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-18031-queue-record-projection-v1" &&
                result.at("format").as_string() ==
                    "sdk-18031-queue-record-projection" &&
                result.at("security_class_raw").as_number() == 2.0 &&
                record(result, 0).at("packed_hex").as_string() ==
                    "000001000000" &&
                result.at("offline").as_bool() &&
                !result.at("sdk_called").as_bool() &&
                result.at("network_requests").as_number() == 0.0,
            "hex CLI reuses the exact identity-aware 18031 domain projection");

    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-18031-queue-record-projection", "--input",
            tdx::path_utf8(input_path), "--encoding", "hex", "--code", "100000"});
    }, "requires --market");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-18031-queue-record-projection", "--input",
            tdx::path_utf8(input_path), "--encoding", "hex", "--market", "3",
            "--code", "100000"});
    }, "--market must be 0, 1, or 2");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-18031-queue-record-projection", "--input",
            tdx::path_utf8(input_path), "--encoding", "hex", "--market", "0"});
    }, "requires --code");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-18031-queue-record-projection", "--input",
            tdx::path_utf8(input_path), "--encoding", "hex", "--market", "0",
            "--code", "100000", "--url", "https://example.invalid"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_divide_by_ten_u32_boundaries_and_packing();
        test_no_division_and_classifier_market_coverage();
        test_body_count_and_identity_boundaries();
        test_header_independence_and_offline_boundary();
        test_cli_hex_identity_and_strict_arguments();
        std::cout << "level2 SDK 18031 queue record projection tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr <<
            "level2 SDK 18031 queue record projection tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
