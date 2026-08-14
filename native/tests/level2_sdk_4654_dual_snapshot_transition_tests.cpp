#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <cstddef>
#include <chrono>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

namespace {

constexpr std::size_t decoded_size = 48;
constexpr std::size_t maximum_snapshot_size = 384U * 1024U;

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

tdx::Json project(tdx::Bytes decoded, tdx::Bytes raw) {
    tdx::Level2Sdk4654DualSnapshotTransitionRequest request;
    request.decoded = std::move(decoded);
    request.raw = std::move(raw);
    return tdx::project_level2_sdk_4654_dual_snapshot_transition(request);
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
            "4654 projection preserves the complete offline boundary");
}

void test_exact_unconditional_replacement() {
    tdx::Bytes decoded(decoded_size, 0x11);
    tdx::Bytes raw{0xde, 0xad, 0xbe, 0xef, 0x22};
    const auto result = project(decoded, raw);
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-4654-dual-snapshot-transition-v1" &&
                result.at("format").as_string() ==
                    "sdk-4654-dual-snapshot-transition" &&
                result.at("dispatcher_qualified_input").as_bool() &&
                result.at("decoded_exact_byte_size").as_number() == 48.0 &&
                result.at("maximum_raw_byte_size").as_number() ==
                    static_cast<double>(maximum_snapshot_size),
            "4654 publishes the exact qualified input contract");

    const auto& replacement = result.at("replacement");
    require(replacement.at("decision").as_string() == "replace" &&
                replacement.at("unconditional").as_bool() &&
                replacement.at("replaced").as_bool() &&
                !replacement.at("retained").as_bool() &&
                !replacement.at("performed_against_host").as_bool(),
            "4654 always projects a replacement without touching host state");
    const auto& prior = replacement.at("previous_logical_state");
    require(!prior.at("input_required").as_bool() &&
                !prior.at("read").as_bool() &&
                !prior.at("retained").as_bool(),
            "4654 does not invent a previous state");

    const auto& state = result.at("projected_state");
    require(state.at("ready").as_bool() &&
                state.at("decoded_snapshot").at("byte_size").as_number() ==
                    48.0 &&
                state.at("raw_snapshot").at("byte_size").as_number() == 5.0 &&
                state.at("decoded_snapshot").at("sha256").as_string().size() ==
                    64 &&
                state.at("raw_snapshot").at("sha256").as_string().size() == 64 &&
                !state.at("decoded_snapshot").at("body_emitted").as_bool() &&
                !state.at("raw_snapshot").at("body_emitted").as_bool(),
            "4654 reports two complete fingerprints without bodies");
    require(state.at("decoded_snapshot").at("sha256").as_string() !=
                state.at("raw_snapshot").at("sha256").as_string(),
            "explicit decoded and raw snapshots stay independent");

    const auto rendered = result.dump(-1);
    require(rendered.find("deadbeef22") == std::string::npos &&
                rendered.find("host_address") == std::string::npos,
            "4654 emits neither bodies nor host addresses");
    require_no_side_effects(result);
}

void test_dispatcher_qualified_size_boundaries() {
    for (const auto size : {decoded_size - 1, decoded_size + 1}) {
        require_throws([=] {
            (void)project(tdx::Bytes(size, 0), tdx::Bytes{1});
        }, "exactly 48");
    }
    require_throws([] {
        (void)project(tdx::Bytes(decoded_size, 0), {});
    }, "raw input must be non-empty");
    require_throws([] {
        (void)project(tdx::Bytes(decoded_size, 0),
                      tdx::Bytes(maximum_snapshot_size + 1, 0));
    }, "384 KiB");

    const auto maximum = project(
        tdx::Bytes(decoded_size, 0),
        tdx::Bytes(maximum_snapshot_size, 0));
    require(maximum.at("projected_state")
                    .at("raw_snapshot")
                    .at("byte_size")
                    .as_number() == static_cast<double>(maximum_snapshot_size),
            "4654 accepts the inclusive 384 KiB raw boundary");
}

void test_cli_explicit_inputs_and_strict_arguments() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-sdk-4654-dual-snapshot-cli-" + suffix);
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
    const auto output_path = directory / "output.json";
    tdx::atomic_write_bytes(decoded_path, tdx::Bytes(decoded_size, 0x31));
    tdx::atomic_write_bytes(raw_path, tdx::Bytes{0x41, 0x42, 0x43});
    require(tdx::command_level2_project({
                "--format", "sdk-4654-dual-snapshot-transition",
                "--decoded-input", tdx::path_utf8(decoded_path),
                "--raw-input", tdx::path_utf8(raw_path), "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "4654 CLI accepts two explicit bounded snapshot files");
    const auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("function_id").as_number() == 4654.0 &&
                result.at("dispatcher_qualified_input").as_bool() &&
                result.at("projected_state")
                        .at("raw_snapshot")
                        .at("byte_size")
                        .as_number() == 3.0,
            "4654 CLI returns the typed domain result");
    require_no_side_effects(result);

    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-4654-dual-snapshot-transition",
            "--decoded-input", tdx::path_utf8(decoded_path),
            "--raw-input", tdx::path_utf8(raw_path), "--state-file",
            "not-accepted.json"});
    }, "does not accept --state-file");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-4654-dual-snapshot-transition",
            "--decoded-input", tdx::path_utf8(decoded_path),
            "--raw-input", tdx::path_utf8(raw_path), "--encoding", "raw"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_exact_unconditional_replacement();
        test_dispatcher_qualified_size_boundaries();
        test_cli_explicit_inputs_and_strict_arguments();
        std::cout << "level2 SDK 4654 dual snapshot transition tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 SDK 4654 dual snapshot transition tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
