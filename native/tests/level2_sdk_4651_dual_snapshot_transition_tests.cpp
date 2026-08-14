#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace {

constexpr std::size_t maximum_snapshot_size = 384U * 1024U;
constexpr const char* previous_decoded_hash =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
constexpr const char* previous_raw_hash =
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";

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

tdx::Bytes qualified_decoded(std::size_t size = 8) {
    tdx::Bytes result(size, 0x44);
    result[2] = 0xff;
    result[3] = 0xff;
    result[4] = 0xff;
    result[5] = 0xff;
    return result;
}

tdx::Level2Sdk4651DualSnapshotPreviousState previous_state(
    std::uint64_t raw_size, bool ready = true) {
    tdx::Level2Sdk4651DualSnapshotPreviousState result;
    result.raw_byte_size = raw_size;
    result.decoded_byte_size = 17;
    result.raw_sha256 = previous_raw_hash;
    result.decoded_sha256 = previous_decoded_hash;
    result.ready = ready;
    return result;
}

tdx::Json project(
    tdx::Bytes decoded, tdx::Bytes raw,
    std::optional<tdx::Level2Sdk4651DualSnapshotPreviousState> previous =
        std::nullopt) {
    tdx::Level2Sdk4651DualSnapshotTransitionRequest request;
    request.decoded = std::move(decoded);
    request.raw = std::move(raw);
    request.previous = std::move(previous);
    return tdx::project_level2_sdk_4651_dual_snapshot_transition(request);
}

void require_no_side_effects(const tdx::Json& result) {
    require(!result.at("input_body_retained").as_bool() &&
                !result.at("decoded_body_emitted").as_bool() &&
                !result.at("raw_body_emitted").as_bool() &&
                !result.at("raw_to_decoded_conversion_performed").as_bool() &&
                !result.at("callback_executed").as_bool() &&
                !result.at("sdk_callback_invoked").as_bool() &&
                !result.at("sdk_callback_executed").as_bool() &&
                !result.at("sdk_called").as_bool() &&
                !result.at("host_storage_call_attempted").as_bool() &&
                !result.at("host_state_write_performed").as_bool() &&
                !result.at("host_message_dispatch_attempted").as_bool() &&
                result.at("host_messages_sent").as_number() == 0.0 &&
                !result.at("wire_bytes_built").as_bool() &&
                !result.at("network_request_bytes_built").as_bool() &&
                result.at("network_requests").as_number() == 0.0 &&
                !result.at("request_sent").as_bool() &&
                !result.at("subscription_sent").as_bool() &&
                !result.at("authorization_attempted").as_bool() &&
                !result.at("authorization_bypass_attempted").as_bool() &&
                !result.at("credentials_accessed").as_bool() &&
                !result.at("implicit_root_accessed").as_bool() &&
                !result.at("config_accessed").as_bool() &&
                !result.at("entitlement_bypass").as_bool() &&
                result.at("offline").as_bool(),
            "4651 projection preserves the complete offline boundary");
}

void test_absent_and_zero_previous_replace() {
    for (const auto previous : {
             std::optional<tdx::Level2Sdk4651DualSnapshotPreviousState>{},
             std::optional<tdx::Level2Sdk4651DualSnapshotPreviousState>{
                 previous_state(0)}}) {
        const auto result = project(qualified_decoded(), tdx::Bytes(3, 0x22),
                                    previous);
        require(result.at("schema").as_string() ==
                    "tdx-level2-sdk-4651-dual-snapshot-transition-v1" &&
                    result.at("format").as_string() ==
                        "sdk-4651-dual-snapshot-transition" &&
                    result.at("dispatcher_qualified_input").as_bool() &&
                    !result.at("decoded_exact_length_proven").as_bool(),
                "4651 publishes its opaque caller-qualified schema");
        const auto& decision = result.at("decision");
        require(decision.at("previous_raw_logical_size").as_number() == 0.0 &&
                    decision.at("new_raw_size").as_number() == 3.0 &&
                    !decision.at("previous_raw_capacity_read").as_bool() &&
                    decision.at("replaced").as_bool() &&
                    !decision.at("retained").as_bool(),
                "zero previous logical size always replaces");
        const auto& state = result.at("projected_state");
        require(state.at("schema").as_string() ==
                    "tdx-level2-sdk-4651-dual-snapshot-state-v1" &&
                    state.at("decoded_byte_size").as_number() == 8.0 &&
                    state.at("raw_byte_size").as_number() == 3.0 &&
                    state.at("decoded_sha256").as_string().size() == 64 &&
                    state.at("raw_sha256").as_string().size() == 64 &&
                    state.at("ready").as_bool() &&
                    state.at("metadata_complete").as_bool(),
                "replacement returns a complete round-trippable state");
        require_no_side_effects(result);
    }
}

