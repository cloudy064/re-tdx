#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace {

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
                "error includes expected contract detail");
        return;
    }
    throw tdx::Error("expected contract validation failure");
}

void append_u16(tdx::Bytes& value, std::uint16_t number) {
    value.push_back(static_cast<std::uint8_t>(number));
    value.push_back(static_cast<std::uint8_t>(number >> 8U));
}

void append_u32(tdx::Bytes& value, std::uint32_t number) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        value.push_back(static_cast<std::uint8_t>(number >> shift));
}

std::string hex_text(const tdx::Bytes& value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : value)
        output << std::setw(2) << static_cast<unsigned>(byte);
    return output.str();
}

tdx::Bytes quote_111() {
    tdx::Bytes body(119, 0);
    body[0] = 1;
    body[2] = '0'; body[3] = '0'; body[4] = '0';
    body[5] = '0'; body[6] = '0'; body[7] = '1';
    body[24] = 1;
    body[39] = 7;
    body[99] = 0; body[100] = 0; body[101] = 0x48; body[102] = 0x41;
    body[103] = 9;
    return body;
}

tdx::Bytes queue_112() {
    tdx::Bytes body(58, 0);
    body[0] = 2;
    body[2] = '6'; body[3] = '0'; body[4] = '0';
    body[5] = '0'; body[6] = '0'; body[7] = '0';
    body[36] = 1;
    body[54] = 0x78; body[55] = 0x56;
    body[56] = 0x34; body[57] = 0x12;
    return body;
}

void append_tuple(tdx::Bytes& segment, std::uint32_t discriminator,
                  const tdx::Bytes& body) {
    append_u32(segment, discriminator);
    append_u16(segment, static_cast<std::uint16_t>(body.size()));
    segment.insert(segment.end(), body.begin(), body.end());
}

tdx::Bytes outer(tdx::Bytes segment, tdx::Bytes trailing = {}) {
    tdx::Bytes result;
    append_u16(result, static_cast<std::uint16_t>(segment.size()));
    result.insert(result.end(), segment.begin(), segment.end());
    result.insert(result.end(), trailing.begin(), trailing.end());
    return result;
}

tdx::Json decode(tdx::Bytes payload, int limit = 20) {
    tdx::Level2Tpbus115BatchDecodeRequest request;
    request.payload = std::move(payload);
    request.summary_limit = limit;
    return tdx::decode_level2_tpbus_115_batch(request);
}

void require_offline(const tdx::Json& value) {
    require(!value.at("input_body_retained").as_bool() &&
                !value.at("body_bytes_emitted").as_bool() &&
                !value.at("event_bus_accessed").as_bool() &&
                !value.at("host_state_read").as_bool() &&
                !value.at("host_state_write_performed").as_bool() &&
                !value.at("host_message_dispatch_attempted").as_bool() &&
                value.at("host_messages_sent").as_number() == 0.0 &&
                !value.at("sdk_called").as_bool() &&
                !value.at("callback_executed").as_bool() &&
                !value.at("wire_bytes_built").as_bool() &&
                value.at("network_requests").as_number() == 0.0 &&
                !value.at("request_sent").as_bool() &&
                !value.at("subscription_sent").as_bool() &&
                !value.at("authorization_accessed").as_bool() &&
                !value.at("entitlement_bypass").as_bool() &&
                value.at("offline").as_bool(),
            "tpbus-115 decode remains fully offline");
}

void test_multiple_records_mapping_summaries_and_first_outer_only() {
    tdx::Bytes segment;
    append_tuple(segment, 0, quote_111());
    append_tuple(segment, 1, quote_111());
    append_tuple(segment, 2, queue_112());
    const auto result = decode(outer(segment, {0xaa, 0xbb, 0xcc}));
    require(result.at("schema").as_string() ==
                "tdx-level2-tpbus-115-batch-v1" &&
                result.at("outer").at("status").as_string() ==
                    "first-segment-ready" &&
                result.at("outer").at("trailing_byte_size").as_number() ==
                    3.0 &&
                result.at("outer").at("trailing_bytes_ignored").as_bool() &&
                result.at("inner").at("status").as_string() == "complete" &&
                result.at("inner").at("dispatched_body_count").as_number() ==
                    3.0,
            "115 consumes only its first outer segment and all complete tuples");
    const auto& entries = result.at("entries").as_array();
    require(entries.size() == 3 &&
                entries[0].at("raw_discriminator").as_number() == 0.0 &&
                entries[0].at("mapped_push_type").as_number() == 111.0 &&
                entries[1].at("raw_discriminator").as_number() == 1.0 &&
                entries[1].at("mapped_push_type").as_number() == 111.0 &&
                entries[2].at("raw_discriminator").as_number() == 2.0 &&
                entries[2].at("mapped_push_type").as_number() == 112.0,
            "115 preserves raw discriminators and exact native mapping");
    require(entries[0].at("body_sha256").as_string().size() == 64 &&
                entries[0].at("body_byte_size").as_number() == 119.0 &&
                entries[0].at("summary_available").as_bool() &&
                entries[0].at("summary").at("push_type").as_number() == 111.0 &&
                entries[2].at("summary_available").as_bool() &&
                entries[2].at("summary").at("push_type").as_number() == 112.0,
            "115 fingerprints bodies and safely reuses existing summaries");
    require(entries[0].as_object().find("body") == entries[0].as_object().end(),
            "115 entries never echo bodies");
    require_offline(result);
}

