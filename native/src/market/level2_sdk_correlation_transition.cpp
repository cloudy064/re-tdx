#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace tdx {
namespace {

constexpr std::size_t correlation_record_size = 25;
constexpr std::size_t maximum_replayed_registry_records = 10000;

int data_type_raw(Level2SdkCallbackDataType value) {
    switch (value) {
    case Level2SdkCallbackDataType::transaction: return 1801;
    case Level2SdkCallbackDataType::order: return 1802;
    case Level2SdkCallbackDataType::multi_level_quote: return 1803;
    case Level2SdkCallbackDataType::price_queue: return 1804;
    case Level2SdkCallbackDataType::quote_update: return 1807;
    case Level2SdkCallbackDataType::extended_quote: return 18071;
    case Level2SdkCallbackDataType::order_queue_at_price:
        break;
    }
    throw Error(
        "SDK correlation transition data_type must use the local registry: "
        "1801, 1802, 1803, 1804, 1807, or 18071");
}

void validate_code(const std::string& code, const std::string& path) {
    if (code.size() != 6 ||
        !std::all_of(code.begin(), code.end(), [](unsigned char value) {
            return value >= '0' && value <= '9';
        }))
        throw Error(path + " must contain exactly six ASCII digits");
}

void validate_record(const Level2SdkCorrelationRecord& record,
                     const std::string& path) {
    if (record.market_raw > 2)
        throw Error(path + ".market_raw must be 0..2");
    validate_code(record.code_ascii, path + ".code_ascii");
    (void)data_type_raw(record.data_type);
}

Json record_json(const Level2SdkCorrelationRecord& record) {
    Json result = Json::object();
    result["callback_key_1_raw"] =
        static_cast<std::uint64_t>(record.callback_key_1_raw);
    result["callback_key_2_raw"] =
        static_cast<std::uint64_t>(record.callback_key_2_raw);
    result["market_raw"] = static_cast<std::uint64_t>(record.market_raw);
    result["code_ascii"] = record.code_ascii;
    result["reserved_zero"] = 0;
    result["data_type"] = data_type_raw(record.data_type);
    result["registry_mode_raw"] =
        static_cast<std::uint64_t>(record.registry_mode_raw);
    return result;
}

Json registry_json(const std::vector<Level2SdkCorrelationRecord>& records) {
    Json items = Json::array();
    for (const auto& record : records) items.push_back(record_json(record));

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-correlation-registry-state-v1";
    result["record_size"] =
        static_cast<std::uint64_t>(correlation_record_size);
    result["record_count"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(items);
    return result;
}

Json identity_json(const Level2SdkCorrelationRecord& record) {
    Json result = Json::object();
    result["market_raw"] = static_cast<std::uint64_t>(record.market_raw);
    result["code_ascii"] = record.code_ascii;
    result["data_type"] = data_type_raw(record.data_type);
    result["registry_mode_raw"] =
        static_cast<std::uint64_t>(record.registry_mode_raw);
    return result;
}

bool callback_matches(const Level2SdkCorrelationRecord& record,
                      const Level2SdkCorrelationCallbackMatch& callback) {
    return data_type_raw(record.data_type) ==
               data_type_raw(callback.data_type) &&
           record.callback_key_1_raw == callback.callback_key_1_raw &&
           record.callback_key_2_raw == callback.callback_key_2_raw;
}

bool upsert_identity_matches(const Level2SdkCorrelationRecord& record,
                             const Level2SdkCorrelationRecord& upsert) {
    return data_type_raw(record.data_type) == data_type_raw(upsert.data_type) &&
           record.registry_mode_raw == upsert.registry_mode_raw &&
           record.market_raw == upsert.market_raw &&
           record.code_ascii == upsert.code_ascii;
}

Json string_array(std::initializer_list<const char*> values) {
    Json result = Json::array();
    for (const auto* value : values) result.push_back(value);
    return result;
}

void add_offline_boundary(Json& result) {
    result["key_semantics"] =
        "opaque SDK callback correlation keys; not sessions, tokens, or "
        "network request IDs";
    result["sdk_called"] = false;
    result["sdk_callback_invoked"] = false;
    result["callback_executed"] = false;
    result["host_security_lookup_attempted"] = false;
    result["host_security_resolved"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["wire_bytes_built"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["credentials_accessed"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
}

}  // namespace

Json project_level2_sdk_correlation_transition(
    const Level2SdkCorrelationTransitionRequest& request) {
    if (request.kind !=
            Level2SdkCorrelationTransitionKind::callback_match &&
        request.kind !=
            Level2SdkCorrelationTransitionKind::sdk_key_upsert)
        throw Error("SDK correlation transition kind is invalid");
    if (request.previous_registry.size() >
        maximum_replayed_registry_records)
        throw Error(
            "SDK correlation transition registry must contain at most 10000 "
            "records; the native cleanup path above that boundary is unresolved");
    for (std::size_t index = 0; index < request.previous_registry.size();
         ++index)
        validate_record(request.previous_registry[index],
                        "previous_registry.records[" +
                            std::to_string(index) + "]");

    const bool callback_operation =
        request.kind == Level2SdkCorrelationTransitionKind::callback_match;
    if (callback_operation != request.callback_match.has_value() ||
        callback_operation == request.sdk_key_upsert.has_value())
        throw Error(
            "SDK correlation transition requires exactly the action selected "
            "by kind");

    auto projected = request.previous_registry;
    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-correlation-transition-v1";
    result["format"] = "sdk-correlation-transition";
    result["normalization_kind"] =
        "offline-sub_68C510-sub_68C750-registry-transition";
    result["input_registry_record_count"] =
        static_cast<std::uint64_t>(projected.size());
    result["native_record_size"] =
        static_cast<std::uint64_t>(correlation_record_size);
    result["replay_record_limit"] =
        static_cast<std::uint64_t>(maximum_replayed_registry_records);

    if (callback_operation) {
        const auto& callback = *request.callback_match;
        (void)data_type_raw(callback.data_type);
        Json action = Json::object();
        action["data_type"] = data_type_raw(callback.data_type);
        action["callback_key_1_raw"] =
            static_cast<std::uint64_t>(callback.callback_key_1_raw);
        action["callback_key_2_raw"] =
            static_cast<std::uint64_t>(callback.callback_key_2_raw);
        result["action"] = std::move(action);
        const auto iterator = std::find_if(
            projected.begin(), projected.end(), [&](const auto& record) {
                return callback_matches(record, callback);
            });
        const bool matched = iterator != projected.end();
        result["operation"] = "callback-match";
        result["match_fields"] = string_array(
            {"data_type", "callback_key_1_raw", "callback_key_2_raw"});
        result["first_match_wins"] = true;
        result["matched"] = matched;
        result["matched_index"] = matched
            ? Json(static_cast<std::uint64_t>(
                  std::distance(projected.begin(), iterator)))
            : Json(nullptr);
        result["identity"] = matched ? identity_json(*iterator) : Json(nullptr);
        result["matched_registry_mode_raw"] = matched
            ? Json(static_cast<std::uint64_t>(iterator->registry_mode_raw))
            : Json(nullptr);

        const bool retained = matched && iterator->registry_mode_raw == 9;
        const bool removed = matched && !retained;
        result["retained_after_matching_callback"] =
            matched ? Json(retained) : Json(nullptr);
        result["removed_before_host_security_resolution"] =
            matched ? Json(removed) : Json(nullptr);
        result["state_change"] = !matched ? "no-match" :
            retained ? "matched-record-retained" :
                       "matched-record-removed";
        if (removed) projected.erase(iterator);
    } else {
        const auto& upsert = *request.sdk_key_upsert;
        validate_record(upsert, "sdk_key_upsert");
        result["action"] = record_json(upsert);
        const auto iterator = std::find_if(
            projected.begin(), projected.end(), [&](const auto& record) {
                return upsert_identity_matches(record, upsert);
            });
        const bool matched = iterator != projected.end();
        result["operation"] = "sdk-key-upsert";
        result["match_fields"] = string_array(
            {"data_type", "registry_mode_raw", "market_raw", "code_ascii"});
        result["first_match_wins"] = true;
        result["matched"] = matched;
        result["matched_index"] = matched
            ? Json(static_cast<std::uint64_t>(
                  std::distance(projected.begin(), iterator)))
            : Json(nullptr);
        result["identity"] = identity_json(upsert);
        result["retained_after_matching_callback"] = Json(nullptr);
        result["removed_before_host_security_resolution"] = Json(nullptr);
        if (matched) {
            iterator->callback_key_1_raw = upsert.callback_key_1_raw;
            iterator->callback_key_2_raw = upsert.callback_key_2_raw;
            result["state_change"] = "existing-record-keys-replaced";
        } else {
            if (projected.size() == maximum_replayed_registry_records)
                throw Error(
                    "SDK correlation transition upsert would create record "
                    "10001; the native cleanup path is unresolved");
            projected.push_back(upsert);
            result["state_change"] = "new-record-appended";
        }
    }

    result["projected_registry_record_count"] =
        static_cast<std::uint64_t>(projected.size());
    result["projected_registry"] = registry_json(projected);
    add_offline_boundary(result);
    result["evidence"] =
        "TdxW sub_68C510 registry upsert and sub_68C750 first-match callback "
        "consume/retain logic";
    return result;
}

}  // namespace tdx
