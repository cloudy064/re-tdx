#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

namespace {

constexpr std::size_t header_size = 96;

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
                "failure includes the expected contract detail");
        return;
    }
    throw tdx::Error("expected contract validation failure");
}

std::uint8_t raw_i8(int value) {
    return static_cast<std::uint8_t>(
        static_cast<std::int8_t>(value));
}

tdx::Bytes raw_fixture(const std::array<int, 6>& terms,
                       std::int16_t market = 0,
                       const std::string& code = "000001",
                       std::uint8_t trigger_86 = 0,
                       std::uint8_t data_fill = 0x6a) {
    const auto units = static_cast<std::int64_t>(terms[5]) +
        6LL * (static_cast<std::int64_t>(terms[0]) +
               2LL * (static_cast<std::int64_t>(terms[2]) +
                      static_cast<std::int64_t>(terms[3]) +
                      static_cast<std::int64_t>(terms[4]))) +
        4LL * static_cast<std::int64_t>(terms[1]);
    const auto signed_size =
        static_cast<std::int64_t>(header_size) + 20LL * units;
    if (signed_size < static_cast<std::int64_t>(header_size))
        throw tdx::Error("invalid 4650 fixture size");
    tdx::Bytes raw(static_cast<std::size_t>(signed_size), data_fill);
    for (std::size_t index = 0; index < terms.size(); ++index)
        raw[index] = raw_i8(terms[index]);
    const auto market_bits = static_cast<std::uint16_t>(market);
    raw[8] = static_cast<std::uint8_t>(market_bits & 0xffU);
    raw[9] = static_cast<std::uint8_t>(market_bits >> 8U);
    std::fill(raw.begin() + 10, raw.begin() + 96, std::uint8_t{0});
    if (code.size() >= 86)
        throw tdx::Error("invalid 4650 fixture code");
    for (std::size_t index = 0; index < code.size(); ++index)
        raw[10 + index] = static_cast<std::uint8_t>(code[index]);
    raw[86] = trigger_86;
    return raw;
}

tdx::Json preflight(tdx::Bytes raw) {
    tdx::Level2Tpbus4650DispatchPreflightRequest request;
    request.raw = std::move(raw);
    return tdx::level2_tpbus_4650_dispatch_preflight_document(request);
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
            "4650 preflight remains pure offline diagnostics");
}

void test_minimum_shape_and_unresolved_gates() {
    const auto result = preflight(raw_fixture({0, 0, 0, 0, 0, 0}));
    require(result.at("schema").as_string() ==
                "tdx-level2-tpbus-4650-dispatch-preflight-v1" &&
                result.at("format").as_string() ==
                    "tpbus-4650-dispatch-preflight" &&
                result.at("function_id").as_number() == 4650.0 &&
                result.at("raw_validator").at("qualified").as_bool() &&
                result.at("raw_validator")
                        .at("actual_byte_size")
                        .as_number() == 96.0 &&
                result.at("raw_validator")
                        .at("variable_units_signed")
                        .as_number() == 0.0,
            "4650 validates the exact signed raw shape");
    require(result.at("raw_identity")
                    .at("market_raw_signed_i16")
                    .as_number() == 0.0 &&
                result.at("raw_identity")
                        .at("code_raw")
                        .at("value")
                        .as_string() == "000001" &&
                !result.at("raw_identity")
                     .at("fully_evaluated")
                     .as_bool(),
            "4650 preserves bounded raw identity without host comparison");
    const auto& gates = result.at("caller_gates");
    require(!gates.at("trigger_passed").as_bool() &&
                !gates.at("raw_plus_96_branch_selected").as_bool() &&
                gates.at("handler_call_possible").is_null() &&
                !result.at("dispatcher_fully_qualified").as_bool() &&
                !result.at("state_projection_closed").as_bool() &&
                !result.at("state_projection_performed").as_bool(),
            "4650 does not promote raw validation to a host-state claim");
    require_offline(result);
}

