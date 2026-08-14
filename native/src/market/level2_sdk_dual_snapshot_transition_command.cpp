#include "level2_sdk_dual_snapshot_transition_command.hpp"

#include "tdx/level2.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::level2_detail {
namespace {

constexpr std::uintmax_t maximum_snapshot_size = level2_offline_payload_limit;
constexpr std::uintmax_t maximum_state_file_size = level2_offline_payload_limit;

fs::path command_path_from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::uintmax_t checked_file_size(const fs::path& path,
                                 const char* option) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error)
        throw Error(std::string("level2 SDK dual snapshot transition cannot inspect ") +
                    option + ": " + error.message());
    return size;
}

Bytes read_snapshot(const fs::path& path, const char* option) {
    const auto size = checked_file_size(path, option);
    if (size == 0 || size > maximum_snapshot_size)
        throw Error(std::string(option) +
                    " must be a non-empty file no larger than 384 KiB");
    auto result = read_bytes(path);
    if (result.empty() || result.size() > maximum_snapshot_size)
        throw Error(std::string(option) +
                    " must contain 1..393216 bytes");
    return result;
}

bool valid_utf8(std::string_view text) {
    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = static_cast<unsigned char>(text[index++]);
        if (first <= 0x7f) continue;
        std::size_t continuation_count{};
        std::uint32_t codepoint{};
        std::uint32_t minimum{};
        if (first >= 0xc2 && first <= 0xdf) {
            continuation_count = 1;
            codepoint = first & 0x1fU;
            minimum = 0x80U;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuation_count = 2;
            codepoint = first & 0x0fU;
            minimum = 0x800U;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation_count = 3;
            codepoint = first & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }
        if (continuation_count > text.size() - index) return false;
        for (std::size_t count = 0; count < continuation_count; ++count) {
            const auto next = static_cast<unsigned char>(text[index++]);
            if ((next & 0xc0U) != 0x80U) return false;
            codepoint = (codepoint << 6U) | (next & 0x3fU);
        }
        if (codepoint < minimum || codepoint > 0x10ffffU ||
            (codepoint >= 0xd800U && codepoint <= 0xdfffU))
            return false;
    }
    return true;
}

bool valid_json_utf8(const Json& value) {
    if (value.is_string()) return valid_utf8(value.as_string());
    if (value.is_array()) {
        return std::all_of(
            value.as_array().begin(), value.as_array().end(),
            [](const Json& item) { return valid_json_utf8(item); });
    }
    if (value.is_object()) {
        return std::all_of(
            value.as_object().begin(), value.as_object().end(),
            [](const auto& item) {
                return valid_utf8(item.first) && valid_json_utf8(item.second);
            });
    }
    return true;
}

const Json& member(const Json& object, const char* name) {
    const auto iterator = object.as_object().find(name);
    if (iterator == object.as_object().end())
        throw Error(std::string("--state-file requires field ") + name);
    return iterator->second;
}

void exact_state_object(const Json& value) {
    if (!value.is_object())
        throw Error("--state-file must contain a JSON object");
    constexpr const char* allowed[]{
        "schema", "decoded_byte_size", "decoded_sha256", "raw_byte_size",
        "raw_sha256", "ready", "source", "metadata_complete",
        "bodies_emitted"};
    for (const auto& [name, unused] : value.as_object()) {
        (void)unused;
        if (std::find(std::begin(allowed), std::end(allowed), name) ==
            std::end(allowed))
            throw Error("--state-file contains unexpected field " + name);
    }
    for (const auto* name : allowed) (void)member(value, name);
}

