#include "server_level2_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/level2.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::server_detail {
namespace {

constexpr std::size_t maximum_level2_payload_bytes = level2_offline_payload_limit;
constexpr std::size_t maximum_fnsubscribe_document_bytes = 16U * 1024U;

const Json& require_object(const Json& body, std::string_view operation) {
    if (!body.is_object())
        throw Error("Level2 " + std::string(operation) +
                    " request body must be a JSON object");
    return body;
}

void require_only_fields(const Json& body, const std::set<std::string>& allowed,
                         std::string_view operation) {
    for (const auto& [name, value] : body.as_object()) {
        (void)value;
        if (!allowed.count(name))
            throw Error("Level2 " + std::string(operation) +
                        " does not accept field: " + name);
    }
}

const Json* member(const Json& body, std::string_view name) {
    const auto found = body.as_object().find(std::string(name));
    return found == body.as_object().end() ? nullptr : &found->second;
}

std::string string_value(const Json& body, std::string_view name,
                         std::string fallback = {}, bool required = false) {
    const auto* value = member(body, name);
    if (!value) {
        if (required) throw Error(std::string(name) + " is required");
        return fallback;
    }
    if (!value->is_string())
        throw Error(std::string(name) + " must be a string");
    const auto result = value->as_string();
    if (required && result.empty())
        throw Error(std::string(name) + " is required");
    return result;
}

std::int64_t integer_value(const Json& body, std::string_view name,
                           std::int64_t fallback, std::int64_t minimum,
                           std::int64_t maximum, bool required = false) {
    const auto* value = member(body, name);
    if (!value) {
        if (required) throw Error(std::string(name) + " is required");
        return fallback;
    }
    if (!value->is_number() || !std::isfinite(value->as_number()) ||
        std::floor(value->as_number()) != value->as_number() ||
        value->as_number() < static_cast<double>(minimum) ||
        value->as_number() > static_cast<double>(maximum))
        throw Error(std::string(name) + " must be an integer in " +
                    std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    return static_cast<std::int64_t>(value->as_number());
}

bool boolean_value(const Json& body, std::string_view name, bool fallback) {
    const auto* value = member(body, name);
    if (!value) return fallback;
    if (!value->is_bool())
        throw Error(std::string(name) + " must be boolean");
    return value->as_bool();
}

float positive_float32_value(const Json& body, std::string_view name) {
    const auto* value = member(body, name);
    if (!value) throw Error(std::string(name) + " is required");
    if (!value->is_number() || !std::isfinite(value->as_number()) ||
        value->as_number() <= 0.0 ||
        value->as_number() >
            static_cast<double>(std::numeric_limits<float>::max()))
        throw Error(std::string(name) +
                    " must be a finite positive float32 value");
    const auto converted = static_cast<float>(value->as_number());
    if (!std::isfinite(converted) || converted <= 0.0F)
        throw Error(std::string(name) +
                    " must be a finite positive float32 value");
    return converted;
}

float finite_float32_value(const Json& body, std::string_view name) {
    const auto* value = member(body, name);
    if (!value) throw Error(std::string(name) + " is required");
    if (!value->is_number() || !std::isfinite(value->as_number()) ||
        value->as_number() <
            static_cast<double>(std::numeric_limits<float>::lowest()) ||
        value->as_number() >
            static_cast<double>(std::numeric_limits<float>::max()))
        throw Error(std::string(name) +
                    " must be a finite float32 value");
    const auto converted = static_cast<float>(value->as_number());
    if (!std::isfinite(converted))
        throw Error(std::string(name) +
                    " must be a finite float32 value");
    return converted;
}

int hex_digit(unsigned char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool strict_utf8(std::string_view text) {
    for (std::size_t index = 0; index < text.size();) {
        const auto lead = static_cast<unsigned char>(text[index++]);
        if (lead <= 0x7f) continue;
        auto continuation = [&](unsigned char minimum,
                                unsigned char maximum) {
            if (index >= text.size()) return false;
            const auto byte = static_cast<unsigned char>(text[index++]);
            return byte >= minimum && byte <= maximum;
        };
        if (lead >= 0xc2 && lead <= 0xdf) {
            if (!continuation(0x80, 0xbf)) return false;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            if (!continuation(lead == 0xe0 ? 0xa0 : 0x80,
                              lead == 0xed ? 0x9f : 0xbf) ||
                !continuation(0x80, 0xbf))
                return false;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            if (!continuation(lead == 0xf0 ? 0x90 : 0x80,
                              lead == 0xf4 ? 0x8f : 0xbf) ||
                !continuation(0x80, 0xbf) ||
                !continuation(0x80, 0xbf))
                return false;
        } else {
            return false;
        }
    }
    return true;
}

Bytes parse_inline_hex(const std::string& text) {
    std::size_t digits = 0;
    for (const unsigned char ch : text) {
        if (std::isspace(ch)) continue;
        if (hex_digit(ch) < 0)
            throw Error("payload_hex must contain hexadecimal digits and whitespace only");
        ++digits;
        if (digits > maximum_level2_payload_bytes * 2)
            throw Error("Level2 decoded payload exceeds the 384 KiB API limit");
    }
    if (!digits) throw Error("payload_hex is required");
    if (digits % 2)
        throw Error("payload_hex must contain an even number of hexadecimal digits");

    Bytes result;
    result.reserve(digits / 2);
    int high = -1;
    for (const unsigned char ch : text) {
        if (std::isspace(ch)) continue;
        const int nibble = hex_digit(ch);
        if (high < 0) high = nibble;
        else {
            result.push_back(static_cast<std::uint8_t>((high << 4) | nibble));
            high = -1;
        }
    }
    return result;
}

std::string render_hex(const Bytes& value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (const auto byte : value) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

bool record_decode_format(const std::string& format) {
    static const std::set<std::string> formats{
        "direct-transaction", "direct-order", "sdk-1801", "sdk-1802",
        "sdk-1803", "sdk-18031", "sdk-1804", "sdk-1807", "sdk-18071",
        "sdk-correlation", "tpbus-111", "tpbus-112", "tpbus-115",
        "tcalc-order-flow",
        "tcalc-order-side"};
    return formats.count(format) != 0;
}

int sdk_json_function_id(const std::string& format) {
    if (format == "sdk-json-4653") return 4653;
    if (format == "sdk-json-4655") return 4655;
    if (format == "sdk-json-4671") return 4671;
    if (format == "sdk-json-4680") return 4680;
    return 0;
}

Level2SdkFnSubscribeDataType fnsubscribe_data_type(const Json& document) {
    const auto* value = member(document, "data_type");
    if (!value) throw Error("data_type is required");
    if (!value->is_number() || !std::isfinite(value->as_number()) ||
        std::floor(value->as_number()) != value->as_number())
        throw Error("data_type must be the integer 1801, 1802, or 1803");
    const double raw = value->as_number();
    if (raw == 1801.0) return Level2SdkFnSubscribeDataType::transaction;
    if (raw == 1802.0) return Level2SdkFnSubscribeDataType::order;
    if (raw == 1803.0)
        return Level2SdkFnSubscribeDataType::multi_level_quote;
    if (raw == 1807.0)
        throw Error("fnSubscribeData type 1807 must use sdk-1807-plan");
    throw Error("data_type must be 1801, 1802, or 1803");
}

Level2SdkFnReqDataType fnreqdata_data_type(const Json& body) {
    const auto raw = integer_value(
        body, "data_type", 0, 1801, 18071, true);
    switch (raw) {
    case 1801: return Level2SdkFnReqDataType::transaction;
    case 1802: return Level2SdkFnReqDataType::order;
    case 1803: return Level2SdkFnReqDataType::multi_level_quote;
    case 1804: return Level2SdkFnReqDataType::price_queue;
    case 18071: return Level2SdkFnReqDataType::extended_quote;
    default:
        throw Error(
            "data_type must be 1801, 1802, 1803, 1804, or 18071");
    }
}

Level2SdkCallbackDataType callback_route_data_type(const Json& body) {
    const auto raw = integer_value(
        body, "data_type", 0, 1801, 18071, true);
    switch (raw) {
    case 1801: return Level2SdkCallbackDataType::transaction;
    case 1802: return Level2SdkCallbackDataType::order;
    case 1803: return Level2SdkCallbackDataType::multi_level_quote;
    case 1804: return Level2SdkCallbackDataType::price_queue;
    case 1807: return Level2SdkCallbackDataType::quote_update;
    case 18031: return Level2SdkCallbackDataType::order_queue_at_price;
    case 18071: return Level2SdkCallbackDataType::extended_quote;
    default:
        throw Error(
            "data_type must be 1801, 1802, 1803, 1804, 1807, 18031, or 18071");
    }
}

}  // namespace

Json query_level2_build(const Json& raw_body) {
    const auto& body = require_object(raw_body, "build");
    const auto format = lower_ascii(trim(string_value(
        body, "format", {}, true)));

    if (format == "sdk-1807-plan") {
        require_only_fields(body, {"format", "payload_hex"}, "build");
        auto packed = parse_inline_hex(string_value(
            body, "payload_hex", {}, true));
        auto result = build_level2_sdk_1807_request_plan(packed);
        result["input_size"] = static_cast<std::uint64_t>(packed.size());
        result["input_retained"] = false;
        result["file_accessed"] = false;
        result["network_requests"] = 0;
        return result;
    }

    if (format == "sdk-fnsubscribe-batch-plan") {
        require_only_fields(body, {"format", "document"}, "build");
        const auto* document = member(body, "document");
        if (!document) throw Error("document is required");
        if (!document->is_object())
            throw Error("document must be an inline JSON object");

        const auto compact = document->dump(-1);
        if (!strict_utf8(compact))
            throw Error("fnSubscribeData document must be valid UTF-8");
        if (compact.empty() ||
            compact.size() > maximum_fnsubscribe_document_bytes)
            throw Error("fnSubscribeData compact JSON document exceeds the 16 KiB API limit");
        require_only_fields(
            *document, {"data_type", "symbols"},
            "fnSubscribeData build document");

        const auto data_type = fnsubscribe_data_type(*document);
        const auto* symbols_value = member(*document, "symbols");
        if (!symbols_value) throw Error("symbols is required");
        if (!symbols_value->is_array())
            throw Error("symbols must be an array");
        const auto& symbol_items = symbols_value->as_array();
        if (symbol_items.empty() || symbol_items.size() > 100)
            throw Error("symbols must contain 1..100 items");
        std::vector<std::string> symbols;
        symbols.reserve(symbol_items.size());
        for (const auto& item : symbol_items) {
            if (!item.is_string())
                throw Error("symbols must contain only strings");
            symbols.push_back(item.as_string());
        }

        auto result = build_level2_sdk_fnsubscribe_batch_call_plan(
            {data_type, std::move(symbols)});
        result["input_encoding"] = "inline-json";
        result["input_size"] = static_cast<std::uint64_t>(compact.size());
        result["input_size_limit"] =
            static_cast<std::uint64_t>(maximum_fnsubscribe_document_bytes);
        result["input_retained"] = false;
        result["file_accessed"] = false;
        return result;
    }

    if (format == "sdk-callback-route-plan") {
        require_only_fields(
            body,
            {"format", "data_type", "registry_mode_raw",
             "host_time_advanced"},
            "SDK callback route build");
        Level2SdkCallbackRoutePlanRequest request;
        request.data_type = callback_route_data_type(body);
        if (member(body, "registry_mode_raw")) {
            request.registry_mode_raw = static_cast<std::uint32_t>(
                integer_value(
                    body, "registry_mode_raw", 0, 0,
                    static_cast<std::int64_t>(
                        std::numeric_limits<std::uint32_t>::max()),
                    true));
        }
        if (const auto* host_time = member(body, "host_time_advanced")) {
            if (!host_time->is_bool())
                throw Error("host_time_advanced must be boolean");
            request.host_time_advanced = host_time->as_bool();
        }
        auto result = build_level2_sdk_callback_route_plan(request);
        result["input_retained"] = false;
        result["file_accessed"] = false;
        return result;
    }

    if (format == "sdk-fnreqdata-plan") {
        require_only_fields(
            body,
            {"format", "market", "code", "data_type", "cursor_raw",
             "count"},
            "SDK fnReqData build");
        Level2SdkFnReqDataCallPlanRequest request;
        request.market_id = static_cast<std::uint16_t>(integer_value(
            body, "market", 0, 0, 2, true));
        request.code = string_value(body, "code", {}, true);
        request.data_type = fnreqdata_data_type(body);
        if (member(body, "cursor_raw")) {
            request.cursor_raw = static_cast<std::uint32_t>(integer_value(
                body, "cursor_raw", 0, 0,
                static_cast<std::int64_t>(
                    std::numeric_limits<std::uint32_t>::max()),
                true));
        }
        if (member(body, "count")) {
            request.request_count = static_cast<std::uint16_t>(integer_value(
                body, "count", 0, 1, 1500, true));
        }
        auto result = build_level2_sdk_fnreqdata_call_plan(request);
        result["input_retained"] = false;
        result["file_accessed"] = false;
        return result;
    }

    if (format == "sdk-fnreqdata-18031-plan") {
        require_only_fields(
            body,
            {"format", "market", "code", "side_mode_raw",
             "selected_price"},
            "SDK fnReqData 18031 build");
        Level2SdkFnReqData18031CallPlanRequest request;
        request.market_id = static_cast<std::uint16_t>(integer_value(
            body, "market", 0, 0, 2, true));
        request.code = string_value(body, "code", {}, true);
        request.side_mode_raw = static_cast<std::uint8_t>(integer_value(
            body, "side_mode_raw", 0, 0, 255, true));
        request.selected_price = finite_float32_value(
            body, "selected_price");
        auto result = build_level2_sdk_fnreqdata_18031_call_plan(request);
        result["input_retained"] = false;
        result["file_accessed"] = false;
        return result;
    }

    std::set<std::string> allowed{"format", "market", "code"};
    if (format == "direct") {
        allowed.insert({"kind", "cursor", "count", "initial"});
    } else if (format == "sdk-4653") {
        allowed.insert({"attach_info", "repurchase_time"});
    } else if (format == "sdk-4655") {
        allowed.insert({"want_number", "attach_info"});
    } else if (format == "sdk-4680") {
        allowed.insert("depth");
    } else if (format == "tdxw-1369") {
        allowed.insert("count");
    } else if (format == "tdxw-1371") {
        allowed.insert({"side_mode_raw", "selected_price", "cursor", "count"});
    } else {
        throw Error("format must be direct, sdk-4653, sdk-4655, sdk-4680, "
                    "sdk-1807-plan, sdk-fnreqdata-plan, "
                    "sdk-fnreqdata-18031-plan, sdk-fnsubscribe-batch-plan, "
                    "sdk-callback-route-plan, tdxw-1369, or tdxw-1371");
    }
    require_only_fields(body, allowed, "build");

    const auto market = integer_value(
        body, "market", 0, 0,
        format == "direct" || format == "tdxw-1369" ||
                format == "tdxw-1371"
            ? 2
            : 65535,
        true);
    const auto code = string_value(body, "code", {}, true);
    Bytes payload;
    Json result = Json::object();
    result["format"] = format;
    result["market_id"] = market;
    result["code"] = code;
    if (format == "direct") {
        const auto kind = lower_ascii(trim(string_value(
            body, "kind", "transaction")));
        const auto cursor = integer_value(
            body, "cursor", 0, 0,
            static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()));
        const auto count = integer_value(body, "count", 1500, 1, 1500);
        const bool initial = boolean_value(body, "initial", false);
        payload = build_level2_direct_request(
            kind, static_cast<int>(market), code,
            static_cast<std::uint32_t>(cursor), static_cast<std::uint16_t>(count),
            initial ? Level2DirectRequestVariant::initial
                    : Level2DirectRequestVariant::standard);
        result["schema"] = "tdx-level2-direct-request-v1";
        result["kind"] = kind;
        result["variant"] = initial ? "initial" : "standard";
        result["initial"] = initial;
        result["command"] = static_cast<std::uint64_t>(
            read_u16_le(payload.data() + 10));
        result["cursor"] = cursor;
        result["count"] = count;
    } else if (format == "tdxw-1369") {
        const auto request_count = integer_value(body, "count", 11, 1, 1000);
        payload = build_level2_tdxw_1369_request(
            {static_cast<std::uint16_t>(market), code,
             static_cast<std::uint16_t>(request_count)});
        result["schema"] = "tdx-level2-tdxw-ipc-request-v1";
        result["command"] = 1369;
        result["data_type"] = 1803;
        result["request_count"] = request_count;
        result["transport"] = "TdxW internal IPC";
        result["request_kind"] = "tdxw-internal-ipc-fixed-body";
        result["sdk_request"] = false;
        result["standalone_network_request"] = false;
        result["network_request_bytes"] = false;
        result["request_sent"] = false;
    } else if (format == "tdxw-1371") {
        const auto side_mode_raw = integer_value(
            body, "side_mode_raw", 0, 0, 255, true);
        const auto selected_price = positive_float32_value(
            body, "selected_price");
        const auto cursor = integer_value(
            body, "cursor", -1,
            static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
            static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()));
        const auto request_count = integer_value(body, "count", 5000, 1, 5000);
        payload = build_level2_tdxw_1371_request(
            {static_cast<std::uint16_t>(market), code,
             static_cast<std::uint8_t>(side_mode_raw), selected_price,
             static_cast<std::int32_t>(cursor),
             static_cast<std::uint16_t>(request_count)});
        result["schema"] = "tdx-level2-tdxw-ipc-request-v1";
        result["command"] = 1371;
        result["data_type"] = 18031;
        result["side_mode_raw"] = side_mode_raw;
        result["selected_price"] = static_cast<double>(selected_price);
        result["cursor"] = cursor;
        result["request_count"] = request_count;
        result["transport"] = "TdxW internal IPC";
        result["request_kind"] = "tdxw-internal-ipc-fixed-body";
        result["sdk_request"] = false;
        result["standalone_network_request"] = false;
        result["network_request_bytes"] = false;
        result["request_sent"] = false;
    } else {
        const int function_id = format == "sdk-4653" ? 4653 :
                                format == "sdk-4655" ? 4655 : 4680;
        const auto want_number = integer_value(
            body, "want_number", 80, 1, 65535);
        const auto depth = integer_value(body, "depth", 10, 5, 10);
        const bool attach_info = boolean_value(body, "attach_info", false);
        const bool repurchase_time = boolean_value(
            body, "repurchase_time", false);
        payload = build_level2_sdk_redirect_request(
            function_id, static_cast<int>(market), code,
            static_cast<std::uint16_t>(want_number), static_cast<int>(depth),
            attach_info, repurchase_time);
        result["schema"] = "tdx-level2-sdk-redirect-request-v1";
        result["function_id"] = function_id;
        if (function_id == 4655)
            result["want_number_effective"] = want_number > 500 ? 80 : want_number;
        if (function_id == 4680) result["depth"] = depth;
        if (function_id == 4653 || function_id == 4655)
            result["attach_info"] = attach_info;
        if (function_id == 4653)
            result["repurchase_time"] = repurchase_time;
    }
    result["size"] = static_cast<std::uint64_t>(payload.size());
    result["payload_hex"] = render_hex(payload);
    result["offline"] = true;
    result["network_requests"] = 0;
    result["subscription_sent"] = false;
    result["entitlement_bypass"] = false;
    return result;
}

Json query_level2_decode(const Json& raw_body) {
    const auto& body = require_object(raw_body, "decode");
    const auto format = lower_ascii(trim(string_value(
        body, "format", {}, true)));
    const int sdk_json_id = sdk_json_function_id(format);
    const bool sdk_callback_invocation =
        format == "sdk-callback-invocation";
    if (!record_decode_format(format) && format != "protobuf" &&
        !sdk_json_id && !sdk_callback_invocation)
        throw Error("unsupported Level2 decode format");

    if (sdk_callback_invocation) {
        require_only_fields(
            body,
            {"format", "data_type", "callback_arg5_raw",
             "callback_arg6_raw", "registry_mode_raw", "payload_hex",
             "limit"},
            "SDK callback invocation decode");

        Level2SdkCallbackInvocationRequest request;
        request.data_type = callback_route_data_type(body);
        const auto data_type = static_cast<int>(request.data_type);
        const auto callback_arg5_raw = integer_value(
            body, "callback_arg5_raw", 0,
            static_cast<std::int64_t>(
                std::numeric_limits<std::int32_t>::min()),
            static_cast<std::int64_t>(
                std::numeric_limits<std::int32_t>::max()),
            true);
        if (data_type == 1801 || data_type == 1802) {
            const auto unit_size = data_type == 1801 ? 52 : 40;
            const auto maximum_count = static_cast<std::int64_t>(
                maximum_level2_payload_bytes /
                static_cast<std::size_t>(unit_size));
            if (callback_arg5_raw < 1 ||
                callback_arg5_raw > maximum_count)
                throw Error(
                    "callback_arg5_raw must be a record count in 1.." +
                    std::to_string(maximum_count) + " for data_type " +
                    std::to_string(data_type));
        }
        request.callback_arg5_raw = static_cast<std::int32_t>(
            callback_arg5_raw);
        request.callback_arg6_raw = static_cast<std::uint32_t>(
            integer_value(
                body, "callback_arg6_raw", 0, 0,
                static_cast<std::int64_t>(
                    std::numeric_limits<std::uint32_t>::max()),
                true));
        if (member(body, "registry_mode_raw")) {
            request.registry_mode_raw = static_cast<std::uint32_t>(
                integer_value(
                    body, "registry_mode_raw", 0, 0,
                    static_cast<std::int64_t>(
                        std::numeric_limits<std::uint32_t>::max()),
                    true));
        }
        request.body = parse_inline_hex(string_value(
            body, "payload_hex", {}, true));
        const auto input_size = request.body.size();
        const auto limit = static_cast<int>(integer_value(
            body, "limit", 20, 0, 10000));
        auto result = decode_level2_sdk_callback_invocation(
            request, limit);
        result["input_encoding"] = "hex";
        result["input_size"] = static_cast<std::uint64_t>(input_size);
        result["input_size_limit"] =
            static_cast<std::uint64_t>(maximum_level2_payload_bytes);
        result["input_retained"] = false;
        result["file_accessed"] = false;
        return result;
    }

    if (sdk_json_id) {
        std::set<std::string> allowed{"format", "document"};
        if (sdk_json_id == 4680) allowed.insert("depth");
        else allowed.insert("limit");
        require_only_fields(body, allowed, "decode");

        const auto* document = member(body, "document");
        if (!document) throw Error("document is required");
        if (!document->is_object())
            throw Error("document must be an inline JSON object");
        const auto input_size = document->dump(-1).size();
        if (input_size > maximum_level2_payload_bytes)
            throw Error("inline SDK JSON document exceeds the 384 KiB API limit");

        const int limit = sdk_json_id == 4680 ? 20 : static_cast<int>(
            integer_value(body, "limit", 20, 0, 10000));
        const int depth = sdk_json_id == 4680 ? static_cast<int>(
            integer_value(body, "depth", 10, 5, 10)) : 10;
        if (sdk_json_id == 4680 && depth != 5 && depth != 10)
            throw Error("depth must be 5 or 10");

        auto result = normalize_level2_sdk_json(
            sdk_json_id, *document, limit, depth);
        result["input_encoding"] = "inline-json";
        result["input_size"] = static_cast<std::uint64_t>(input_size);
        result["input_size_limit"] =
            static_cast<std::uint64_t>(maximum_level2_payload_bytes);
        result["input_retained"] = false;
        result["file_accessed"] = false;
        result["network_requests"] = 0;
        result["subscription_sent"] = false;
        return result;
    }

    std::set<std::string> allowed{"format", "payload_hex"};
    if (record_decode_format(format) && format != "tcalc-order-side")
        allowed.insert("limit");
    if (format == "direct-transaction" || format == "direct-order")
        allowed.insert("xor_key");
    if (format == "protobuf")
        allowed.insert({"max_fields", "sample_bytes"});
    require_only_fields(body, allowed, "decode");

    auto payload = parse_inline_hex(string_value(
        body, "payload_hex", {}, true));
    const auto payload_size = payload.size();
    const int limit = static_cast<int>(integer_value(
        body, "limit", 20, 0, 10000));
    const int xor_key = static_cast<int>(integer_value(
        body, "xor_key", -1, -1, 255));
    const int max_fields = static_cast<int>(integer_value(
        body, "max_fields", 100, 1, 10000));
    const int sample_bytes = static_cast<int>(integer_value(
        body, "sample_bytes", 64, 0, 4096));
    auto result = decode_level2_document(
        format, std::move(payload), limit, xor_key, max_fields, sample_bytes);
    result["input_encoding"] = "hex";
    result["input_size"] = static_cast<std::uint64_t>(payload_size);
    result["input_size_limit"] =
        static_cast<std::uint64_t>(maximum_level2_payload_bytes);
    result["input_retained"] = false;
    result["file_accessed"] = false;
    result["network_requests"] = 0;
    result["subscription_sent"] = false;
    return result;
}

}  // namespace tdx::server_detail
