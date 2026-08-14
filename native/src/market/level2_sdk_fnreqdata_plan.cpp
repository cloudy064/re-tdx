#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tdx {
namespace {

constexpr std::uint16_t detail_request_limit = 1500;
constexpr std::size_t abi_slot_count = 12;
constexpr std::size_t correlation_record_size = 25;

struct FnReqDataTemplate {
    int data_type;
    const char* name;
    bool variable_window;
};

FnReqDataTemplate request_template(Level2SdkFnReqDataType data_type) {
    switch (data_type) {
    case Level2SdkFnReqDataType::transaction:
        return {1801, "transaction", true};
    case Level2SdkFnReqDataType::order:
        return {1802, "order", true};
    case Level2SdkFnReqDataType::multi_level_quote:
        return {1803, "multi-level-quote", false};
    case Level2SdkFnReqDataType::price_queue:
        return {1804, "price-queue", false};
    case Level2SdkFnReqDataType::extended_quote:
        return {18071, "extended-quote", false};
    }
    throw Error("fnReqData plan data type must be 1801, 1802, 1803, 1804, or 18071");
}

bool six_digit_code(const std::string& code) {
    return code.size() == 6 &&
           std::all_of(code.begin(), code.end(), [](unsigned char ch) {
               return ch >= '0' && ch <= '9';
           });
}

const char* market_prefix(std::uint16_t market_id) {
    switch (market_id) {
    case 0: return "SZ";
    case 1: return "SH";
    case 2: return "BJ";
    }
    throw Error("fnReqData plan market-id must be 0, 1, or 2");
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

Json scalar_slot(std::size_t index, const char* name, const char* abi_type,
                 std::uint32_t raw_value) {
    Json slot = Json::object();
    slot["index"] = static_cast<std::uint64_t>(index);
    slot["name"] = name;
    slot["abi_type"] = abi_type;
    slot["raw_value"] = static_cast<std::uint64_t>(raw_value);
    slot["resolved"] = true;
    return slot;
}

Json host_pointer_slot(std::size_t index, const char* name,
                       const char* abi_type, const char* blocked_by) {
    Json slot = Json::object();
    slot["index"] = static_cast<std::uint64_t>(index);
    slot["name"] = name;
    slot["abi_type"] = abi_type;
    slot["raw_value"] = Json(nullptr);
    slot["resolved"] = false;
    slot["blocked_by"] = blocked_by;
    return slot;
}

Json logical_pointer_slot(std::size_t index, const char* name,
                          const char* logical_value) {
    Json slot = host_pointer_slot(index, name, "pointer-u32", "runtime-storage");
    slot["logical_value"] = logical_value;
    slot["materialization"] = "caller-owned ASCII storage";
    return slot;
}

}  // namespace

Json build_level2_sdk_fnreqdata_call_plan(
    const Level2SdkFnReqDataCallPlanRequest& request) {
    const auto plan_template = request_template(request.data_type);
    const auto* prefix = market_prefix(request.market_id);
    if (!six_digit_code(request.code))
        throw Error("fnReqData plan code must contain exactly six ASCII digits");

    std::uint32_t cursor_raw = 0;
    std::uint16_t request_count = 1;
    if (plan_template.variable_window) {
        if (!request.cursor_raw || !request.request_count)
            throw Error("fnReqData 1801/1802 plans require explicit cursor-raw and request-count");
        if (*request.request_count == 0 ||
            *request.request_count > detail_request_limit)
            throw Error("fnReqData 1801/1802 request-count must be 1..1500");
        cursor_raw = *request.cursor_raw;
        request_count = *request.request_count;
    } else if (request.cursor_raw || request.request_count) {
        throw Error("fnReqData 1803/1804/18071 plans use fixed cursor 0 and request-count 1");
    }
    const std::uint32_t registry_mode_raw =
        plan_template.variable_window && cursor_raw != 0 ? 1U : 0U;

    Json security = Json::object();
    security["market_id"] = static_cast<std::uint64_t>(request.market_id);
    security["market_prefix"] = prefix;
    security["code"] = request.code;

    Json logical_arguments = Json::object();
    logical_arguments["selector_raw"] = 0;
    logical_arguments["selected_price_f32_input"] = 0.0;
    logical_arguments["selected_price_f64_abi"] = 0.0;
    logical_arguments["data_type"] = plan_template.data_type;
    logical_arguments["reserved_zero_before_cursor"] = 0;
    logical_arguments["cursor_raw"] = static_cast<std::uint64_t>(cursor_raw);
    logical_arguments["request_count"] =
        static_cast<std::uint64_t>(request_count);
    logical_arguments["reserved_zero_after_count"] = 0;

    Json slots = Json::array();
    slots.push_back(host_pointer_slot(
        0, "owner_window_raw", "host-hwnd-u32", "live-owner-window"));
    slots.push_back(host_pointer_slot(
        1, "callback_key_output_raw", "out-pointer-u32",
        "runtime-callback-key-storage"));
    slots.push_back(logical_pointer_slot(2, "market_prefix_raw", prefix));
    slots.push_back(logical_pointer_slot(3, "code_raw", request.code.c_str()));
    slots.push_back(scalar_slot(4, "selector_raw", "i32", 0));
    slots.push_back(scalar_slot(
        5, "selected_price_f64_low_word", "u32", 0));
    slots.push_back(scalar_slot(
        6, "selected_price_f64_high_word", "u32", 0));
    slots.push_back(scalar_slot(
        7, "data_type", "i32",
        static_cast<std::uint32_t>(plan_template.data_type)));
    slots.push_back(scalar_slot(8, "reserved_zero_before_cursor", "u32", 0));
    slots.push_back(scalar_slot(9, "cursor_raw", "u32", cursor_raw));
    slots.push_back(scalar_slot(
        10, "request_count", "u32",
        static_cast<std::uint32_t>(request_count)));
    slots.push_back(scalar_slot(11, "reserved_zero_after_count", "u32", 0));

    Json correlation = Json::object();
    correlation["kind"] = "25-byte-local-registry";
    correlation["record_size"] =
        static_cast<std::uint64_t>(correlation_record_size);
    correlation["layout"] = correlation_layout();
    correlation["callback_key_1_raw"] = Json(nullptr);
    correlation["callback_key_2_raw"] = Json(nullptr);
    correlation["market_raw"] =
        static_cast<std::uint64_t>(request.market_id);
    correlation["code_ascii"] = request.code;
    correlation["reserved_zero"] = 0;
    correlation["data_type"] = plan_template.data_type;
    correlation["registry_mode_raw"] =
        static_cast<std::uint64_t>(registry_mode_raw);
    correlation["key_semantics"] =
        "opaque callback correlation keys; not a session or token";
    correlation["ready"] = false;
    correlation["record_built"] = false;
    correlation["requires_sdk_output"] = true;

    Json blocked_by = Json::array();
    blocked_by.push_back("live-owner-window");
    blocked_by.push_back("loaded-fnReqData-export");
    blocked_by.push_back("runtime-callback-key-storage");

    Json sdk_call = Json::object();
    sdk_call["api"] = "fnReqData";
    sdk_call["raw_abi_word_count"] =
        static_cast<std::uint64_t>(abi_slot_count);
    sdk_call["raw_abi_word_size"] = 4;
    sdk_call["raw_abi_slots"] = std::move(slots);
    sdk_call["ready"] = false;
    sdk_call["invoked"] = false;
    sdk_call["blocked_by"] = std::move(blocked_by);

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-fnreqdata-call-plan-v1";
    result["format"] = "sdk-fnreqdata-plan";
    result["plan_kind"] = "sdk-export-logical-call";
    result["template"] = plan_template.name;
    result["data_type"] = plan_template.data_type;
    result["window_arguments"] =
        plan_template.variable_window ? "explicit" : "fixed-0-and-1";
    result["security"] = std::move(security);
    result["logical_arguments"] = std::move(logical_arguments);
    result["sdk_call"] = std::move(sdk_call);
    result["callback_correlation"] = std::move(correlation);
    result["security_resolution_required"] = false;
    result["abi_stack_bytes_built"] = false;
    result["wire_bytes_built"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
    result["evidence"] =
        "TdxW sub_68C510 fnReqData call and 25-byte callback registry; exact callers sub_69AE10/sub_69B0B0/sub_69B3F0/sub_697ED0/sub_69CE10";
    return result;
}

}  // namespace tdx