void test_size_replace_retain_and_equality() {
    const auto smaller = project(
        qualified_decoded(), tdx::Bytes(4, 0x21), previous_state(5, false));
    require(!smaller.at("decision").at("replaced").as_bool() &&
                smaller.at("decision").at("retained").as_bool() &&
                smaller.at("decision").at("result").as_string() ==
                    "retain-previous",
            "smaller new raw snapshot retains previous state");
    const auto& retained = smaller.at("projected_state");
    require(retained.at("raw_byte_size").as_number() == 5.0 &&
                retained.at("decoded_byte_size").as_number() == 17.0 &&
                retained.at("raw_sha256").as_string() ==
                    std::string(64, 'b') &&
                retained.at("decoded_sha256").as_string() ==
                    std::string(64, 'a') &&
                !retained.at("ready").as_bool() &&
                retained.at("metadata_complete").as_bool(),
            "retention preserves complete explicit metadata and canonical hashes");

    const auto equal = project(
        qualified_decoded(), tdx::Bytes(5, 0x22), previous_state(5));
    require(equal.at("decision").at("replaced").as_bool() &&
                equal.at("projected_state").at("raw_byte_size").as_number() ==
                    5.0 &&
                equal.at("projected_state").at("source").as_string() ==
                    "new-explicit-snapshots",
            "equal new raw size replaces previous state");

    const auto larger = project(
        qualified_decoded(), tdx::Bytes(6, 0x23), previous_state(5));
    require(larger.at("decision").at("replaced").as_bool(),
            "larger new raw size replaces previous state");
}

void test_round_trip_and_signed_previous_boundary() {
    const auto first = project(
        qualified_decoded(9), tdx::Bytes(7, 0x31));
    const auto& state = first.at("projected_state");
    tdx::Level2Sdk4651DualSnapshotPreviousState replay;
    replay.decoded_byte_size = static_cast<std::uint64_t>(
        state.at("decoded_byte_size").as_number());
    replay.decoded_sha256 = state.at("decoded_sha256").as_string();
    replay.raw_byte_size = static_cast<std::uint64_t>(
        state.at("raw_byte_size").as_number());
    replay.raw_sha256 = state.at("raw_sha256").as_string();
    replay.ready = state.at("ready").as_bool();
    const auto second = project(
        qualified_decoded(), tdx::Bytes(6, 0x32), replay);
    const auto& retained = second.at("projected_state");
    require(!second.at("decision").at("replaced").as_bool() &&
                retained.at("decoded_byte_size").as_number() == 9.0 &&
                retained.at("raw_byte_size").as_number() == 7.0 &&
                retained.at("decoded_sha256").as_string() ==
                    replay.decoded_sha256 &&
                retained.at("raw_sha256").as_string() == replay.raw_sha256 &&
                retained.at("ready").as_bool(),
            "projected state fields round-trip unchanged into retention");

    const auto maximum_previous = project(
        qualified_decoded(), tdx::Bytes{1},
        previous_state(
            static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())));
    require(maximum_previous.at("decision")
                    .at("previous_raw_logical_size")
                    .as_number() ==
                static_cast<double>(std::numeric_limits<std::int32_t>::max()) &&
                maximum_previous.at("decision").at("retained").as_bool(),
            "4651 accepts the native signed logical-size maximum");
    require_throws([] {
        (void)project(
            qualified_decoded(), tdx::Bytes{1},
            previous_state(
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int32_t>::max()) + 1U));
    }, "0..INT32_MAX");
    auto oversized_decoded = previous_state(1);
    oversized_decoded.decoded_byte_size =
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int32_t>::max()) + 1U;
    require_throws([&] {
        (void)project(
            qualified_decoded(), tdx::Bytes{1}, oversized_decoded);
    }, "0..INT32_MAX");
}

void test_caller_gate_and_input_boundaries() {
    require_throws([] {
        (void)project({}, tdx::Bytes{1});
    }, "decoded input must be non-empty");
    require_throws([] {
        (void)project(qualified_decoded(), {});
    }, "raw input must be non-empty");
    require_throws([] {
        (void)project(tdx::Bytes(5, 0xff), tdx::Bytes{1});
    }, "at least 6 bytes");
    require_throws([] {
        (void)project(tdx::Bytes(6, 0), tdx::Bytes{1});
    }, "offset 2");
    require_throws([] {
        (void)project(tdx::Bytes(maximum_snapshot_size + 1, 0xff),
                      tdx::Bytes{1});
    }, "384 KiB");
    require_throws([] {
        (void)project(qualified_decoded(),
                      tdx::Bytes(maximum_snapshot_size + 1, 0));
    }, "384 KiB");

    auto qualified_maximum = tdx::Bytes(maximum_snapshot_size, 0);
    qualified_maximum[2] = qualified_maximum[3] =
        qualified_maximum[4] = qualified_maximum[5] = 0xff;
    const auto result = project(
        std::move(qualified_maximum),
        tdx::Bytes(maximum_snapshot_size, 0));
    require(result.at("candidate")
                    .at("decoded_snapshot")
                    .at("byte_size")
                    .as_number() == static_cast<double>(maximum_snapshot_size) &&
                result.at("candidate")
                    .at("raw_snapshot")
                    .at("byte_size")
                    .as_number() == static_cast<double>(maximum_snapshot_size) &&
                result.at("caller_gate").at("passed").as_bool() &&
                !result.at("caller_gate").at("inferred_from_raw").as_bool(),
            "4651 accepts both inclusive bounds and validates decoded only");
}

