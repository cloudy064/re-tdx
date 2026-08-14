#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

namespace {

constexpr std::size_t companion_size = 46;
constexpr std::size_t header_size = 39;
constexpr std::size_t record_size = 18;
constexpr std::size_t attach_size = 120;

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

tdx::Bytes raw_snapshot(std::int16_t count, std::int8_t attach,
                        std::uint8_t fill = 0x5a) {
    const auto signed_size = static_cast<std::int64_t>(header_size) +
        static_cast<std::int64_t>(record_size) * count +
        static_cast<std::int64_t>(attach_size) * attach;
    if (signed_size < 31) throw tdx::Error("invalid raw test fixture size");
    tdx::Bytes raw(static_cast<std::size_t>(signed_size), fill);
    raw[28] = static_cast<std::uint8_t>(attach);
    const auto count_bits = static_cast<std::uint16_t>(count);
    raw[29] = static_cast<std::uint8_t>(count_bits & 0xffU);
    raw[30] = static_cast<std::uint8_t>(count_bits >> 8U);
    return raw;
}

tdx::Json project(tdx::Bytes companion, tdx::Bytes raw) {
    tdx::Level2Sdk4655CompanionRawTransitionRequest request;
    request.companion_snapshot = std::move(companion);
    request.raw = std::move(raw);
    return tdx::project_level2_sdk_4655_companion_raw_transition(request);
}

void require_offline(const tdx::Json& value) {
    require(!value.at("input_body_retained").as_bool() &&
                !value.at("companion_body_emitted").as_bool() &&
                !value.at("raw_body_emitted").as_bool() &&
                !value.at("raw_to_companion_conversion_performed").as_bool() &&
                !value.at("callback_executed").as_bool() &&
                !value.at("sdk_called").as_bool() &&
                !value.at("host_state_gate_evaluated").as_bool() &&
                !value.at("host_state_write_performed").as_bool() &&
                !value.at("host_notification_executed").as_bool() &&
                !value.at("host_message_dispatch_attempted").as_bool() &&
                value.at("host_messages_sent").as_number() == 0.0 &&
                !value.at("wire_bytes_built").as_bool() &&
                value.at("network_requests").as_number() == 0.0 &&
                !value.at("request_sent").as_bool() &&
                !value.at("subscription_sent").as_bool() &&
                !value.at("entitlement_bypass").as_bool() &&
                value.at("offline").as_bool(),
            "4655 projection remains entirely offline and unexecuted");
}

void test_minimum_record_and_unresolved_gate() {
    const auto result = project(tdx::Bytes(companion_size, 0x41),
                                raw_snapshot(1, 0));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-4655-companion-raw-transition-v1" &&
                result.at("format").as_string() ==
                    "sdk-4655-companion-raw-transition" &&
                result.at("dispatcher_qualified").as_bool() &&
                result.at("companion_exact_byte_size").as_number() == 46.0 &&
                !result.at("companion_input_derived_from_raw").as_bool() &&
                !result.at("actual_host_replacement_claimed").as_bool(),
            "4655 publishes the companion/raw evidence boundary");
    require(result.at("raw_records").at("count_raw_signed_i16").as_number() ==
                1.0 &&
                result.at("raw_records").at("stride").as_number() == 18.0 &&
                result.at("raw_records").at("iteration_order").as_string() ==
                    "raw-source-index-ascending",
            "4655 preserves only proven raw record layout metadata");
    const auto& gate = result.at("host_state_gate");
    require(!gate.at("inputs_available_in_request").as_bool() &&
                !gate.at("evaluated").as_bool() &&
                gate.at("replacement_branch_selected").is_null(),
            "4655 leaves its unavailable host-state gate unresolved");
    const auto& state = result.at("projected_state_candidate");
    require(state.at("conditional").as_bool() &&
                state.at("companion_snapshot").at("byte_size").as_number() ==
                    46.0 &&
                state.at("raw_snapshot").at("byte_size").as_number() == 57.0 &&
                state.at("companion_snapshot").at("sha256").as_string().size() ==
                    64 &&
                state.at("raw_snapshot").at("sha256").as_string().size() == 64,
            "4655 candidate fingerprints both inputs without echoing bodies");
    require_offline(result);
}

void test_attach_and_shape_boundaries() {
    const auto attached = project(tdx::Bytes(companion_size, 0x31),
                                  raw_snapshot(0, 1, 0x62));
    const auto& attach = attached.at("projected_state_candidate")
                             .at("attach_snapshot");
    require(attach.at("present").as_bool() &&
                attach.at("offset").as_number() == 39.0 &&
                attach.at("byte_size").as_number() == 120.0 &&
                attach.at("sha256").as_string().size() == 64 &&
                !attach.at("body_emitted").as_bool(),
            "4655 accepts the count-zero attach-one dispatcher shape");
    require_offline(attached);

    const auto signed_count = project(tdx::Bytes(companion_size, 0x32),
                                      raw_snapshot(-1, 1, 0x63));
    const auto& signed_attach = signed_count.at("projected_state_candidate")
                                    .at("attach_snapshot");
    require(signed_count.at("raw_records")
                    .at("count_raw_signed_i16")
                    .as_number() == -1.0 &&
                signed_count.at("raw_records")
                        .at("iteration_count_effective")
                        .as_number() == 0.0 &&
                signed_attach.at("offset").as_number() == 21.0 &&
                signed_attach.at("byte_size").as_number() == 120.0,
            "4655 preserves the dispatcher signed-count attach shape");
    require_offline(signed_count);

    for (const auto size : {companion_size - 1, companion_size + 1})
        require_throws([=] {
            (void)project(tdx::Bytes(size, 0), raw_snapshot(1, 0));
        }, "exactly 46");
    require_throws([] {
        auto raw = raw_snapshot(1, 0);
        raw.pop_back();
        (void)project(tdx::Bytes(companion_size, 0), std::move(raw));
    }, "at least 57");
    require_throws([] {
        auto raw = raw_snapshot(2, 0);
        raw.pop_back();
        (void)project(tdx::Bytes(companion_size, 0), std::move(raw));
    }, "must equal");
}

void test_cli_companion_naming_and_strict_arguments() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-sdk-4655-companion-cli-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};
    const auto companion_path = directory / "companion.bin";
    const auto raw_path = directory / "raw.bin";
    const auto output_path = directory / "output.json";
    tdx::atomic_write_bytes(companion_path, tdx::Bytes(companion_size, 0x71));
    tdx::atomic_write_bytes(raw_path, raw_snapshot(1, 0, 0x72));

    require(tdx::command_level2_project({
                "--format", "sdk-4655-companion-raw-transition",
                "--companion-input", tdx::path_utf8(companion_path),
                "--raw-input", tdx::path_utf8(raw_path), "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "4655 CLI accepts explicit companion and raw files");
    const auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("function_id").as_number() == 4655.0 &&
                result.at("dispatcher_qualified").as_bool(),
            "4655 CLI returns the typed conditional projection");
    require_offline(result);
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-4655-companion-raw-transition",
            "--decoded-input", tdx::path_utf8(companion_path),
            "--raw-input", tdx::path_utf8(raw_path)});
    }, "requires --companion-input");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-4655-companion-raw-transition",
            "--companion-input", tdx::path_utf8(companion_path),
            "--raw-input", tdx::path_utf8(raw_path), "--url",
            "https://invalid.example"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_minimum_record_and_unresolved_gate();
        test_attach_and_shape_boundaries();
        test_cli_companion_naming_and_strict_arguments();
        std::cout << "level2 SDK 4655 companion/raw transition tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 SDK 4655 companion/raw transition tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
