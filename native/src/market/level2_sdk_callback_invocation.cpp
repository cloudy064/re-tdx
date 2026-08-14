#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t maximum_callback_body_size = level2_offline_payload_limit;

struct CallbackBodyContract {
    int data_type;
    const char* callback_kind;
    const char* decoder_format;
    const char* decoded_schema;
    std::size_t body_unit_size;
    bool counted_record_array;
};

CallbackBodyContract callback_body_contract(
    Level2SdkCallbackDataType data_type) {
    switch (data_type) {
    case Level2SdkCallbackDataType::transaction:
        return {1801, "transaction", "sdk-1801",
                "tdx-level2-sdk-1801-v1", 52, true};
    case Level2SdkCallbackDataType::order:
        return {1802, "order", "sdk-1802",
                "tdx-level2-sdk-1802-v1", 40, true};
    case Level2SdkCallbackDataType::multi_level_quote:
        return {1803, "multi-level-quote", "sdk-1803",
                "tdx-level2-sdk-1803-v1", 32016, false};
    case Level2SdkCallbackDataType::price_queue:
        return {1804, "price-queue", "sdk-1804",
                "tdx-level2-sdk-1804-v1", 432, false};
    case Level2SdkCallbackDataType::quote_update:
        return {1807, "quote-update", "sdk-1807",
                "tdx-level2-sdk-1807-v1", 380, false};
    case Level2SdkCallbackDataType::order_queue_at_price:
        return {18031, "order-queue-at-price", "sdk-18031",
                "tdx-level2-sdk-18031-v1", 20012, false};
    case Level2SdkCallbackDataType::extended_quote:
        return {18071, "extended-quote", "sdk-18071",
                "tdx-level2-sdk-18071-v1", 380, false};
    }
    throw Error(
        "SDK callback invocation data-type must be 1801, 1802, 1803, "
        "1804, 1807, 18031, or 18071");
}

std::size_t validate_callback_body(
    const Level2SdkCallbackInvocationRequest& request,
    const CallbackBodyContract& contract) {
    if (request.body.size() > maximum_callback_body_size)
        throw Error("SDK callback invocation body exceeds the 384 KiB offline safety limit");
    if (!contract.counted_record_array) {
        if (request.body.size() != contract.body_unit_size)
            throw Error("SDK callback invocation " +
                        std::to_string(contract.data_type) +
                        " requires exactly " +
                        std::to_string(contract.body_unit_size) +
                        " body bytes");
        return 1;
    }

    if (request.callback_arg5_raw <= 0)
        throw Error("SDK callback invocation " +
                    std::to_string(contract.data_type) +
                    " requires callback-arg5-raw to be a positive record count");
    const auto count =
        static_cast<std::size_t>(request.callback_arg5_raw);
    const auto maximum_record_count =
        maximum_callback_body_size / contract.body_unit_size;
    if (count > maximum_record_count)
        throw Error("SDK callback invocation " +
                    std::to_string(contract.data_type) +
                    " arg5 record count exceeds the 384 KiB offline safety limit");
    if (count > std::numeric_limits<std::size_t>::max() /
                    contract.body_unit_size)
        throw Error("SDK callback invocation body-size calculation overflow");
    const auto expected_size = count * contract.body_unit_size;
    if (request.body.size() != expected_size)
        throw Error("SDK callback invocation " +
                    std::to_string(contract.data_type) + " arg5 count requires " +
                    std::to_string(expected_size) + " body bytes, got " +
                    std::to_string(request.body.size()));
    return count;
}

Json decoder_reference(const CallbackBodyContract& contract) {
    Json result = Json::object();
    result["public_api"] = "decode_level2_document";
    result["format"] = contract.decoder_format;
    result["schema"] = contract.decoded_schema;
    result["existing_decoder_reused"] = true;
    result["parser_logic_duplicated"] = false;
    return result;
}

}  // namespace