std::uint64_t logical_size(const Json& value, const char* name) {
    const auto& field = member(value, name);
    if (!field.is_number() || !std::isfinite(field.as_number()) ||
        std::floor(field.as_number()) != field.as_number() ||
        field.as_number() < 0.0 ||
        field.as_number() >
            static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        throw Error(std::string("--state-file.") + name +
                    " must be an integer in 0..INT32_MAX");
    return static_cast<std::uint64_t>(field.as_number());
}

std::string string_field(const Json& value, const char* name) {
    const auto& field = member(value, name);
    if (!field.is_string())
        throw Error(std::string("--state-file.") + name +
                    " must be a string");
    return field.as_string();
}

bool bool_field(const Json& value, const char* name) {
    const auto& field = member(value, name);
    if (!field.is_bool())
        throw Error(std::string("--state-file.") + name +
                    " must be a boolean");
    return field.as_bool();
}

Level2Sdk4651DualSnapshotPreviousState read_previous_state(
    const fs::path& path) {
    const auto size = checked_file_size(path, "--state-file");
    if (size == 0 || size > maximum_state_file_size)
        throw Error(
            "--state-file must be a non-empty JSON file no larger than 384 KiB");
    const auto bytes = read_bytes(path);
    const std::string text(reinterpret_cast<const char*>(bytes.data()),
                           bytes.size());
    if (!valid_utf8(text))
        throw Error("--state-file must be valid UTF-8 JSON");
    const auto state = Json::parse(text);
    if (!valid_json_utf8(state))
        throw Error("--state-file contains an invalid Unicode scalar sequence");
    exact_state_object(state);
    if (string_field(state, "schema") !=
        "tdx-level2-sdk-4651-dual-snapshot-state-v1")
        throw Error(
            "--state-file.schema must be "
            "tdx-level2-sdk-4651-dual-snapshot-state-v1");
    (void)string_field(state, "source");
    if (!bool_field(state, "metadata_complete"))
        throw Error("--state-file.metadata_complete must be true");
    if (bool_field(state, "bodies_emitted"))
        throw Error("--state-file.bodies_emitted must be false");

    Level2Sdk4651DualSnapshotPreviousState result;
    result.decoded_byte_size = logical_size(state, "decoded_byte_size");
    result.decoded_sha256 = string_field(state, "decoded_sha256");
    result.raw_byte_size = logical_size(state, "raw_byte_size");
    result.raw_sha256 = string_field(state, "raw_sha256");
    result.ready = bool_field(state, "ready");
    return result;
}

}  // namespace

int command_level2_sdk_dual_snapshot_transition(Args& args,
                                                int function_id) {
    if (function_id != 4654 && function_id != 4651)
        throw Error("internal SDK dual snapshot transition function id is invalid");
    const auto decoded_name = args.take_option("--decoded-input");
    const auto raw_name = args.take_option("--raw-input");
    if (decoded_name.empty() || raw_name.empty())
        throw Error(
            "level2 SDK dual snapshot transition requires --decoded-input "
            "FILE and --raw-input FILE");
    const auto state_name = args.take_option("--state-file");
    if (function_id == 4654 && !state_name.empty())
        throw Error("SDK 4654 dual snapshot transition does not accept --state-file");
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    auto decoded = read_snapshot(
        command_path_from_utf8(decoded_name), "--decoded-input");
    auto raw = read_snapshot(
        command_path_from_utf8(raw_name), "--raw-input");
    Json result;
    if (function_id == 4654) {
        Level2Sdk4654DualSnapshotTransitionRequest request;
        request.decoded = std::move(decoded);
        request.raw = std::move(raw);
        result = project_level2_sdk_4654_dual_snapshot_transition(request);
    } else {
        Level2Sdk4651DualSnapshotTransitionRequest request;
        request.decoded = std::move(decoded);
        request.raw = std::move(raw);
        if (!state_name.empty())
            request.previous = read_previous_state(
                command_path_from_utf8(state_name));
        result = project_level2_sdk_4651_dual_snapshot_transition(request);
    }

    const auto rendered = result.dump(compact ? -1 : 2) + "\n";
    if (output_name.empty()) std::cout << rendered;
    else atomic_write_text(command_path_from_utf8(output_name), rendered);
    return 0;
}

}  // namespace tdx::level2_detail
