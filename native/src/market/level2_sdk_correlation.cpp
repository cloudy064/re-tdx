#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t correlation_record_size = 25;
constexpr std::size_t native_registry_record_limit = 10001;

std::string bytes_hex(const std::uint8_t* data, std::size_t size) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index)
        output << std::setw(2) << static_cast<unsigned>(data[index]);
    return output.str();
}

std::string correlation_code(const std::uint8_t* data,
                             std::size_t record_index) {
    std::string result;
    bool terminated = false;
    for (std::size_t index = 0; index < 6; ++index) {
        const auto value = data[index];
        if (!value) {
            terminated = true;
            continue;
        }
        if (terminated)
            throw Error("SDK correlation record " +
                        std::to_string(record_index) +
                        " has non-zero code bytes after NUL padding");
        if (value < 0x20 || value > 0x7e)
            throw Error("SDK correlation record " +
                        std::to_string(record_index) +
                        " code is not printable ASCII");
        result.push_back(static_cast<char>(value));
    }
    return result;
}

Json field_layout(const char* name, std::size_t offset, std::size_t size,
                  const char* type) {
    Json field = Json::object();
    field["name"] = name;
    field["offset"] = static_cast<std::uint64_t>(offset);
    field["size"] = static_cast<std::uint64_t>(size);
    field["type"] = type;
    return field;
}

Json correlation_layout() {
    Json layout = Json::array();
    layout.push_back(field_layout("callback_key_1_raw", 0, 4, "u32-le"));
    layout.push_back(field_layout("callback_key_2_raw", 4, 4, "u32-le"));
    layout.push_back(field_layout("market_raw", 8, 2, "u16-le"));
    layout.push_back(field_layout("code_raw", 10, 6, "char[6]"));
    layout.push_back(field_layout("reserved_zero", 16, 1, "u8"));
    layout.push_back(field_layout("data_type", 17, 4, "u32-le"));
    layout.push_back(field_layout("registry_mode_raw", 21, 4, "u32-le"));
    return layout;
}

}  // namespace

Json decode_level2_sdk_correlation_registry(const Bytes& payload, int limit) {
    if (limit < 0 || limit > 10000)
        throw Error("SDK correlation output limit must be 0..10000");
    if (payload.empty() || payload.size() % correlation_record_size != 0)
        throw Error("SDK correlation registry must be a non-empty exact "
                    "multiple of 25 bytes");
    const auto count = payload.size() / correlation_record_size;
    if (count > native_registry_record_limit)
        throw Error("SDK correlation registry exceeds the native 10001-record bound");

    const auto selected = std::min<std::size_t>(
        count, static_cast<std::size_t>(limit));
    Json records = Json::array();
    for (std::size_t index = 0; index < count; ++index) {
        const auto* record = payload.data() + index * correlation_record_size;
        const auto market = read_u16_le(record + 8);
        if (market > 2)
            throw Error("SDK correlation record " + std::to_string(index) +
                        " market_raw must be 0..2");
        if (record[16] != 0)
            throw Error("SDK correlation record " + std::to_string(index) +
                        " reserved byte at +16 must be zero");
        const auto code = correlation_code(record + 10, index);
        if (index >= selected) continue;

        const auto mode = read_u32_le(record + 21);
        Json item = Json::object();
        item["index"] = static_cast<std::uint64_t>(index);
        item["callback_key_1_raw"] =
            static_cast<std::uint64_t>(read_u32_le(record));
        item["callback_key_2_raw"] =
            static_cast<std::uint64_t>(read_u32_le(record + 4));
        item["market_raw"] = static_cast<std::uint64_t>(market);
        item["code_ascii"] = code;
        item["code_hex"] = bytes_hex(record + 10, 6);
        item["reserved_zero"] = 0;
        item["data_type"] =
            static_cast<std::uint64_t>(read_u32_le(record + 17));
        item["registry_mode_raw"] = static_cast<std::uint64_t>(mode);
        item["callback_policy"] = mode == 9
            ? "retained-after-matching-callback"
            : "removed-after-matching-callback";
        records.push_back(std::move(item));
    }

    Json lookup = Json::object();
    lookup["match_fields"] = Json::array();
    lookup["match_fields"].push_back("callback_key_1_raw");
    lookup["match_fields"].push_back("callback_key_2_raw");
    lookup["match_fields"].push_back("data_type");
    lookup["identity_fields"] = Json::array();
    lookup["identity_fields"].push_back("market_raw");
    lookup["identity_fields"].push_back("code_ascii");
    lookup["mode_9_policy"] = "retained-after-matching-callback";
    lookup["other_mode_policy"] = "removed-after-matching-callback";

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-correlation-registry-v1";
    result["format"] = "sdk-correlation";
    result["record_scope"] =
        "TdxW in-process SDK callback correlation registry";
    result["record_size"] =
        static_cast<std::uint64_t>(correlation_record_size);
    result["record_count"] = static_cast<std::uint64_t>(count);
    result["native_record_bound"] =
        static_cast<std::uint64_t>(native_registry_record_limit);
    result["records"] = std::move(records);
    result["truncated"] = selected < count;
    result["batch_semantics"] = "exact-record-array";
    result["layout"] = correlation_layout();
    result["callback_lookup"] = std::move(lookup);
    result["key_semantics"] =
        "opaque callback correlation keys; not identified as a session or token";
    result["network_payload"] = false;
    result["session_record"] = false;
    result["token_record"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
    result["evidence"] =
        "TdxW sub_68C510 registry writer and sub_68C750 callback lookup";
    return result;
}

}  // namespace tdx