void test_previous_metadata_and_no_body_echo() {
    auto invalid = previous_state(10);
    invalid.raw_sha256 = "abcd";
    require_throws([&] {
        (void)project(qualified_decoded(), tdx::Bytes{1}, invalid);
    }, "64 hexadecimal digits");

    const auto result = project(
        qualified_decoded(),
        tdx::Bytes{0xde, 0xad, 0xbe, 0xef}, previous_state(4));
    const auto rendered = result.dump(-1);
    require(rendered.find("deadbeef") == std::string::npos &&
                rendered.find("host_address") == std::string::npos,
            "4651 emits no body or host-address value");
    require_no_side_effects(result);
}

void test_cli_round_trip_and_strict_state() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-sdk-4651-dual-snapshot-cli-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    const auto decoded_path = directory / "decoded.bin";
    const auto raw_path = directory / "raw.bin";
    const auto smaller_raw_path = directory / "smaller.bin";
    const auto state_path = directory / "state.json";
    const auto invalid_state_path = directory / "invalid-state.json";
    const auto output_path = directory / "output.json";
    tdx::atomic_write_bytes(decoded_path, qualified_decoded(8));
    tdx::atomic_write_bytes(raw_path, tdx::Bytes(7, 0x51));
    tdx::atomic_write_bytes(smaller_raw_path, tdx::Bytes(6, 0x52));

    require(tdx::command_level2_project({
                "--format", "sdk-4651-dual-snapshot-transition",
                "--decoded-input", tdx::path_utf8(decoded_path),
                "--raw-input", tdx::path_utf8(raw_path), "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "4651 CLI accepts explicit decoded and raw snapshot files");
    auto first = tdx::Json::parse(tdx::read_text_utf8(output_path));
    tdx::atomic_write_text(
        state_path, first.at("projected_state").dump(-1));

    require(tdx::command_level2_project({
                "--format", "sdk-4651-dual-snapshot-transition",
                "--decoded-input", tdx::path_utf8(decoded_path),
                "--raw-input", tdx::path_utf8(smaller_raw_path),
                "--state-file", tdx::path_utf8(state_path), "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "4651 CLI consumes its projected_state directly");
    const auto second = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(second.at("decision").at("retained").as_bool() &&
                second.at("projected_state").at("raw_byte_size").as_number() ==
                    7.0,
            "4651 CLI preserves the larger prior raw logical state");
    require_no_side_effects(second);

    tdx::atomic_write_text(
        state_path, second.at("projected_state").dump(-1));
    require(tdx::command_level2_project({
                "--format", "sdk-4651-dual-snapshot-transition",
                "--decoded-input", tdx::path_utf8(decoded_path),
                "--raw-input", tdx::path_utf8(smaller_raw_path),
                "--state-file", tdx::path_utf8(state_path), "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "4651 CLI can replay a retained projected_state again");
    const auto third = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(third.at("decision").at("retained").as_bool() &&
                third.at("projected_state").at("raw_byte_size").as_number() ==
                    7.0,
            "4651 retained state stays schema-clean across repeated replay");
    require_no_side_effects(third);

    auto invalid = first.at("projected_state");
    invalid["unexpected"] = 1;
    tdx::atomic_write_text(invalid_state_path, invalid.dump(-1));
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-4651-dual-snapshot-transition",
            "--decoded-input", tdx::path_utf8(decoded_path),
            "--raw-input", tdx::path_utf8(smaller_raw_path),
            "--state-file", tdx::path_utf8(invalid_state_path)});
    }, "unexpected field");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-4651-dual-snapshot-transition",
            "--decoded-input", tdx::path_utf8(decoded_path),
            "--raw-input", tdx::path_utf8(raw_path), "--url",
            "https://invalid.example"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_absent_and_zero_previous_replace();
        test_size_replace_retain_and_equality();
        test_round_trip_and_signed_previous_boundary();
        test_caller_gate_and_input_boundaries();
        test_previous_metadata_and_no_body_echo();
        test_cli_round_trip_and_strict_state();
        std::cout << "level2 SDK 4651 dual snapshot transition tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 SDK 4651 dual snapshot transition tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