void test_branch_candidate_signed_terms_and_no_echo() {
    auto raw = raw_fixture({1, -1, 1, 0, 0, 0}, -2, "600000", 1,
                           static_cast<std::uint8_t>('Z'));
    const auto raw_size = raw.size();
    const auto result = preflight(raw);
    require(result.at("raw_validator")
                    .at("variable_units_signed")
                    .as_number() == 14.0 &&
                result.at("raw_validator")
                        .at("actual_byte_size")
                        .as_number() == static_cast<double>(raw_size),
            "4650 uses signed i8 terms in the native expression");
    const auto& identity = result.at("raw_identity");
    require(identity.at("market_raw_signed_i16").as_number() == -2.0 &&
                identity.at("code_raw").at("value").as_string() ==
                    "600000",
            "4650 retains raw market and bounded code evidence");
    const auto& gates = result.at("caller_gates");
    require(gates.at("trigger_passed").as_bool() &&
                gates.at("raw_plus_96_branch_selected").as_bool(),
            "4650 evaluates only the proven raw caller gates");
    const auto& candidate = result.at("raw_plus_96_candidate");
    require(candidate.at("selected_by_raw_gate").as_bool() &&
                candidate.at("offset").as_number() == 96.0 &&
                candidate.at("byte_size").as_number() ==
                    static_cast<double>(raw_size - header_size) &&
                candidate.at("sha256").as_string().size() == 64 &&
                !candidate.at("body_emitted").as_bool(),
            "4650 fingerprints but does not emit the raw+96 candidate");
    require(result.dump(-1).find(std::string(40, 'Z')) == std::string::npos,
            "4650 response does not echo the raw data region");
    require_offline(result);
}

void test_shape_and_safety_boundaries() {
    require_throws([] {
        (void)preflight(tdx::Bytes(header_size - 1, 0));
    }, "at least 96");
    require_throws([] {
        auto raw = raw_fixture({1, 0, 0, 0, 0, 0});
        raw.pop_back();
        (void)preflight(std::move(raw));
    }, "must equal");
    require_throws([] {
        auto raw = raw_fixture({1, 0, 0, 0, 0, 0});
        raw.push_back(0);
        (void)preflight(std::move(raw));
    }, "must equal");
    require_throws([] {
        (void)preflight(tdx::Bytes(384U * 1024U + 1U, 0));
    }, "384 KiB");

    auto unterminated = raw_fixture({0, 0, 0, 0, 0, 0});
    std::fill(unterminated.begin() + 10, unterminated.begin() + 96,
              static_cast<std::uint8_t>('A'));
    const auto result = preflight(std::move(unterminated));
    require(!result.at("raw_identity")
                 .at("code_raw")
                 .at("nul_terminated_within_header")
                 .as_bool() &&
                result.at("raw_identity")
                    .at("code_raw")
                    .at("value")
                    .is_null(),
            "4650 never performs an unbounded native strcmp offline");
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

void test_cli_raw_hex_and_strict_arguments() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-tpbus-4650-preflight-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    const auto raw = raw_fixture({1, 0, 0, 0, 0, 0}, 1, "000001", 1);
    const auto raw_path = directory / "capture.bin";
    const auto hex_path = directory / "capture.hex";
    const auto output_path = directory / "result.json";
    tdx::atomic_write_bytes(raw_path, raw);
    tdx::atomic_write_text(hex_path, hex_text(raw));

    require(tdx::command_level2_preflight({
                "--format", "tpbus-4650-dispatch-preflight", "--input",
                tdx::path_utf8(raw_path), "--encoding", "raw", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "4650 CLI accepts bounded raw input");
    auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("raw_validator").at("qualified").as_bool(),
            "4650 raw CLI routes through the typed preflight");
    require_offline(result);

    require(tdx::command_level2_preflight({
                "--format", "tpbus-4650-dispatch-preflight", "--input",
                tdx::path_utf8(hex_path), "--encoding", "hex", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "4650 CLI accepts bounded hex input");
    result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("raw_validator").at("actual_byte_size").as_number() ==
                static_cast<double>(raw.size()),
            "4650 hex CLI preserves the decoded byte size");

    require_throws([&] {
        (void)tdx::command_level2_preflight({
            "--format", "tpbus-4650-dispatch-preflight", "--input",
            tdx::path_utf8(raw_path)});
    }, "requires --encoding");
    require_throws([&] {
        (void)tdx::command_level2_preflight({
            "--format", "tpbus-4650-dispatch-preflight", "--input",
            tdx::path_utf8(raw_path), "--encoding", "raw", "--url",
            "https://invalid.example"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_minimum_shape_and_unresolved_gates();
        test_branch_candidate_signed_terms_and_no_echo();
        test_shape_and_safety_boundaries();
        test_cli_raw_hex_and_strict_arguments();
        std::cout << "level2 tpbus 4650 dispatch preflight tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 tpbus 4650 dispatch preflight tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
