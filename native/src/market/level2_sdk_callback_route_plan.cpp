#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace tdx {
namespace {

struct CallbackTemplate {
    int data_type;
    const char* name;
};

CallbackTemplate callback_template(Level2SdkCallbackDataType data_type) {
    switch (data_type) {
    case Level2SdkCallbackDataType::transaction:
        return {1801, "transaction"};
    case Level2SdkCallbackDataType::order:
        return {1802, "order"};
    case Level2SdkCallbackDataType::multi_level_quote:
        return {1803, "multi-level-quote"};
    case Level2SdkCallbackDataType::price_queue:
        return {1804, "price-queue"};
    case Level2SdkCallbackDataType::quote_update:
        return {1807, "quote-update"};
    case Level2SdkCallbackDataType::order_queue_at_price:
        return {18031, "order-queue-at-price"};
    case Level2SdkCallbackDataType::extended_quote:
        return {18071, "extended-quote"};
    }
    throw Error(
        "SDK callback route data-type must be 1801, 1802, 1803, 1804, "
        "1807, 18031, or 18071");
}

std::string u32_hex(std::uint32_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << std::setfill('0') << std::setw(8)
           << value;
    return output.str();
}

Json string_array(std::initializer_list<const char*> values) {
    Json result = Json::array();
    for (const auto* value : values) result.push_back(value);
    return result;
}

Json raw_parameter(const char* source, std::uint32_t value) {
    Json parameter = Json::object();
    parameter["source"] = source;
    parameter["raw_value"] = static_cast<std::uint64_t>(value);
    parameter["raw_hex"] = u32_hex(value);
    parameter["resolved"] = true;
    return parameter;
}

Json unresolved_security_parameter() {
    Json parameter = Json::object();
    parameter["source"] = "resolved-host-security-reference-raw";
    parameter["raw_value"] = Json(nullptr);
    parameter["resolved"] = false;
    parameter["blocked_by"] = "live-host-security-correlation";
    return parameter;
}

Json recipient_filter(bool secondary) {
    Json filter = Json::object();
    filter["kind"] = "host-runtime-class-filter";
    filter["runtime_class_semantics_resolved"] = false;
    filter["window_handle"] = Json(nullptr);
    filter["window_handle_resolved"] = false;
    filter["ida_runtime_class_predicates"] = secondary
        ? string_array({"sub_6B8560", "sub_46FFC0", "sub_48ED20"})
        : string_array({"sub_46FFC0", "sub_48ED20"});
    if (secondary) {
        filter["additional_ida_predicate"] = "sub_7E44F0";
        filter["additional_predicate_semantics_resolved"] = false;
    }
    return filter;
}

Json route(std::uint32_t message_id, const char* message_id_hex,
           const char* api, Json conditions, Json wparam, Json lparam,
           bool secondary_recipient = false) {
    Json result = Json::object();
    result["condition"] = std::move(conditions);
    result["message_id_raw"] = static_cast<std::uint64_t>(message_id);
    result["message_id_hex"] = message_id_hex;
    result["api"] = api;
    result["delivery"] = std::string(api) == "SendMessageA"
        ? "synchronous" : "asynchronous";
    result["wparam"] = std::move(wparam);
    result["lparam"] = std::move(lparam);
    result["recipient_filter"] = recipient_filter(secondary_recipient);
    result["dispatch_attempted"] = false;
    result["messages_sent"] = 0;
    return result;
}

Json common_conditions() {
    return string_array({
        "callback-correlation-resolved",
        "host-security-state-resolved",
        "sub_597310-host-ui-gate",
        "eligible-host-runtime-class"});
}

Json ordinary_route(const CallbackTemplate& value,
                    std::optional<std::uint32_t> registry_mode_raw,
                    bool include_time_condition) {
    auto conditions = common_conditions();
    if (include_time_condition)
        conditions.push_back("host-time-advanced=false");

    std::uint32_t message_id = 0x54d;
    const char* message_id_hex = "0x054d";
    const char* api = "PostMessageA";
    Json lparam = raw_parameter("callback-data-type", value.data_type);
    switch (value.data_type) {
    case 1801:
        message_id = 0x54e;
        message_id_hex = "0x054e";
        api = *registry_mode_raw == 9 ? "SendMessageA" : "PostMessageA";
        lparam = raw_parameter("registry-mode-raw", *registry_mode_raw);
        break;
    case 1802:
        message_id = 0x54f;
        message_id_hex = "0x054f";
        api = *registry_mode_raw == 9 ? "SendMessageA" : "PostMessageA";
        lparam = raw_parameter("registry-mode-raw", *registry_mode_raw);
        break;
    case 1803:
        message_id = 0x551;
        message_id_hex = "0x0551";
        lparam = raw_parameter("registry-mode-raw", *registry_mode_raw);
        break;
    case 18031:
        message_id = 0x551;
        message_id_hex = "0x0551";
        lparam = raw_parameter("fixed-host-route-value", 2);
        break;
    case 1804:
    case 1807:
    case 18071:
        break;
    default:
        throw Error("unsupported SDK callback route template");
    }
    return route(message_id, message_id_hex, api, std::move(conditions),
                 unresolved_security_parameter(), std::move(lparam));
}

Json special_1807_route() {
    auto conditions = common_conditions();
    conditions.push_back("host-time-advanced=true");
    return route(0x8b9, "0x08b9", "SendMessageA", std::move(conditions),
                 unresolved_security_parameter(),
                 raw_parameter("fixed-zero", 0));
}

Json secondary_1807_route() {
    auto conditions = common_conditions();
    conditions.push_back("host-time-advanced=true");
    conditions.push_back("at-least-one-0x08b9-recipient-matched");
    conditions.push_back("sub_7E44F0-recipient-predicate");
    return route(0x91e, "0x091e", "SendMessageA", std::move(conditions),
                 raw_parameter("fixed-zero", 0),
                 raw_parameter("fixed-zero", 0), true);
}

Json correlation_plan(const CallbackTemplate& value,
                      std::optional<std::uint32_t> registry_mode_raw) {
    Json correlation = Json::object();
    correlation["resolved"] = false;
    correlation["security_handle_raw"] = Json(nullptr);
    correlation["key_semantics"] =
        "host callback correlation only; not a session, token, or network request id";
    if (value.data_type == 18031) {
        correlation["kind"] = "host-global-correlation";
        correlation["source"] =
            "TdxW host global set by the 18031 request path";
        correlation["uses_25_byte_local_registry"] = false;
        correlation["registry_mode_raw"] = Json(nullptr);
        correlation["record_retention"] = "not-applicable";
        correlation["lookup_fields"] =
            string_array({"host_global_correlation_raw"});
        return correlation;
    }

    const auto mode = *registry_mode_raw;
    correlation["kind"] = "25-byte-local-registry";
    correlation["uses_25_byte_local_registry"] = true;
    correlation["registry_mode_raw"] = static_cast<std::uint64_t>(mode);
    correlation["registry_mode_hex"] = u32_hex(mode);
    correlation["lookup_fields"] = string_array({
        "callback_key_1_raw", "callback_key_2_raw", "data_type"});
    correlation["record_retention"] = mode == 9
        ? "retained-after-matching-callback"
        : "removed-before-body-consumption";
    correlation["retained_after_matching_callback"] = mode == 9;
    return correlation;
}

}  // namespace

