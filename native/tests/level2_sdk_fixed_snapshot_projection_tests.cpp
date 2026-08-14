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

constexpr std::size_t body_1803_size = 32016;
constexpr std::size_t body_18031_size = 20012;

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

tdx::Json project(tdx::Level2SdkHostSnapshotDataType data_type,
                  tdx::Bytes body) {
    tdx::Level2SdkHostSnapshotReplacementRequest request;
    request.data_type = data_type;
    request.body = std::move(body);
    return tdx::project_level2_sdk_host_snapshot_replacement(request);
}

const tdx::Json& replacement(const tdx::Json& result) {
    return result.at("replacement");
}

void require_offline_boundary(const tdx::Json& result) {
    require(!result.at("input_body_retained").as_bool() &&
                !result.at("raw_body_emitted").as_bool() &&
                !result.at("body_transformed").as_bool() &&
                !result.at("field_projection_performed").as_bool() &&
                !result.at("decoded_summary_included").as_bool() &&
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
                !result.at("entitlement_bypass").as_bool() &&
                result.at("offline").as_bool(),
            "fixed snapshot projection preserves every no-side-effect boundary");
}

void require_replacement_contract(const tdx::Json& result,
                                  int data_type, std::size_t body_size,
                                  const std::string& expected_hash) {
    require_number(result.at("data_type"),
                   static_cast<std::uint64_t>(data_type),
                   "snapshot projection preserves the data type");
    require_number(result.at("source_body_size"), body_size,
                   "snapshot projection reports exact source size");
    require_number(result.at("projected_logical_state_size"), body_size,
                   "new logical state retains exact source size");
    require_number(result.at("maximum_input_body_size"), 384U * 1024U,
                   "snapshot projection publishes the 384 KiB boundary");
    require(result.at("projected_logical_state_sha256").as_string() ==
                expected_hash,
            "snapshot projection reports exact SHA-256");

    const auto& prior = replacement(result).at("previous_logical_state");
    require(!prior.at("input_required").as_bool() &&
                !prior.at("read").as_bool() &&
                !prior.at("retained").as_bool() &&
                prior.at("disposition").as_string() == "replaced-in-full",
            "complete replacement does not invent a previous-state input");
    const auto& next = replacement(result).at("new_logical_state");
    require_number(next.at("byte_size"), body_size,
                   "replacement metadata reports the next-state size");
    require(next.at("sha256").as_string() == expected_hash &&
                !next.at("body_emitted").as_bool(),
            "replacement metadata fingerprints without echoing next state");
    require(replacement(result).at("scope").as_string() ==
                "complete-fixed-size-logical-state" &&
                replacement(result).at("source_copy_semantics").as_string() ==
                    "byte-for-byte-without-field-transform" &&
                replacement(result).at("projected").as_bool() &&
                !replacement(result).at("performed_against_host").as_bool(),
            "replacement is byte-exact and performs no invented transform");
    require_offline_boundary(result);
}

void test_exact_1803_replacement() {
    constexpr const char* zero_hash =
        "e928a3dcebdf9258f7b0824ae21fafae7e02e14b38ea4735ad6c9250632031f8";
    const auto result = project(
        tdx::Level2SdkHostSnapshotDataType::multi_level_quote,
        tdx::Bytes(body_1803_size, 0));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-1803-host-snapshot-replacement-v1" &&
                result.at("format").as_string() ==
                    "sdk-1803-host-projection" &&
                result.at("callback_kind").as_string() ==
                    "multi-level-quote",
            "1803 fixed snapshot schema is stable");
    require_replacement_contract(result, 1803, body_1803_size, zero_hash);
}

void test_exact_18031_replacement() {
    constexpr const char* zero_hash =
        "8db8822291212268576b1ba07a96216b3a7ab7a7fd83319fe7ef334746979327";
    const auto result = project(
        tdx::Level2SdkHostSnapshotDataType::order_queue_at_price,
        tdx::Bytes(body_18031_size, 0));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-18031-host-snapshot-replacement-v1" &&
                result.at("format").as_string() ==
                    "sdk-18031-host-projection" &&
                result.at("callback_kind").as_string() ==
                    "order-queue-at-price",
            "18031 fixed snapshot schema is stable");
    require_replacement_contract(result, 18031, body_18031_size, zero_hash);
}

void test_no_body_echo_or_field_projection() {
    auto body = tdx::Bytes(body_1803_size, 0);
    constexpr unsigned char marker[]{
        0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe};
    for (std::size_t index = 0; index < sizeof(marker); ++index)
        body[index] = marker[index];
    const auto result = project(
        tdx::Level2SdkHostSnapshotDataType::multi_level_quote,
        std::move(body));
    const auto rendered = result.dump(-1);
    require(rendered.find("deadbeefcafebabe") == std::string::npos &&
                rendered.find("header_u32") == std::string::npos &&
                rendered.find("host_address") == std::string::npos,
            "projection emits neither raw bytes, decoded fields, nor an address");
}