Json decode_level2_sdk_callback_invocation(
    const Level2SdkCallbackInvocationRequest& request, int limit) {
    const auto contract = callback_body_contract(request.data_type);
    const bool global_correlation = contract.data_type == 18031;
    if (global_correlation && request.registry_mode_raw)
        throw Error(
            "SDK callback invocation 18031 uses host-global correlation and "
            "does not accept registry-mode-raw");
    if (!global_correlation && !request.registry_mode_raw)
        throw Error(
            "SDK callback invocation requires registry-mode-raw except for 18031");

    const auto record_count = validate_callback_body(request, contract);
    auto decoded = decode_level2_document(
        contract.decoder_format, request.body, limit);

    Level2SdkCallbackRoutePlanRequest route_request;
    route_request.data_type = request.data_type;
    route_request.registry_mode_raw = request.registry_mode_raw;
    // Invocation bytes alone do not resolve TdxW's live 1807 host-time branch.
    route_request.host_time_advanced = std::nullopt;
    auto route_plan = build_level2_sdk_callback_route_plan(route_request);

    Json callback_arguments = Json::object();
    callback_arguments["data_type_raw"] = contract.data_type;
    callback_arguments["callback_arg5_raw"] =
        static_cast<std::int64_t>(request.callback_arg5_raw);
    callback_arguments["callback_arg5_semantics"] =
        contract.counted_record_array
            ? Json("record-count-for-this-callback-type")
            : Json(nullptr);
    callback_arguments["callback_arg5_consumed_as_record_count"] =
        contract.counted_record_array;
    callback_arguments["callback_arg6_raw"] =
        static_cast<std::uint64_t>(request.callback_arg6_raw);
    callback_arguments["callback_arg6_semantics"] = Json(nullptr);
    callback_arguments["callback_arg6_consumed_by_recovered_dispatcher"] =
        false;

    Json body_contract = Json::object();
    body_contract["kind"] = contract.counted_record_array
        ? "counted-record-array" : "single-fixed-body";
    body_contract["body_unit_size"] =
        static_cast<std::uint64_t>(contract.body_unit_size);
    body_contract["body_size"] =
        static_cast<std::uint64_t>(request.body.size());
    body_contract["expected_body_size"] =
        static_cast<std::uint64_t>(record_count * contract.body_unit_size);
    body_contract["record_count"] =
        static_cast<std::uint64_t>(record_count);
    body_contract["exact_size_validated"] = true;
    body_contract["multiple_bodies_allowed"] =
        contract.counted_record_array;
    body_contract["maximum_input_body_size"] =
        static_cast<std::uint64_t>(maximum_callback_body_size);

    Json input = Json::object();
    input["data_type"] = contract.data_type;
    input["callback_arg5_raw"] =
        static_cast<std::int64_t>(request.callback_arg5_raw);
    input["callback_arg6_raw"] =
        static_cast<std::uint64_t>(request.callback_arg6_raw);
    input["registry_mode_raw"] = request.registry_mode_raw
        ? Json(static_cast<std::uint64_t>(*request.registry_mode_raw))
        : Json(nullptr);
    input["body_size"] =
        static_cast<std::uint64_t>(request.body.size());

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-callback-invocation-v1";
    result["format"] = "sdk-callback-invocation";
    result["normalization_kind"] =
        "offline-sub_68C750-callback-envelope";
    result["data_type"] = contract.data_type;
    result["callback_kind"] = contract.callback_kind;
    result["record_count"] = static_cast<std::uint64_t>(record_count);
    result["body_size"] =
        static_cast<std::uint64_t>(request.body.size());
    result["input"] = std::move(input);
    result["callback_arguments"] = std::move(callback_arguments);
    result["body_contract"] = std::move(body_contract);
    result["callback_correlation"] =
        route_plan.at("callback_correlation");
    result["decoder_reference"] = decoder_reference(contract);
    result["decoded_document"] = std::move(decoded);
    result["route_plan"] = std::move(route_plan);
    result["input_body_retained"] = false;
    result["callback_executed"] = false;
    result["sdk_callback_invoked"] = false;
    result["sdk_callback_executed"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["sdk_called"] = false;
    result["wire_bytes_built"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
    result["transport_scope"] =
        "authorized callback body supplied offline; no callback, SDK, host "
        "message, or network operation";
    result["evidence"] =
        "TdxW sub_68C750 callback ABI and existing exact SDK body decoders";
    return result;
}

}  // namespace tdx
