#include "level2_sdk_correlation_transition_command.hpp"

#include "tdx/level2.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::level2_detail {
namespace {

constexpr std::uintmax_t maximum_transition_json_size = 4U * 1024U * 1024U;
constexpr std::size_t maximum_registry_records = 10000;

fs::path transition_path_from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
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
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuation_count = 2;
            codepoint = first & 0x0fU;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation_count = 3;
            codepoint = first & 0x07U;
            minimum = 0x10000;
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

const Json& member(const Json& object, const char* name,
                   std::string_view path) {
    if (!object.is_object())
        throw Error(std::string(path) + " must be an object");
    const auto iterator = object.as_object().find(name);
    if (iterator == object.as_object().end())
        throw Error(std::string(path) + " requires " + name);
    return iterator->second;
}

void exact_object(const Json& value, const std::string& path,
                  std::initializer_list<const char*> allowed) {
    if (!value.is_object()) throw Error(path + " must be an object");
    for (const auto& [name, unused] : value.as_object()) {
        (void)unused;
        const bool known = std::any_of(
            allowed.begin(), allowed.end(), [&](const char* candidate) {
                return name == candidate;
            });
        if (!known) throw Error(path + " contains unexpected field " + name);
    }
    for (const auto* name : allowed) (void)member(value, name, path);
}

std::string required_string(const Json& value, const std::string& path) {
    if (!value.is_string()) throw Error(path + " must be a string");
    return value.as_string();
}

std::uint32_t u32_number(const Json& value, const std::string& path) {
    if (!value.is_number() || !std::isfinite(value.as_number()) ||
        std::floor(value.as_number()) != value.as_number() ||
        value.as_number() < 0.0 ||
        value.as_number() >
            static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
        throw Error(path + " must be an unsigned 32-bit integer");
    return static_cast<std::uint32_t>(value.as_number());
}

std::uint16_t u16_number(const Json& value, const std::string& path) {
    const auto result = u32_number(value, path);
    if (result > std::numeric_limits<std::uint16_t>::max())
        throw Error(path + " must be an unsigned 16-bit integer");
    return static_cast<std::uint16_t>(result);
}

Level2SdkCallbackDataType local_data_type(const Json& value,
                                          const std::string& path) {
    switch (u32_number(value, path)) {
    case 1801: return Level2SdkCallbackDataType::transaction;
    case 1802: return Level2SdkCallbackDataType::order;
    case 1803: return Level2SdkCallbackDataType::multi_level_quote;
    case 1804: return Level2SdkCallbackDataType::price_queue;
    case 1807: return Level2SdkCallbackDataType::quote_update;
    case 18071: return Level2SdkCallbackDataType::extended_quote;
    default:
        throw Error(path +
                    " must be local-registry data type 1801, 1802, 1803, "
                    "1804, 1807, or 18071");
    }
}

Level2SdkCorrelationRecord parse_record(const Json& value,
                                        const std::string& path) {
    exact_object(value, path,
                 {"callback_key_1_raw", "callback_key_2_raw", "market_raw",
                  "code_ascii", "reserved_zero", "data_type",
                  "registry_mode_raw"});
    if (u32_number(member(value, "reserved_zero", path),
                   path + ".reserved_zero") != 0)
        throw Error(path + ".reserved_zero must be zero");

    Level2SdkCorrelationRecord result;
    result.callback_key_1_raw = u32_number(
        member(value, "callback_key_1_raw", path),
        path + ".callback_key_1_raw");
    result.callback_key_2_raw = u32_number(
        member(value, "callback_key_2_raw", path),
        path + ".callback_key_2_raw");
    result.market_raw = u16_number(member(value, "market_raw", path),
                                   path + ".market_raw");
    result.code_ascii = required_string(member(value, "code_ascii", path),
                                        path + ".code_ascii");
    result.data_type = local_data_type(member(value, "data_type", path),
                                       path + ".data_type");
    result.registry_mode_raw = u32_number(
        member(value, "registry_mode_raw", path),
        path + ".registry_mode_raw");
    return result;
}

std::vector<Level2SdkCorrelationRecord> parse_registry(const Json& value) {
    constexpr const char* path = "input.previous_registry";
    exact_object(value, path,
                 {"schema", "record_size", "record_count", "records"});
    if (required_string(member(value, "schema", path),
                        std::string(path) + ".schema") !=
        "tdx-level2-sdk-correlation-registry-state-v1")
        throw Error(
            "input.previous_registry.schema must be "
            "tdx-level2-sdk-correlation-registry-state-v1");
    if (u32_number(member(value, "record_size", path),
                   std::string(path) + ".record_size") != 25)
        throw Error("input.previous_registry.record_size must be 25");
    const auto& records = member(value, "records", path);
    if (!records.is_array())
        throw Error("input.previous_registry.records must be an array");
    if (records.size() > maximum_registry_records)
        throw Error(
            "input.previous_registry.records must contain at most 10000 "
            "records");
    const auto declared_count = u32_number(
        member(value, "record_count", path),
        std::string(path) + ".record_count");
    if (declared_count != records.size())
        throw Error(
            "input.previous_registry.record_count must equal records.size");

    std::vector<Level2SdkCorrelationRecord> result;
    result.reserve(records.size());
    for (std::size_t index = 0; index < records.size(); ++index)
        result.push_back(parse_record(
            records.as_array()[index],
            "input.previous_registry.records[" + std::to_string(index) + "]"));
    return result;
}

Level2SdkCorrelationTransitionRequest parse_transition_input(
    const Json& input) {
    if (!input.is_object()) throw Error("input must be a JSON object");
    const auto schema = required_string(member(input, "schema", "input"),
                                        "input.schema");
    if (schema != "tdx-level2-sdk-correlation-transition-input-v1")
        throw Error(
            "input.schema must be "
            "tdx-level2-sdk-correlation-transition-input-v1");
    const auto operation = required_string(
        member(input, "operation", "input"), "input.operation");

    Level2SdkCorrelationTransitionRequest request;
    if (operation == "callback-match") {
        exact_object(input, "input",
                     {"schema", "operation", "previous_registry",
                      "callback"});
        const auto& callback = member(input, "callback", "input");
        exact_object(callback, "input.callback",
                     {"data_type", "callback_key_1_raw",
                      "callback_key_2_raw"});
        Level2SdkCorrelationCallbackMatch action;
        action.data_type = local_data_type(
            member(callback, "data_type", "input.callback"),
            "input.callback.data_type");
        action.callback_key_1_raw = u32_number(
            member(callback, "callback_key_1_raw", "input.callback"),
            "input.callback.callback_key_1_raw");
        action.callback_key_2_raw = u32_number(
            member(callback, "callback_key_2_raw", "input.callback"),
            "input.callback.callback_key_2_raw");
        request.kind = Level2SdkCorrelationTransitionKind::callback_match;
        request.callback_match = action;
    } else if (operation == "sdk-key-upsert") {
        exact_object(input, "input",
                     {"schema", "operation", "previous_registry",
                      "sdk_key_upsert"});
        request.kind = Level2SdkCorrelationTransitionKind::sdk_key_upsert;
        request.sdk_key_upsert = parse_record(
            member(input, "sdk_key_upsert", "input"),
            "input.sdk_key_upsert");
    } else {
        throw Error(
            "input.operation must be callback-match or sdk-key-upsert");
    }
    request.previous_registry = parse_registry(
        member(input, "previous_registry", "input"));
    return request;
}

Json read_transition_input(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error)
        throw Error("level2 correlation transition cannot inspect --input: " +
                    error.message());
    if (size == 0 || size > maximum_transition_json_size)
        throw Error(
            "level2 correlation transition --input must be a non-empty JSON "
            "file no larger than 4 MiB");
    const auto bytes = read_bytes(path);
    const std::string text(reinterpret_cast<const char*>(bytes.data()),
                           bytes.size());
    if (!valid_utf8(text))
        throw Error(
            "level2 correlation transition --input must be valid UTF-8");
    auto value = Json::parse(text);
    if (!valid_json_utf8(value))
        throw Error(
            "level2 correlation transition --input contains an invalid "
            "Unicode scalar sequence");
    if (!value.is_object())
        throw Error(
            "level2 correlation transition --input must contain a JSON object");
    return value;
}

}  // namespace

int command_level2_sdk_correlation_transition(Args& args) {
    const auto input_name = args.take_option("--input");
    if (input_name.empty())
        throw Error(
            "level2 correlation transition requires --input JSON_FILE");
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto input = read_transition_input(
        transition_path_from_utf8(input_name));
    const auto request = parse_transition_input(input);
    const auto rendered = project_level2_sdk_correlation_transition(request)
                              .dump(compact ? -1 : 2) + "\n";
    if (output_name.empty()) std::cout << rendered;
    else atomic_write_text(transition_path_from_utf8(output_name), rendered);
    return 0;
}

}  // namespace tdx::level2_detail
