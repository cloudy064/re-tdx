#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace tdx {
namespace {

constexpr std::size_t security_record_size = 7;
constexpr std::size_t local_fallback_record_size = 11;
constexpr std::size_t sdk_1807_batch_limit = 100;

std::string bytes_hex(const std::uint8_t* data, std::size_t size) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index)
        output << std::setw(2) << static_cast<unsigned>(data[index]);
    return output.str();
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

Json packed_security_layout() {
    Json layout = Json::array();
    layout.push_back(field_layout("market_raw", 0, 1, "u8"));
    layout.push_back(field_layout("code_raw", 1, 6, "char[6]"));
    return layout;
}

Json local_record_layout() {
    Json layout = packed_security_layout();
    layout.push_back(field_layout("local_context_key_raw", 7, 4, "u32-le"));
    return layout;
}

Json display_code(const std::uint8_t* code) {
    const bool printable = std::all_of(
        code, code + 6, [](std::uint8_t value) { return value >= 0x20 && value <= 0x7e; });
    if (!printable) return Json(nullptr);
    return Json(std::string(reinterpret_cast<const char*>(code), 6));
}

}  // namespace

Json build_level2_sdk_1807_request_plan(const Bytes& packed_securities) {
    if (packed_securities.empty())
        throw Error("SDK 1807 request plan requires at least one 7-byte security record");
    if (packed_securities.size() % security_record_size != 0)
        throw Error("SDK 1807 request plan input must be an exact multiple of 7 bytes");
    const auto count = packed_securities.size() / security_record_size;
    if (count > sdk_1807_batch_limit)
        throw Error("SDK 1807 request plan accepts at most 100 security records");

    Json items = Json::array();
    Json unresolved = Json::array();
    for (std::size_t index = 0; index < count; ++index) {
        const auto* record = packed_securities.data() + index * security_record_size;
        Json item = Json::object();
        item["index"] = static_cast<std::uint64_t>(index);
        item["market_raw"] = static_cast<std::uint64_t>(record[0]);
        item["code_ascii"] = display_code(record + 1);
        item["code_hex"] = bytes_hex(record + 1, 6);
        item["packed_hex"] = bytes_hex(record, security_record_size);
        item["route"] = "needs-resolution";
        item["sdk_symbol"] = Json(nullptr);
        item["resolved_market_raw"] = Json(nullptr);
        item["special_local_only"] = Json(nullptr);
        item["reason"] =
            "TdxW sub_401AB0 and sub_594580 require the live in-process security table";
        items.push_back(std::move(item));

        Json missing = Json::object();
        missing["item_index"] = static_cast<std::uint64_t>(index);
        missing["kind"] = "security-resolution";
        missing["status"] = "needs-resolution";
        missing["reason"] =
            "the offline tool cannot invoke TdxW security resolvers";
        unresolved.push_back(std::move(missing));
    }

    Json input = Json::object();
    input["encoding"] = "packed-security-records";
    input["record_size"] = static_cast<std::uint64_t>(security_record_size);
    input["record_count"] = static_cast<std::uint64_t>(count);
    input["batch_limit"] = static_cast<std::uint64_t>(sdk_1807_batch_limit);
    input["layout"] = packed_security_layout();

    Json sdk_arguments = Json::object();
    sdk_arguments["symbol_list"] = Json(nullptr);
    sdk_arguments["symbol_delimiter"] = ",";
    sdk_arguments["symbol_count"] = 0;
    sdk_arguments["data_type"] = 1807;
    sdk_arguments["reserved"] = 0;

    Json sdk_call = Json::object();
    sdk_call["api"] = "fnSubscribeData";
    sdk_call["ready"] = false;
    sdk_call["arguments"] = std::move(sdk_arguments);
    sdk_call["blocked_by"] = "security-resolution";
    sdk_call["note"] =
        "the symbol list is formed only after TdxW resolves each security and chooses SDK versus local fallback";

    Json callback_correlation = Json::object();
    callback_correlation["registry_mode_raw"] = 9;
    callback_correlation["retained_on_callback"] = true;
    callback_correlation["wire_argument"] = false;
    callback_correlation["note"] =
        "mode 9 belongs to the local request/callback registry, not to an SDK wire body";

    Json local_fallback = Json::object();
    local_fallback["ready"] = false;
    local_fallback["record_size"] =
        static_cast<std::uint64_t>(local_fallback_record_size);
    local_fallback["count"] = 0;
    local_fallback["items"] = Json::array();
    local_fallback["layout"] = local_record_layout();
    local_fallback["context_key_policy"] =
        "copied from the resolved local quote state; some native callers force it to zero";

    Json resolution = Json::object();
    resolution["required"] = true;
    resolution["resolver"] = "TdxW sub_401AB0 plus sub_594580";
    resolution["sdk_symbol_rules"] =
        "resolved market 0 becomes SZ+code; other SDK-routable markets become SH+code";
    resolution["local_fallback_rules"] =
        "sub_594580 special securities and resolved market 2 use the 11-byte local path";

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-1807-request-plan-v1";
    result["format"] = "sdk-1807-plan";
    result["plan_kind"] = "sdk-subscription-plan";
    result["status"] = "needs-resolution";
    result["data_type"] = 1807;
    result["input"] = std::move(input);
    result["items"] = std::move(items);
    result["needs_resolution_count"] = static_cast<std::uint64_t>(count);
    result["unresolved"] = std::move(unresolved);
    result["resolution"] = std::move(resolution);
    result["sdk_call"] = std::move(sdk_call);
    result["callback_correlation"] = std::move(callback_correlation);
    result["local_fallback"] = std::move(local_fallback);
    result["network_request_bytes_built"] = false;
    result["input_records_are_network_request_bytes"] = false;
    result["offline"] = true;
    result["subscription_sent"] = false;
    result["entitlement_bypass"] = false;
    result["evidence"] =
        "TdxW sub_69CEF0 -> sub_68C320; packed identity 7 bytes, data type 1807, registry mode 9";
    return result;
}

}  // namespace tdx
