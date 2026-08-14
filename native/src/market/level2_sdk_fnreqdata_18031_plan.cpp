#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr int data_type = 18031;
constexpr std::uint32_t sdk_cursor_raw = 0;
constexpr std::uint32_t sdk_request_count = 1;
constexpr std::uint32_t registry_mode_raw = 2;
constexpr std::size_t abi_slot_count = 12;

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "18031 plan requires IEEE-754 binary32 float");
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "18031 plan requires IEEE-754 binary64 double");

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
    throw Error("fnReqData 18031 plan market-id must be 0, 1, or 2");
}

std::uint32_t f32_bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

std::uint64_t f64_bits(double value) {
    std::uint64_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

template <typename Value>
std::string fixed_hex(Value value) {
    std::ostringstream output;
    output << "0x" << std::hex << std::setfill('0')
           << std::setw(static_cast<int>(sizeof(Value) * 2)) << value;
    return output.str();
}

Json scalar_slot(std::size_t index, const char* name, const char* abi_type,
                 std::uint32_t raw_value) {
    Json slot = Json::object();
    slot["index"] = static_cast<std::uint64_t>(index);
    slot["name"] = name;
    slot["abi_type"] = abi_type;
    slot["raw_value"] = static_cast<std::uint64_t>(raw_value);
    slot["raw_hex"] = fixed_hex(raw_value);
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
                          const std::string& logical_value) {
    Json slot = host_pointer_slot(index, name, "pointer-u32", "runtime-storage");
    slot["logical_value"] = logical_value;
    slot["materialization"] = "caller-owned ASCII storage";
    return slot;
}

}  // namespace

Json build_level2_sdk_fnreqdata_18031_call_plan(
    const Level2SdkFnReqData18031CallPlanRequest& request) {
    const auto* prefix = market_prefix(request.market_id);
    if (!six_digit_code(request.code))
        throw Error("fnReqData 18031 plan code must contain exactly six ASCII digits");
    if (!std::isfinite(request.selected_price))
        throw Error("fnReqData 18031 selected-price must be a finite float");

    const std::uint32_t selector_raw = request.side_mode_raw == 1 ? 0U : 1U;
    const auto input_bits = f32_bits(request.selected_price);
    const double promoted_price = static_cast<double>(request.selected_price);
    if (!std::isfinite(promoted_price))
        throw Error("fnReqData 18031 selected-price cannot be represented as the SDK ABI double");
    const auto promoted_bits = f64_bits(promoted_price);
    const auto promoted_low = static_cast<std::uint32_t>(promoted_bits);
    const auto promoted_high = static_cast<std::uint32_t>(promoted_bits >> 32U);

    Json security = Json::object();
    security["market_id"] = static_cast<std::uint64_t>(request.market_id);
    security["market_prefix"] = prefix;
    security["code"] = request.code;

    Json logical_arguments = Json::object();
    logical_arguments["side_mode_raw"] =
        static_cast<std::uint64_t>(request.side_mode_raw);
    logical_arguments["sdk_selector_raw"] =
        static_cast<std::uint64_t>(selector_raw);
    logical_arguments["selected_price_f32_input"] = promoted_price;
    logical_arguments["selected_price_f32_bits_hex"] = fixed_hex(input_bits);
    logical_arguments["selected_price_f64_abi"] = promoted_price;
    logical_arguments["selected_price_f64_bits_hex"] = fixed_hex(promoted_bits);
    logical_arguments["data_type"] = data_type;
    logical_arguments["reserved_zero_before_cursor"] = 0;
    logical_arguments["cursor_raw"] =
        static_cast<std::uint64_t>(sdk_cursor_raw);
    logical_arguments["request_count"] =
        static_cast<std::uint64_t>(sdk_request_count);
    logical_arguments["reserved_zero_after_count"] = 0;
    logical_arguments["registry_mode_raw"] =
        static_cast<std::uint64_t>(registry_mode_raw);
    logical_arguments["registry_mode_is_sdk_abi_slot"] = false;

    Json slots = Json::array();
    slots.push_back(host_pointer_slot(
        0, "owner_window_raw", "host-hwnd-u32", "live-owner-window"));
    auto callback_output = host_pointer_slot(
        1, "callback_key_output_raw", "out-pointer-u32",
        "runtime-callback-key-storage");
    callback_output["pointee_size"] = 8;
    slots.push_back(std::move(callback_output));
    slots.push_back(logical_pointer_slot(2, "market_prefix_raw", prefix));
    slots.push_back(logical_pointer_slot(3, "code_raw", request.code));
    slots.push_back(scalar_slot(4, "selector_raw", "i32", selector_raw));
    slots.push_back(scalar_slot(
        5, "selected_price_f64_low_word", "u32", promoted_low));
    slots.push_back(scalar_slot(
        6, "selected_price_f64_high_word", "u32", promoted_high));
    slots.push_back(scalar_slot(7, "data_type", "i32", data_type));
    slots.push_back(scalar_slot(
        8, "reserved_zero_before_cursor", "u32", 0));
    slots.push_back(scalar_slot(9, "cursor_raw", "u32", sdk_cursor_raw));
    slots.push_back(scalar_slot(
        10, "request_count", "u32", sdk_request_count));
    slots.push_back(scalar_slot(
        11, "reserved_zero_after_count", "u32", 0));

    Json callback = Json::object();
    callback["kind"] = "host-global-correlation";
    callback["status"] = "needs-live-host-context";
    callback["key"] = Json(nullptr);
    callback["registry_mode_raw"] =
        static_cast<std::uint64_t>(registry_mode_raw);
    callback["uses_25_byte_local_registry"] = false;
    callback["local_registry_record_built"] = false;
    callback["ready"] = false;
    callback["requires_live_security_key"] = true;
    callback["key_source"] =
        "TdxW sub_525BB0(market_raw, live Source+27 security key)";
    callback["note"] =
        "data type 18031 is explicitly excluded from sub_68C510's 25-byte local registry";

    Json blocked_by = Json::array();
    blocked_by.push_back("live-owner-window");
    blocked_by.push_back("loaded-fnReqData-export");
    blocked_by.push_back("runtime-callback-key-storage");
    blocked_by.push_back("live-host-global-correlation");

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
    result["schema"] = "tdx-level2-sdk-fnreqdata-18031-call-plan-v1";
    result["format"] = "sdk-fnreqdata-18031-plan";
    result["plan_kind"] = "sdk-export-logical-call";
    result["template"] = "price-level-order-queue";
    result["data_type"] = data_type;
    result["security"] = std::move(security);
    result["side_mode_raw"] =
        static_cast<std::uint64_t>(request.side_mode_raw);
    result["sdk_selector_raw"] = static_cast<std::uint64_t>(selector_raw);
    result["cursor_raw"] = static_cast<std::uint64_t>(sdk_cursor_raw);
    result["request_count"] =
        static_cast<std::uint64_t>(sdk_request_count);
    result["registry_mode_raw"] =
        static_cast<std::uint64_t>(registry_mode_raw);
    result["logical_arguments"] = std::move(logical_arguments);
    result["sdk_call"] = std::move(sdk_call);
    result["callback_correlation"] = std::move(callback);
    result["security_resolution_required"] = false;
    result["abi_stack_bytes_built"] = false;
    result["wire_bytes_built"] = false;
    result["standalone_network_request"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["operation_executed"] = false;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
    result["evidence"] =
        "TdxW sub_69B800 -> sub_68C510(type 18031, selector side_mode_raw!=1, selected float price, cursor 0, count 1, registry mode 2); global correlation dword_D44E3C";
    return result;
}

}  // namespace tdx