void test_domain_size_and_type_boundaries() {
    for (const auto size : {body_1803_size - 1, body_1803_size + 1}) {
        require_throws([=] {
            (void)project(
                tdx::Level2SdkHostSnapshotDataType::multi_level_quote,
                tdx::Bytes(size, 0));
        }, "exactly 32016");
    }
    for (const auto size : {body_18031_size - 1, body_18031_size + 1}) {
        require_throws([=] {
            (void)project(
                tdx::Level2SdkHostSnapshotDataType::order_queue_at_price,
                tdx::Bytes(size, 0));
        }, "exactly 20012");
    }
    require_throws([] {
        (void)project(
            static_cast<tdx::Level2SdkHostSnapshotDataType>(1804),
            tdx::Bytes(432, 0));
    }, "must be 1803 or 18031");
    require_throws([] {
        (void)project(
            tdx::Level2SdkHostSnapshotDataType::multi_level_quote,
            tdx::Bytes(384U * 1024U + 1U, 0));
    }, "384 KiB");
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

void test_cli_formats_and_pre_read_bounds() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-sdk-fixed-snapshot-projection-cli-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    const auto raw_path = directory / "1803.bin";
    const auto hex_path = directory / "18031.hex";
    const auto output_path = directory / "output.json";
    tdx::atomic_write_bytes(raw_path, tdx::Bytes(body_1803_size, 0));
    tdx::atomic_write_text(
        hex_path, body_hex(tdx::Bytes(body_18031_size, 0)));

    require(tdx::command_level2_project({
                "--format", "sdk-1803-host-projection", "--input",
                tdx::path_utf8(raw_path), "--encoding", "raw", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "raw CLI 1803 fixed snapshot projection succeeds");
    auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("format").as_string() ==
                "sdk-1803-host-projection" &&
                result.at("projected_logical_state_sha256").as_string() ==
                    "e928a3dcebdf9258f7b0824ae21fafae7e02e14b38ea4735ad6c9250632031f8",
            "raw CLI preserves the 1803 body fingerprint");
    require_offline_boundary(result);

    require(tdx::command_level2_project({
                "--format", "sdk-18031-host-projection", "--input",
                tdx::path_utf8(hex_path), "--encoding", "hex", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "hex CLI 18031 fixed snapshot projection succeeds");
    result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("format").as_string() ==
                "sdk-18031-host-projection" &&
                result.at("projected_logical_state_sha256").as_string() ==
                    "8db8822291212268576b1ba07a96216b3a7ab7a7fd83319fe7ef334746979327",
            "hex CLI preserves the 18031 body fingerprint");
    require_offline_boundary(result);

    const auto wrong_path = directory / "wrong.bin";
    const auto oversized_raw_path = directory / "oversized.bin";
    const auto oversized_hex_path = directory / "oversized.hex";
    const auto invalid_hex_path = directory / "invalid.hex";
    tdx::atomic_write_bytes(wrong_path, tdx::Bytes(body_1803_size - 1, 0));
    tdx::atomic_write_bytes(
        oversized_raw_path, tdx::Bytes(384U * 1024U + 1U, 0));
    tdx::atomic_write_text(
        oversized_hex_path, std::string(768U * 1024U + 1U, '0'));
    tdx::atomic_write_text(
        invalid_hex_path, std::string(body_18031_size * 2 - 1, '0') + "g");

    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1803-host-projection", "--input",
            tdx::path_utf8(wrong_path), "--encoding", "raw"});
    }, "exactly 32016");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1803-host-projection", "--input",
            tdx::path_utf8(oversized_raw_path), "--encoding", "raw"});
    }, "384 KiB");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-18031-host-projection", "--input",
            tdx::path_utf8(oversized_hex_path), "--encoding", "hex"});
    }, "768 KiB");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-18031-host-projection", "--input",
            tdx::path_utf8(invalid_hex_path), "--encoding", "hex"});
    }, "non-hexadecimal");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1803-host-projection", "--input",
            tdx::path_utf8(raw_path)});
    }, "requires --encoding");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1803-host-projection", "--input",
            tdx::path_utf8(raw_path), "--encoding", "raw", "--state-file",
            "not-needed.json"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_exact_1803_replacement();
        test_exact_18031_replacement();
        test_no_body_echo_or_field_projection();
        test_domain_size_and_type_boundaries();
        test_cli_formats_and_pre_read_bounds();
        std::cout << "level2 SDK 1803/18031 fixed snapshot projection tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 SDK 1803/18031 fixed snapshot projection tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