void test_zero_body_skip_and_raw_other_mapping() {
    tdx::Bytes segment;
    append_tuple(segment, 1, {});
    append_tuple(segment, 0xffffffffU, queue_112());
    const auto result = decode(outer(segment));
    require(result.at("inner").at("complete_tuple_count").as_number() == 2.0 &&
                result.at("inner").at("zero_body_skipped_count").as_number() ==
                    1.0 &&
                result.at("entries").size() == 1 &&
                result.at("entries").as_array().at(0).at("tuple_index").as_number() == 1.0 &&
                result.at("entries").as_array().at(0).at("raw_discriminator").as_number() ==
                    4294967295.0 &&
                result.at("entries").as_array().at(0).at("mapped_push_type").as_number() ==
                    112.0,
            "zero bodies are skipped and all non-0/1 discriminators map to 112");
}

void test_outer_and_inner_stop_reasons() {
    require(decode({}).at("outer").at("status").as_string() ==
                "length-prefix-truncated",
            "missing outer length has an exact stop reason");
    require(decode({5, 0, 1, 2}).at("outer").at("status").as_string() ==
                "first-segment-truncated",
            "truncated first outer segment is reported");
    require(decode({0, 0}).at("outer").at("status").as_string() ==
                "zero-length-first-segment",
            "zero-length outer segment never enters inner iteration");

    require(decode(outer({1, 2, 3})).at("inner").at("status").as_string() ==
                "discriminator-truncated",
            "partial discriminator stops inner iteration");
    require(decode(outer({1, 0, 0, 0, 9})).at("inner").at("status").as_string() ==
                "body-length-prefix-truncated",
            "partial body length stops inner iteration");
    auto truncated_body = tdx::Bytes{};
    append_u32(truncated_body, 1);
    append_u16(truncated_body, 4);
    truncated_body.push_back(0xaa);
    const auto body_result = decode(outer(truncated_body));
    require(body_result.at("inner").at("status").as_string() ==
                "body-truncated" &&
                body_result.at("inner").at("pending_declared_body_byte_size")
                        .as_number() == 4.0 &&
                body_result.at("entries").size() == 0,
            "truncated body is never emitted as a partial record");
}

void test_summary_failure_is_bounded_metadata_not_batch_failure() {
    tdx::Bytes segment;
    append_tuple(segment, 0, {0x01});
    const auto result = decode(outer(segment));
    const auto& entry = result.at("entries").as_array().at(0);
    require(!entry.at("summary_available").as_bool() &&
                entry.at("summary").is_null() &&
                entry.at("body_byte_size").as_number() == 1.0 &&
                entry.at("body_sha256").as_string().size() == 64 &&
                result.at("inner").at("status").as_string() == "complete",
            "invalid nested 111/112 shape retains only safe envelope metadata");
}

void test_cli_raw_hex_and_strict_options() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-tpbus-115-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    tdx::Bytes segment;
    append_tuple(segment, 7, queue_112());
    const auto payload = outer(segment);
    const auto raw_path = directory / "batch.bin";
    const auto hex_path = directory / "batch.hex";
    const auto output_path = directory / "result.json";
    tdx::atomic_write_bytes(raw_path, payload);
    tdx::atomic_write_text(hex_path, hex_text(payload));

    require(tdx::command_level2_decode({
                "--format", "tpbus-115", "--input", tdx::path_utf8(raw_path),
                "--encoding", "raw", "--output", tdx::path_utf8(output_path),
                "--compact"}) == 0,
            "tpbus-115 raw CLI succeeds");
    auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("entries").size() == 1 &&
                result.at("entries").as_array().at(0).at("mapped_push_type").as_number() ==
                    112.0,
            "tpbus-115 CLI uses typed domain");
    require(tdx::command_level2_decode({
                "--format", "tpbus-115", "--input", tdx::path_utf8(hex_path),
                "--encoding", "hex", "--output", tdx::path_utf8(output_path),
                "--compact"}) == 0,
            "tpbus-115 bounded hex CLI succeeds");
    require_throws([&] {
        (void)tdx::command_level2_decode({
            "--format", "tpbus-115", "--input", tdx::path_utf8(raw_path),
            "--encoding", "raw", "--xor-key", "1"});
    }, "accepts only");
    require_throws([&] {
        (void)tdx::command_level2_decode({
            "--format", "tpbus-115", "--input", tdx::path_utf8(raw_path),
            "--encoding", "raw", "--url", "https://invalid.example"});
    }, "unknown argument");
}

void test_safety_boundaries_and_evidence_addresses() {
    require_throws([] {
        (void)decode(tdx::Bytes(384U * 1024U + 1U, 0));
    }, "384 KiB");
    require_throws([] { (void)decode({}, 10001); }, "0..10000");
    const auto result = decode({});
    require(result.at("evidence").at("batch_dispatcher").as_string().find(
                "0x1007BFBE") != std::string::npos &&
                result.at("evidence").at("sequence_initializer").as_string().find(
                    "0x101A5AD0") != std::string::npos &&
                result.at("evidence").at("u16_length_reader").as_string().find(
                    "0x101A6060") != std::string::npos &&
                result.at("evidence").at("u32_reader").as_string().find(
                    "0x10066392") != std::string::npos,
            "typed output cites the four targeted evidence functions");
}

}  // namespace

int main() {
    try {
        test_multiple_records_mapping_summaries_and_first_outer_only();
        test_zero_body_skip_and_raw_other_mapping();
        test_outer_and_inner_stop_reasons();
        test_summary_failure_is_bounded_metadata_not_batch_failure();
        test_cli_raw_hex_and_strict_options();
        test_safety_boundaries_and_evidence_addresses();
        std::cout << "level2 tpbus-115 batch tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 tpbus-115 batch tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
