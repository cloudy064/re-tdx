#include "server_level2_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/level2.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::server_detail {
namespace {

constexpr std::size_t maximum_project_request_bytes = level2_offline_payload_limit;
constexpr std::size_t quote_body_size = 380U;

const Json* project_member(const Json& body, std::string_view name) {
    const auto found = body.as_object().find(std::string(name));
    return found == body.as_object().end() ? nullptr : &found->second;
}

const Json& required_project_member(const Json& body, std::string_view name) {
    const auto* value = project_member(body, name);
    if (!value) throw Error(std::string(name) + " is required");
    return *value;
}

void require_project_fields(const Json& body,
                            const std::set<std::string>& allowed,
                            std::string_view label) {
    for (const auto& [name, value] : body.as_object()) {
        (void)value;
        if (!allowed.count(name))
            throw Error("Level2 " + std::string(label) +
                        " project does not accept field: " + name);
    }
}

std::string required_project_string(const Json& body,
                                    std::string_view name) {
    const auto& value = required_project_member(body, name);
    if (!value.is_string() || value.as_string().empty())
        throw Error(std::string(name) + " must be a non-empty string");
    return value.as_string();
}

int project_data_type(const Json& body) {
    const auto& value = required_project_member(body, "data_type");
    if (!value.is_number() || !std::isfinite(value.as_number()) ||
        std::floor(value.as_number()) != value.as_number() ||
        (value.as_number() != 1807.0 && value.as_number() != 18071.0))
        throw Error("data_type must be the integer 1807 or 18071");
    return static_cast<int>(value.as_number());
}

std::uint16_t project_market(const Json& body) {
    const auto& value = required_project_member(body, "market");
    if (!value.is_number() || !std::isfinite(value.as_number()) ||
        std::floor(value.as_number()) != value.as_number() ||
        value.as_number() < 0.0 || value.as_number() > 2.0)
        throw Error("market must be the integer 0, 1, or 2");
    return static_cast<std::uint16_t>(value.as_number());
}

std::string project_code(const Json& body) {
    const auto code = required_project_string(body, "code");
    if (code.size() != 6)
        throw Error("code must contain exactly 6 ASCII digits");
    for (const auto value : code) {
        if (value < '0' || value > '9')
            throw Error("code must contain exactly 6 ASCII digits");
    }
    return code;
}

int project_hex_digit(unsigned char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Bytes parse_project_inline_hex(const std::string& text,
                               std::string_view field = "payload_hex") {
    std::size_t digits = 0;
    for (const unsigned char value : text) {
        if (std::isspace(value)) continue;
        if (project_hex_digit(value) < 0)
            throw Error(std::string(field) +
                        " must contain hexadecimal digits and whitespace only");
        ++digits;
        if (digits > maximum_project_request_bytes * 2U)
            throw Error("Level2 project payload exceeds the 384 KiB API limit");
    }
    if (!digits) throw Error(std::string(field) + " is required");
    if (digits % 2U != 0)
        throw Error(std::string(field) +
                    " must contain an even number of hexadecimal digits");

    Bytes result;
    result.reserve(digits / 2U);
    int high = -1;
    for (const unsigned char value : text) {
        if (std::isspace(value)) continue;
        const int nibble = project_hex_digit(value);
        if (high < 0) high = nibble;
        else {
            result.push_back(static_cast<std::uint8_t>((high << 4) | nibble));
            high = -1;
        }
    }
    return result;
}

std::uint64_t project_previous_size(const Json& previous,
                                    std::string_view name) {
    const auto& value = required_project_member(previous, name);
    if (!value.is_number() || !std::isfinite(value.as_number()) ||
        std::floor(value.as_number()) != value.as_number() ||
        value.as_number() < 0.0 ||
        value.as_number() >
            static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        throw Error("previous." + std::string(name) +
                    " must be an integer in 0..INT32_MAX");
    return static_cast<std::uint64_t>(value.as_number());
}

std::string project_previous_string(const Json& previous,
                                    std::string_view name) {
    const auto& value = required_project_member(previous, name);
    if (!value.is_string())
        throw Error("previous." + std::string(name) + " must be a string");
    return value.as_string();
}

bool project_previous_bool(const Json& previous, std::string_view name) {
    const auto& value = required_project_member(previous, name);
    if (!value.is_bool())
        throw Error("previous." + std::string(name) + " must be a boolean");
    return value.as_bool();
}

Level2Sdk4651DualSnapshotPreviousState project_4651_previous(
    const Json& previous) {
    if (!previous.is_object()) throw Error("previous must be an object");
    static const std::set<std::string> allowed{
        "schema", "decoded_byte_size", "decoded_sha256", "raw_byte_size",
        "raw_sha256", "ready", "source", "metadata_complete",
        "bodies_emitted"};
    require_project_fields(previous, allowed, "SDK 4651 previous state");
    for (const auto& name : allowed)
        (void)required_project_member(previous, name);
    if (project_previous_string(previous, "schema") !=
        "tdx-level2-sdk-4651-dual-snapshot-state-v1")
        throw Error(
            "previous.schema must be "
            "tdx-level2-sdk-4651-dual-snapshot-state-v1");
    (void)project_previous_string(previous, "source");
    if (!project_previous_bool(previous, "metadata_complete"))
        throw Error("previous.metadata_complete must be true");
    if (project_previous_bool(previous, "bodies_emitted"))
        throw Error("previous.bodies_emitted must be false");

    Level2Sdk4651DualSnapshotPreviousState result;
    result.decoded_byte_size = project_previous_size(
        previous, "decoded_byte_size");
    result.decoded_sha256 = project_previous_string(
        previous, "decoded_sha256");
    result.raw_byte_size = project_previous_size(previous, "raw_byte_size");
    result.raw_sha256 = project_previous_string(previous, "raw_sha256");
    result.ready = project_previous_bool(previous, "ready");
    return result;
}

void add_project_transport_metadata(Json& result, std::size_t input_size) {
    result["input_encoding"] = "hex";
    result["input_size"] = static_cast<std::uint64_t>(input_size);
    result["input_size_limit"] =
        static_cast<std::uint64_t>(maximum_project_request_bytes);
    result["request_size_limit"] =
        static_cast<std::uint64_t>(maximum_project_request_bytes);
    result["input_retained"] = false;
    result["file_accessed"] = false;
    result["subscription_sent"] = false;
}

void add_project_dual_transport_metadata(Json& result,
                                         std::size_t first_size,
                                         std::size_t second_size,
                                         std::string_view first_name,
                                         std::string_view second_name) {
    add_project_transport_metadata(result, first_size + second_size);
    result[std::string(first_name) + "_input_size"] =
        static_cast<std::uint64_t>(first_size);
    result[std::string(second_name) + "_input_size"] =
        static_cast<std::uint64_t>(second_size);
}

}  // namespace

Json query_level2_project(const Json& raw_body) {
    if (!raw_body.is_object())
        throw Error("Level2 project request body must be a JSON object");
    if (raw_body.dump(-1).size() > maximum_project_request_bytes)
        throw Error("Level2 project request body exceeds the 384 KiB API limit");
    const auto format = lower_ascii(trim(required_project_string(
        raw_body, "format")));
    if (format == "sdk-quote-transition") {
        static const std::set<std::string> allowed{
            "format", "data_type", "payload_hex", "previous", "context"};
        require_project_fields(raw_body, allowed, "SDK quote transition");
        const auto data_type = project_data_type(raw_body);
        auto payload = parse_project_inline_hex(required_project_string(
            raw_body, "payload_hex"));
        if (payload.size() != quote_body_size)
            throw Error("Level2 SDK quote transition project requires exactly 380 body bytes");

        const auto& previous = required_project_member(raw_body, "previous");
        const auto& context = required_project_member(raw_body, "context");
        if (!previous.is_object()) throw Error("previous must be an object");
        if (!context.is_object()) throw Error("context must be an object");

        Level2SdkQuoteTransitionRequest request;
        request.data_type = data_type == 1807
            ? Level2SdkCallbackDataType::quote_update
            : Level2SdkCallbackDataType::extended_quote;
        request.body = std::move(payload);
        request.previous = previous;
        request.context = context;
        auto result = project_level2_sdk_quote_transition(request);
        add_project_transport_metadata(result, quote_body_size);
        result["previous_input_retained"] = false;
        result["context_input_retained"] = false;
        return result;
    }

    if (format == "sdk-1801-host-projection" ||
        format == "sdk-1802-host-projection") {
        static const std::set<std::string> allowed{
            "format", "market", "code", "payload_hex"};
        require_project_fields(raw_body, allowed, "SDK 1801/1802 host");
        const auto market = project_market(raw_body);
        const auto code = project_code(raw_body);
        auto payload = parse_project_inline_hex(required_project_string(
            raw_body, "payload_hex"));
        const auto input_size = payload.size();

        Json result;
        if (format == "sdk-1801-host-projection") {
            Level2Sdk1801HostProjectionRequest request;
            request.market_id = market;
            request.code = code;
            request.body = std::move(payload);
            result = project_level2_sdk_1801_host_projection(request);
        } else {
            Level2Sdk1802HostProjectionRequest request;
            request.market_id = market;
            request.code = code;
            request.body = std::move(payload);
            result = project_level2_sdk_1802_host_projection(request);
        }
        add_project_transport_metadata(result, input_size);
        return result;
    }

    if (format == "sdk-1804-host-projection") {
        static const std::set<std::string> allowed{
            "format", "payload_hex"};
        require_project_fields(raw_body, allowed, "SDK 1804 host");
        auto payload = parse_project_inline_hex(required_project_string(
            raw_body, "payload_hex"));
        const auto input_size = payload.size();
        Level2Sdk1804HostProjectionRequest request;
        request.body = std::move(payload);
        auto result = project_level2_sdk_1804_host_projection(request);
        add_project_transport_metadata(result, input_size);
        return result;
    }

    if (format == "sdk-1803-depth-record-projection") {
        static const std::set<std::string> allowed{
            "format", "payload_hex"};
        require_project_fields(raw_body, allowed, "SDK 1803 depth record");
        auto payload = parse_project_inline_hex(required_project_string(
            raw_body, "payload_hex"));
        const auto input_size = payload.size();
        Level2Sdk1803DepthRecordProjectionRequest request;
        request.body = std::move(payload);
        auto result = project_level2_sdk_1803_depth_record_projection(request);
        add_project_transport_metadata(result, input_size);
        return result;
    }

    if (format == "sdk-18031-queue-record-projection") {
        static const std::set<std::string> allowed{
            "format", "market", "code", "payload_hex"};
        require_project_fields(raw_body, allowed, "SDK 18031 queue record");
        const auto market = project_market(raw_body);
        const auto code = project_code(raw_body);
        auto payload = parse_project_inline_hex(required_project_string(
            raw_body, "payload_hex"));
        const auto input_size = payload.size();
        Level2Sdk18031QueueRecordProjectionRequest request;
        request.market_id = market;
        request.code = code;
        request.body = std::move(payload);
        auto result = project_level2_sdk_18031_queue_record_projection(request);
        add_project_transport_metadata(result, input_size);
        return result;
    }

    if (format == "sdk-4654-dual-snapshot-transition") {
        static const std::set<std::string> allowed{
            "format", "decoded_hex", "raw_hex"};
        require_project_fields(raw_body, allowed, "SDK 4654 dual snapshot");
        auto decoded = parse_project_inline_hex(required_project_string(
            raw_body, "decoded_hex"), "decoded_hex");
        auto raw = parse_project_inline_hex(required_project_string(
            raw_body, "raw_hex"), "raw_hex");
        const auto decoded_size = decoded.size();
        const auto raw_size = raw.size();
        Level2Sdk4654DualSnapshotTransitionRequest request;
        request.decoded = std::move(decoded);
        request.raw = std::move(raw);
        auto result = project_level2_sdk_4654_dual_snapshot_transition(request);
        add_project_dual_transport_metadata(
            result, decoded_size, raw_size, "decoded", "raw");
        return result;
    }

    if (format == "sdk-4651-dual-snapshot-transition") {
        static const std::set<std::string> allowed{
            "format", "decoded_hex", "raw_hex", "previous"};
        require_project_fields(raw_body, allowed, "SDK 4651 dual snapshot");
        auto decoded = parse_project_inline_hex(required_project_string(
            raw_body, "decoded_hex"), "decoded_hex");
        auto raw = parse_project_inline_hex(required_project_string(
            raw_body, "raw_hex"), "raw_hex");
        const auto decoded_size = decoded.size();
        const auto raw_size = raw.size();
        Level2Sdk4651DualSnapshotTransitionRequest request;
        request.decoded = std::move(decoded);
        request.raw = std::move(raw);
        if (const auto* previous = project_member(raw_body, "previous"))
            request.previous = project_4651_previous(*previous);
        auto result = project_level2_sdk_4651_dual_snapshot_transition(request);
        add_project_dual_transport_metadata(
            result, decoded_size, raw_size, "decoded", "raw");
        result["previous_input_retained"] = false;
        return result;
    }

    if (format == "sdk-4655-companion-raw-transition") {
        static const std::set<std::string> allowed{
            "format", "companion_hex", "raw_hex"};
        require_project_fields(raw_body, allowed, "SDK 4655 companion/raw");
        auto companion = parse_project_inline_hex(required_project_string(
            raw_body, "companion_hex"), "companion_hex");
        auto raw = parse_project_inline_hex(required_project_string(
            raw_body, "raw_hex"), "raw_hex");
        const auto companion_size = companion.size();
        const auto raw_size = raw.size();
        Level2Sdk4655CompanionRawTransitionRequest request;
        request.companion_snapshot = std::move(companion);
        request.raw = std::move(raw);
        auto result = project_level2_sdk_4655_companion_raw_transition(request);
        add_project_dual_transport_metadata(
            result, companion_size, raw_size, "companion", "raw");
        return result;
    }

    throw Error(
        "Level2 project format must be sdk-quote-transition, "
        "sdk-1801-host-projection, sdk-1802-host-projection, or "
        "sdk-1804-host-projection, sdk-1803-depth-record-projection, or "
        "sdk-18031-queue-record-projection, "
        "sdk-4654-dual-snapshot-transition, "
        "sdk-4651-dual-snapshot-transition, or "
        "sdk-4655-companion-raw-transition");
}

}  // namespace tdx::server_detail
