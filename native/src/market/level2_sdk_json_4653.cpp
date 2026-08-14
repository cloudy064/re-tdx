#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx {
namespace {

constexpr std::size_t maximum_document_size = level2_offline_payload_limit;
constexpr std::size_t maximum_record_count = 10000;
constexpr std::size_t native_header_size = 35;
constexpr std::size_t native_record_size = 18;
constexpr std::size_t optional_attachment_size = 120;
constexpr double price_scale_divisor = 1000.0;

struct NormalizedRecord {
    std::string datetime_raw;
    std::string datetime_right4_raw;
    std::uint16_t time_u16_raw{};
    float close_price{};
    float average_price{};
    std::uint32_t trade_volume_raw{};
    std::optional<float> reference_price;
};

const Json* member(const Json& object, std::string_view name) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

const Json& required_member(const Json& object, std::string_view name,
                            std::string_view path) {
    const auto* value = member(object, name);
    if (!value)
        throw Error(std::string(path) + " requires " + std::string(name));
    return *value;
}

void exact_object(const Json& value, std::string_view path,
                  std::initializer_list<std::string_view> allowed) {
    if (!value.is_object())
        throw Error(std::string(path) + " must be an object");
    for (const auto& [name, unused] : value.as_object()) {
        (void)unused;
        const std::string_view name_view(name.data(), name.size());
        if (std::find(allowed.begin(), allowed.end(), name_view) ==
            allowed.end())
            throw Error(std::string(path) + " has unknown field " + name);
    }
}

double strict_number(const Json& value, std::string_view path) {
    if (!value.is_number())
        throw Error(std::string(path) + " must be a JSON number");
    const auto result = value.as_number();
    if (!std::isfinite(result))
        throw Error(std::string(path) + " must be finite");
    return result;
}

float scaled_f32(const Json& value, std::string_view path) {
    const auto raw = strict_number(value, path);
    constexpr auto maximum =
        static_cast<double>(std::numeric_limits<float>::max());
    if (raw < -maximum || raw > maximum)
        throw Error(std::string(path) + " is outside the finite f32 range");
    const auto raw_f32 = static_cast<float>(raw);
    const auto scaled = raw_f32 / static_cast<float>(price_scale_divisor);
    if (!std::isfinite(scaled))
        throw Error(std::string(path) + " does not produce a finite scaled f32");
    return scaled;
}

std::uint32_t strict_u32(const Json& value, std::string_view path) {
    const auto number = strict_number(value, path);
    constexpr auto maximum =
        static_cast<double>(std::numeric_limits<std::uint32_t>::max());
    if (number < 0.0 || number > maximum || std::trunc(number) != number)
        throw Error(std::string(path) + " must be an integer in 0..4294967295");
    return static_cast<std::uint32_t>(number);
}

NormalizedRecord normalize_record(const Json& item, std::size_t index) {
    const auto path = "SDK 4653 Data[" + std::to_string(index) + "]";
    exact_object(item, path,
                 {"datetime", "closePrice", "averagePrice", "tradeVolume",
                  "reference_price"});

    const auto& datetime = required_member(item, "datetime", path);
    if (!datetime.is_string())
        throw Error(path + ".datetime must be a string");
    const auto& datetime_raw = datetime.as_string();
    if (datetime_raw.empty() ||
        !std::all_of(datetime_raw.begin(), datetime_raw.end(),
                     [](unsigned char value) {
                         return value >= '0' && value <= '9';
                     }))
        throw Error(path + ".datetime must be a non-empty ASCII digit string");

    const auto right4_size = std::min<std::size_t>(4, datetime_raw.size());
    const auto right4 = datetime_raw.substr(datetime_raw.size() - right4_size);
    unsigned parsed = 0;
    for (const auto digit : right4)
        parsed = parsed * 10U + static_cast<unsigned>(digit - '0');

    NormalizedRecord result;
    result.datetime_raw = datetime_raw;
    result.datetime_right4_raw = right4;
    result.time_u16_raw = static_cast<std::uint16_t>(61U * (parsed % 100U));
    result.close_price = scaled_f32(
        required_member(item, "closePrice", path), path + ".closePrice");
    result.average_price = scaled_f32(
        required_member(item, "averagePrice", path), path + ".averagePrice");
    result.trade_volume_raw = strict_u32(
        required_member(item, "tradeVolume", path), path + ".tradeVolume");

    if (const auto* reference = member(item, "reference_price"))
        result.reference_price = scaled_f32(
            *reference, path + ".reference_price");
    if (index == 0 && !result.reference_price)
        throw Error("SDK 4653 Data[0] requires reference_price");
    return result;
}

Json header_document(const std::vector<NormalizedRecord>& records) {
    Json header = Json::object();
    header["native_size"] = static_cast<std::uint64_t>(native_header_size);
    header["record_count_u16_raw"] =
        static_cast<std::uint64_t>(records.size());
    header["record_count_native_offset"] = 33;
    header["reference_price_native_offset"] = 29;
    header["reference_price_source_index"] =
        records.empty() ? Json(nullptr) : Json(std::uint64_t{0});
    header["reference_price"] =
        records.empty()
            ? Json(nullptr)
            : Json(static_cast<double>(*records.front().reference_price));
    header["request_context_fields_projected"] = false;
    header["cache_reference_fallback_projected"] = false;
    return header;
}

Json record_document(const NormalizedRecord& record, std::size_t index) {
    Json result = Json::object();
    result["index"] = static_cast<std::uint64_t>(index);
    result["source_index"] = static_cast<std::uint64_t>(index);
    result["native_offset"] = static_cast<std::uint64_t>(
        native_header_size + native_record_size * index);
    result["datetime_raw"] = record.datetime_raw;
    result["datetime_right4_raw"] = record.datetime_right4_raw;
    result["time_u16_raw"] = static_cast<std::uint64_t>(record.time_u16_raw);
    result["close_price"] = static_cast<double>(record.close_price);
    result["average_price"] = static_cast<double>(record.average_price);
    result["trade_volume_u32_raw"] =
        static_cast<std::uint64_t>(record.trade_volume_raw);
    return result;
}

void add_offline_boundary(Json& result) {
    result["input_retained"] = false;
    result["input_document_retained"] = false;
    result["file_retained"] = false;
    result["source_file_retained"] = false;
    result["sdk_called"] = false;
    result["sdk_callback_invoked"] = false;
    result["native_body_built"] = false;
    result["wire_bytes_built"] = false;
    result["network_request_bytes"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_built"] = false;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
}

}  // namespace

Json normalize_level2_sdk_json_4653(const Json& document, int limit) {
    if (limit < 0 || limit > static_cast<int>(maximum_record_count))
        throw Error("SDK 4653 limit must be 0..10000");
    exact_object(document, "SDK 4653 response", {"Data"});
    const auto& data = required_member(document, "Data", "SDK 4653 response");
    if (!data.is_array()) throw Error("SDK 4653 Data must be an array");
    if (data.size() > maximum_record_count)
        throw Error("SDK 4653 record count exceeds the offline safety limit");

    std::vector<NormalizedRecord> normalized;
    normalized.reserve(data.size());
    for (std::size_t index = 0; index < data.size(); ++index)
        normalized.push_back(normalize_record(data.as_array()[index], index));

    const auto compact_size = document.dump(-1).size();
    if (compact_size > maximum_document_size)
        throw Error("SDK 4653 compact JSON document exceeds 384 KiB");

    const auto returned = std::min<std::size_t>(
        normalized.size(), static_cast<std::size_t>(limit));
    Json records = Json::array();
    for (std::size_t index = 0; index < returned; ++index)
        records.push_back(record_document(normalized[index], index));

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-json-4653-v1";
    result["format"] = "sdk-json-4653";
    result["function_id"] = 4653;
    result["adapter"] = "tpbus ProtocolSZSDK2TDX::SetAnsData";
    result["evidence_function"] = "sub_10082805";
    result["input_scope"] = "authorized post-transport SDK JSON response";
    result["input_compact_size"] = static_cast<std::uint64_t>(compact_size);
    result["input_compact_size_limit"] =
        static_cast<std::uint64_t>(maximum_document_size);
    result["native_header_size"] =
        static_cast<std::uint64_t>(native_header_size);
    result["native_record_size"] =
        static_cast<std::uint64_t>(native_record_size);
    result["recovered_base_layout_size"] = static_cast<std::uint64_t>(
        native_header_size + native_record_size * normalized.size());
    result["optional_attachment_size"] =
        static_cast<std::uint64_t>(optional_attachment_size);
    result["optional_attachment_projected"] = false;
    result["source_order"] = "first-to-last";
    result["price_scale_divisor"] = price_scale_divisor;
    result["time_transform"] =
        "u16(61 * (atoi(CString::Right(datetime, 4)) % 100))";
    result["time_semantics_resolved"] = false;
    result["record_reserved_zero_bytes"] = 4;
    result["count"] = static_cast<std::uint64_t>(normalized.size());
    result["returned"] = static_cast<std::uint64_t>(returned);
    result["header"] = header_document(normalized);
    result["records"] = std::move(records);
    result["truncated"] = returned < normalized.size();
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