Json build_level2_sdk_callback_route_plan(
    const Level2SdkCallbackRoutePlanRequest& request) {
    const auto value = callback_template(request.data_type);
    const bool global_correlation = value.data_type == 18031;
    if (global_correlation && request.registry_mode_raw)
        throw Error("SDK callback route 18031 does not use registry-mode-raw");
    if (!global_correlation && !request.registry_mode_raw)
        throw Error("SDK callback route requires registry-mode-raw except for 18031");
    if (value.data_type != 1807 && request.host_time_advanced)
        throw Error("host-time-advanced is only valid for SDK callback route 1807");

    const bool time_branch_unknown = !request.host_time_advanced.has_value();
    const bool time_advanced = request.host_time_advanced.value_or(false);
    Json routes = Json::array();
    if (value.data_type != 1807 || time_branch_unknown || !time_advanced) {
        routes.push_back(ordinary_route(
            value, request.registry_mode_raw, value.data_type == 1807));
    }
    if (value.data_type == 1807 && (time_branch_unknown || time_advanced)) {
        routes.push_back(special_1807_route());
        routes.push_back(secondary_1807_route());
    }

    Json input = Json::object();
    input["data_type"] = value.data_type;
    input["registry_mode_raw"] = request.registry_mode_raw
        ? Json(static_cast<std::uint64_t>(*request.registry_mode_raw))
        : Json(nullptr);
    input["host_time_advanced"] = request.host_time_advanced
        ? Json(*request.host_time_advanced) : Json(nullptr);

    Json time_branch = Json::object();
    time_branch["applicable"] = value.data_type == 1807;
    time_branch["resolved"] = request.host_time_advanced.has_value();
    time_branch["value"] = request.host_time_advanced
        ? Json(*request.host_time_advanced) : Json(nullptr);
    time_branch["semantics"] = value.data_type == 1807
        ? "sub_689E80(callback time) is greater than the prior host quote time"
        : "not-applicable";
    time_branch["conditional_alternatives_preserved"] =
        value.data_type == 1807 && !request.host_time_advanced;

    Json prerequisites = string_array({
        "callback-reached-sub_68C750",
        global_correlation ? "host-global-security-correlation-ready"
                           : "matching-25-byte-correlation-record",
        "host-security-state-resolved",
        "sub_597310-host-ui-gate",
        "eligible-host-window"});
    if (value.data_type == 1807)
        prerequisites.push_back("host-prior-quote-time-for-route-selection");

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-callback-route-plan-v1";
    result["format"] = "sdk-callback-route-plan";
    result["plan_kind"] = "tdxw-host-callback-dispatch-metadata";
    result["data_type"] = value.data_type;
    result["callback_kind"] = value.name;
    result["input"] = std::move(input);
    result["callback_correlation"] =
        correlation_plan(value, request.registry_mode_raw);
    result["host_time_advanced_branch"] = std::move(time_branch);
    result["host_prerequisites"] = std::move(prerequisites);
    result["routes"] = std::move(routes);
    result["route_count"] =
        static_cast<std::uint64_t>(result.at("routes").size());
    result["wparam_resolved"] = false;
    result["host_window_handles_resolved"] = false;
    result["sdk_callback_invoked"] = false;
    result["sdk_callback_executed"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["wire_bytes_built"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
    result["transport_scope"] =
        "TdxW in-process host UI routing metadata; not SDK or network wire";
    result["evidence"] =
        "TdxW sub_68C750 correlation lookup, callback body dispatch, and host notification branches";
    return result;
}

}  // namespace tdx
