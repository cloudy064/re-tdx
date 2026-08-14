#include "server_level2_internal.hpp"

#include "tdx/common.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_error(const std::function<void()>& action,
                   const std::string& fragment) {
    try {
        action();
    } catch (const tdx::Error& error) {
        require(std::string(error.what()).find(fragment) != std::string::npos,
                "error includes the expected contract detail");
        return;
    }
    throw tdx::Error("expected Level2 project validation failure");
}

tdx::Json levels() {
    auto result = tdx::Json::array();
    for (int index = 0; index < 10; ++index) {
        auto row = tdx::Json::object();
        row["level"] = index + 1;
        row["price"] = 0;
        row["quantity_raw"] = 0;
        result.push_back(std::move(row));
    }
    return result;
}

tdx::Json previous_state() {
    auto base = tdx::Json::object();
    for (const auto* field : {
             "pre_close_price", "open_price", "high_price", "low_price",
             "last_price", "special_volume_projected_raw", "amount_raw"})
        base[field] = 0;
    for (const auto* field : {
             "time_hhmmss_raw", "cumulative_volume_raw",
             "last_positive_volume_delta_raw", "first_volume_bucket_raw",
             "second_volume_bucket_raw", "host_status_flags_raw",
             "host_auxiliary_142_raw"})
        base[field] = 0;

    auto aggregate = tdx::Json::object();
    aggregate["average_bid_price"] = 0;
    aggregate["total_bid_quantity_raw"] = 0;
    aggregate["average_ask_price"] = 0;
    aggregate["total_ask_quantity_raw"] = 0;

    auto result = tdx::Json::object();
    result["schema"] = "tdx-level2-sdk-host-quote-state-v1";
    result["base_quote"] = std::move(base);
    result["ask_levels"] = levels();
    result["bid_levels"] = levels();
    result["aggregate"] = std::move(aggregate);
    result["caller_secret"] = "previous-secret-sentinel";
    return result;
}

tdx::Json context() {
    auto result = tdx::Json::object();
    result["security_class_raw"] = 0;
    result["price_transition_guard_raw"] = 0;
    result["small_last_price_fallback_predicate_raw"] = false;
    result["special_volume_multiplier_raw"] = 1;
    result["security_auxiliary_dword_73_raw"] = 0;
    result["host_word_280_raw"] = 0;
    result["caller_secret"] = "context-secret-sentinel";
    return result;
}

tdx::Json request(int data_type = 1807,
                  std::size_t payload_bytes = 380) {
    auto result = tdx::Json::object();
    result["format"] = "sdk-quote-transition";
    result["data_type"] = data_type;
    result["payload_hex"] = std::string(payload_bytes * 2U, '0');
    result["previous"] = previous_state();
    result["context"] = context();
    return result;
}

tdx::Json host_request(int data_type, std::size_t payload_bytes) {
    auto result = tdx::Json::object();
    result["format"] = data_type == 1801
        ? "sdk-1801-host-projection" : "sdk-1802-host-projection";
    result["market"] = data_type == 1801 ? 0 : 1;
    result["code"] = data_type == 1801 ? "000001" : "600000";
    result["payload_hex"] = std::string(payload_bytes * 2U, '0');
    return result;
}

std::string payload_hex(const tdx::Bytes& body) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(body.size() * 2U);
    for (const auto byte : body) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

void write_u32(tdx::Bytes& body, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index)
        body[offset + index] =
            static_cast<std::uint8_t>(value >> (8U * index));
}

void test_happy_paths_and_non_retention() {
    for (const int data_type : {1807, 18071}) {
        const auto projected =
            tdx::server_detail::query_level2_project(request(data_type));
        require(projected.at("schema").as_string() ==
                    "tdx-level2-sdk-quote-transition-v1" &&
                    projected.at("format").as_string() ==
                    "sdk-quote-transition" &&
                    projected.at("data_type").as_number() == data_type &&
                    projected.at("input_encoding").as_string() == "hex" &&
                    projected.at("input_size").as_number() == 380.0 &&
                    projected.at("input_size_limit").as_number() ==
                        384.0 * 1024.0 &&
                    projected.at("request_size_limit").as_number() ==
                        384.0 * 1024.0 &&
                    !projected.at("input_retained").as_bool() &&
                    !projected.at("input_body_retained").as_bool() &&
                    !projected.at("previous_input_retained").as_bool() &&
                    !projected.at("context_input_retained").as_bool() &&
                    !projected.at("file_accessed").as_bool() &&
                    projected.at("offline").as_bool() &&
                    !projected.at("sdk_called").as_bool() &&
                    !projected.at("sdk_callback_executed").as_bool() &&
                    !projected.at("host_message_dispatch_attempted").as_bool() &&
                    !projected.at("wire_bytes_built").as_bool() &&
                    !projected.at("network_request_bytes_built").as_bool() &&
                    projected.at("network_requests").as_number() == 0.0 &&
                    !projected.at("request_sent").as_bool() &&
                    !projected.at("subscription_sent").as_bool() &&
                    !projected.at("entitlement_bypass").as_bool(),
                "SDK quote transition HTTP projection stays offline");
        const auto rendered = projected.dump(-1);
        require(rendered.find("previous-secret-sentinel") == std::string::npos &&
                    rendered.find("context-secret-sentinel") == std::string::npos &&
                    rendered.find(std::string(760, '0')) == std::string::npos,
                "HTTP projection does not echo previous, context, or payload bodies");
    }
}

void test_strict_request_contract() {
    for (const auto* forbidden : {
             "path", "url", "file", "input", "body", "document",
             "token", "handle", "encoding", "callback_key",
             "window_handle", "endpoint"}) {
        auto value = request();
        value[forbidden] = "forbidden";
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(value);
        }, std::string("does not accept field: ") + forbidden);
    }
    for (const auto* required : {
             "format", "data_type", "payload_hex", "previous", "context"}) {
        auto value = request();
        value.as_object().erase(required);
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(value);
        }, std::string(required) + " is required");
    }
    auto wrong_format = request();
    wrong_format["format"] = "sdk-1807";
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(wrong_format);
    }, "sdk-1801-host-projection");
    for (const double data_type : {1801.0, 1807.5, 18072.0}) {
        auto value = request();
        value["data_type"] = data_type;
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(value);
        }, "integer 1807 or 18071");
    }
    for (const auto size : {379U, 381U}) {
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(request(1807, size));
        }, "exactly 380");
    }
    auto odd_hex = request();
    odd_hex["payload_hex"] = "abc";
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(odd_hex);
    }, "even number");
    auto invalid_hex = request();
    invalid_hex["payload_hex"] = std::string(759, '0') + "g";
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(invalid_hex);
    }, "hexadecimal digits");

    auto previous_array = request();
    previous_array["previous"] = tdx::Json::array();
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(previous_array);
    }, "previous must be an object");
    auto context_array = request();
    context_array["context"] = tdx::Json::array();
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(context_array);
    }, "context must be an object");

    auto oversized = request();
    oversized["padding"] = std::string(384U * 1024U, 'x');
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(oversized);
    }, "384 KiB");
}


void test_1801_1802_host_projection_http_contract() {
    struct Case {
        int data_type;
        std::size_t source_size;
        const char* schema;
        const char* format;
    };
    for (const auto& fixture : {
             Case{1801, 52, "tdx-level2-sdk-1801-host-projection-v1",
                  "sdk-1801-host-projection"},
             Case{1802, 40, "tdx-level2-sdk-1802-host-projection-v1",
                  "sdk-1802-host-projection"}}) {
        const auto projected = tdx::server_detail::query_level2_project(
            host_request(fixture.data_type, fixture.source_size));
        require(projected.at("schema").as_string() == fixture.schema &&
                    projected.at("format").as_string() == fixture.format &&
                    projected.at("source_record_count").as_number() == 1.0 &&
                    projected.at("projected_state").at("record_size")
                            .as_number() == 20.0 &&
                    projected.at("projected_state").at("record_count")
                            .as_number() == 1.0 &&
                    projected.at("input_encoding").as_string() == "hex" &&
                    projected.at("input_size").as_number() ==
                        static_cast<double>(fixture.source_size) &&
                    !projected.at("input_retained").as_bool() &&
                    !projected.at("input_body_retained").as_bool() &&
                    !projected.at("file_accessed").as_bool() &&
                    !projected.at("subscription_sent").as_bool() &&
                    projected.at("offline").as_bool() &&
                    !projected.at("sdk_called").as_bool() &&
                    !projected.at("host_storage_mutated").as_bool() &&
                    projected.at("network_requests").as_number() == 0.0 &&
                    !projected.at("request_sent").as_bool() &&
                    !projected.at("entitlement_bypass").as_bool(),
                "SDK 1801/1802 HTTP host projection stays offline");
        require(projected.dump(-1).find(
                    std::string(fixture.source_size * 2U, '0')) ==
                    std::string::npos,
                "HTTP host projection does not echo its inline payload");
    }

    for (const auto* forbidden : {
             "path", "url", "file", "input", "body", "document",
             "token", "handle", "encoding", "data_type", "previous",
             "context", "callback_key", "window_handle", "endpoint"}) {
        auto value = host_request(1801, 52);
        value[forbidden] = "forbidden";
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(value);
        }, std::string("does not accept field: ") + forbidden);
    }
    for (const auto* required : {"market", "code", "payload_hex"}) {
        auto value = host_request(1801, 52);
        value.as_object().erase(required);
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(value);
        }, std::string(required) + " is required");
    }
    for (const double market : {-1.0, 1.5, 3.0}) {
        auto value = host_request(1801, 52);
        value["market"] = market;
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(value);
        }, "integer 0, 1, or 2");
    }
    for (const auto* code : {"00001", "00A001", "0000001"}) {
        auto value = host_request(1802, 40);
        value["code"] = code;
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(value);
        }, "exactly 6 ASCII digits");
    }
    require_error([] {
        (void)tdx::server_detail::query_level2_project(
            host_request(1801, 51));
    }, "multiple of 52");
    require_error([] {
        (void)tdx::server_detail::query_level2_project(
            host_request(1802, 41));
    }, "multiple of 40");
}

void test_1804_host_projection_http_contract() {
    auto value = tdx::Json::object();
    value["format"] = "sdk-1804-host-projection";
    value["payload_hex"] = std::string(432U * 2U, '0');
    const auto projected = tdx::server_detail::query_level2_project(value);
    require(projected.at("schema").as_string() ==
                "tdx-level2-sdk-1804-host-projection-v1" &&
                projected.at("format").as_string() ==
                    "sdk-1804-host-projection" &&
                projected.at("input_size").as_number() == 432.0 &&
                projected.at("projected_state").at("slot_count")
                        .as_number() == 105.0 &&
                projected.at("projected_state").at("byte_size")
                        .as_number() == 420.0 &&
                !projected.at("input_retained").as_bool() &&
                !projected.at("input_body_retained").as_bool() &&
                !projected.at("file_accessed").as_bool() &&
                !projected.at("sdk_called").as_bool() &&
                !projected.at("sub_525600_called").as_bool() &&
                projected.at("network_requests").as_number() == 0.0 &&
                !projected.at("request_sent").as_bool() &&
                !projected.at("entitlement_bypass").as_bool(),
            "SDK 1804 HTTP host projection stays offline");
    require(projected.dump(-1).find(std::string(432U * 2U, '0')) ==
                std::string::npos,
            "HTTP 1804 projection does not echo its inline payload");

    for (const auto size : {431U, 433U}) {
        auto invalid = value;
        invalid["payload_hex"] = std::string(size * 2U, '0');
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(invalid);
        }, "exactly 432");
    }
    for (const auto* forbidden : {
             "market", "code", "path", "url", "file", "input",
             "document", "token", "handle", "encoding", "previous",
             "context", "endpoint"}) {
        auto invalid = value;
        invalid[forbidden] = "forbidden";
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(invalid);
        }, std::string("does not accept field: ") + forbidden);
    }
}

void test_1803_18031_record_projection_http_contract() {
    tdx::Bytes depth_body(32016, 0);
    write_u32(depth_body, 8, 1);
    write_u32(depth_body, 12, 1);
    auto depth_request = tdx::Json::object();
    depth_request["format"] = "sdk-1803-depth-record-projection";
    depth_request["payload_hex"] = payload_hex(depth_body);
    const auto depth =
        tdx::server_detail::query_level2_project(depth_request);
    require(depth.at("schema").as_string() ==
                "tdx-level2-sdk-1803-depth-record-projection-v1" &&
                depth.at("format").as_string() ==
                    "sdk-1803-depth-record-projection" &&
                depth.at("input_size").as_number() == 32016.0 &&
                depth.at("projected_record_size").as_number() == 13.0 &&
                depth.at("projected_state").at("projected_record_count")
                        .as_number() == 2.0 &&
                depth.at("projected_state").at("first").at("count_raw")
                        .as_number() == 1.0 &&
                depth.at("projected_state").at("second").at("count_raw")
                        .as_number() == 1.0 &&
                !depth.at("input_retained").as_bool() &&
                !depth.at("input_body_retained").as_bool() &&
                !depth.at("file_accessed").as_bool() &&
                !depth.at("sdk_called").as_bool() &&
                !depth.at("host_state_write_performed").as_bool() &&
                depth.at("network_requests").as_number() == 0.0 &&
                !depth.at("request_sent").as_bool() &&
                !depth.at("entitlement_bypass").as_bool(),
            "SDK 1803 HTTP depth record projection stays offline");
    require(depth.dump(-1).find(depth_request.at("payload_hex").as_string()) ==
                std::string::npos,
            "HTTP 1803 projection does not echo its inline payload");

    tdx::Bytes queue_body(20012, 0);
    write_u32(queue_body, 8, 1);
    write_u32(queue_body, 12, 123);
    auto queue_request = tdx::Json::object();
    queue_request["format"] = "sdk-18031-queue-record-projection";
    queue_request["market"] = 0;
    queue_request["code"] = "000001";
    queue_request["payload_hex"] = payload_hex(queue_body);
    const auto queue =
        tdx::server_detail::query_level2_project(queue_request);
    require(queue.at("schema").as_string() ==
                "tdx-level2-sdk-18031-queue-record-projection-v1" &&
                queue.at("format").as_string() ==
                    "sdk-18031-queue-record-projection" &&
                queue.at("input_size").as_number() == 20012.0 &&
                queue.at("projected_record_size").as_number() == 6.0 &&
                queue.at("projected_state").at("first_raw")
                        .at("count_raw").as_number() == 1.0 &&
                queue.at("projected_state").at("first_raw")
                        .at("projected_record_count").as_number() == 1.0 &&
                !queue.at("input_retained").as_bool() &&
                !queue.at("input_body_retained").as_bool() &&
                !queue.at("file_accessed").as_bool() &&
                !queue.at("sdk_called").as_bool() &&
                !queue.at("host_state_write_performed").as_bool() &&
                queue.at("network_requests").as_number() == 0.0 &&
                !queue.at("request_sent").as_bool() &&
                !queue.at("entitlement_bypass").as_bool(),
            "SDK 18031 HTTP queue record projection stays offline");
    require(queue.dump(-1).find(
                queue_request.at("payload_hex").as_string()) ==
                std::string::npos,
            "HTTP 18031 projection does not echo its inline payload");

    for (const auto size : {32015U, 32017U}) {
        auto invalid = depth_request;
        invalid["payload_hex"] = std::string(size * 2U, '0');
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(invalid);
        }, "exactly 32016");
    }
    for (const auto size : {20011U, 20013U}) {
        auto invalid = queue_request;
        invalid["payload_hex"] = std::string(size * 2U, '0');
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(invalid);
        }, "exactly 20012");
    }
    for (const auto* forbidden : {
             "market", "code", "path", "url", "file", "input",
             "document", "token", "handle", "encoding", "previous",
             "context", "endpoint"}) {
        auto invalid = depth_request;
        invalid[forbidden] = "forbidden";
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(invalid);
        }, std::string("does not accept field: ") + forbidden);
    }
    for (const auto* forbidden : {
             "path", "url", "file", "input", "document", "token",
             "handle", "encoding", "previous", "context", "endpoint"}) {
        auto invalid = queue_request;
        invalid[forbidden] = "forbidden";
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(invalid);
        }, std::string("does not accept field: ") + forbidden);
    }
    for (const auto* required : {"market", "code", "payload_hex"}) {
        auto invalid = queue_request;
        invalid.as_object().erase(required);
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(invalid);
        }, std::string(required) + " is required");
    }
}

tdx::Bytes qualified_4651_decoded(std::size_t size = 8) {
    tdx::Bytes result(size, 0x41);
    write_u32(result, 2, 0xffffffffU);
    return result;
}

tdx::Bytes qualified_4655_raw() {
    tdx::Bytes result(57, 0);
    result[28] = 0;
    result[29] = 1;
    result[30] = 0;
    return result;
}

void require_dual_project_offline(const tdx::Json& result,
                                  std::size_t first_size,
                                  std::size_t raw_size,
                                  const char* first_name) {
    require(result.at("offline").as_bool() &&
                result.at("input_encoding").as_string() == "hex" &&
                result.at("input_size").as_number() ==
                    static_cast<double>(first_size + raw_size) &&
                result.at(std::string(first_name) + "_input_size").as_number() ==
                    static_cast<double>(first_size) &&
                result.at("raw_input_size").as_number() ==
                    static_cast<double>(raw_size) &&
                !result.at("input_retained").as_bool() &&
                !result.at("input_body_retained").as_bool() &&
                !result.at("file_accessed").as_bool() &&
                !result.at("sdk_called").as_bool() &&
                !result.at("sdk_callback_executed").as_bool() &&
                !result.at("host_message_dispatch_attempted").as_bool() &&
                result.at("network_requests").as_number() == 0.0 &&
                !result.at("request_sent").as_bool() &&
                !result.at("subscription_sent").as_bool() &&
                !result.at("entitlement_bypass").as_bool(),
            "dual snapshot HTTP project remains offline and non-retaining");
}

void test_4654_4651_4655_project_http_contracts() {
    auto request4654 = tdx::Json::object();
    request4654["format"] = "sdk-4654-dual-snapshot-transition";
    request4654["decoded_hex"] = payload_hex(tdx::Bytes(48, 0x54));
    request4654["raw_hex"] = payload_hex(tdx::Bytes(12, 0xa4));
    const auto result4654 =
        tdx::server_detail::query_level2_project(request4654);
    require(result4654.at("schema").as_string() ==
                "tdx-level2-sdk-4654-dual-snapshot-transition-v1" &&
                result4654.at("replacement").at("decision").as_string() ==
                    "replace" &&
                result4654.at("projected_state").at("ready").as_bool(),
            "4654 HTTP surface projects the unconditional dual replacement");
    require_dual_project_offline(result4654, 48, 12, "decoded");

    auto request4651 = tdx::Json::object();
    request4651["format"] = "sdk-4651-dual-snapshot-transition";
    request4651["decoded_hex"] = payload_hex(qualified_4651_decoded());
    request4651["raw_hex"] = payload_hex(tdx::Bytes(7, 0x51));
    const auto first4651 =
        tdx::server_detail::query_level2_project(request4651);
    require(first4651.at("schema").as_string() ==
                "tdx-level2-sdk-4651-dual-snapshot-transition-v1" &&
                first4651.at("decision").at("replaced").as_bool() &&
                !first4651.at("previous_input_retained").as_bool(),
            "4651 HTTP surface replaces an absent previous state");
    require_dual_project_offline(first4651, 8, 7, "decoded");

    auto retained_request = request4651;
    retained_request["raw_hex"] = payload_hex(tdx::Bytes(6, 0x52));
    retained_request["previous"] = first4651.at("projected_state");
    const auto retained =
        tdx::server_detail::query_level2_project(retained_request);
    require(retained.at("decision").at("retained").as_bool() &&
                retained.at("projected_state").at("raw_byte_size")
                        .as_number() == 7.0 &&
                !retained.at("previous_input_retained").as_bool(),
            "4651 HTTP surface consumes projected_state without echoing it");
    require_dual_project_offline(retained, 8, 6, "decoded");

    auto request4655 = tdx::Json::object();
    request4655["format"] = "sdk-4655-companion-raw-transition";
    request4655["companion_hex"] = payload_hex(tdx::Bytes(46, 0x55));
    request4655["raw_hex"] = payload_hex(qualified_4655_raw());
    const auto result4655 =
        tdx::server_detail::query_level2_project(request4655);
    require(result4655.at("schema").as_string() ==
                "tdx-level2-sdk-4655-companion-raw-transition-v1" &&
                result4655.at("dispatcher_qualified").as_bool() &&
                !result4655.at("host_state_gate").at("evaluated").as_bool() &&
                !result4655.at("actual_host_replacement_claimed").as_bool(),
            "4655 HTTP surface preserves the unresolved host-state gate");
    require_dual_project_offline(result4655, 46, 57, "companion");

    for (const auto* forbidden : {
             "path", "url", "file", "input", "payload_hex", "document",
             "token", "handle", "encoding", "endpoint", "callback_key"}) {
        auto invalid = request4654;
        invalid[forbidden] = "forbidden";
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(invalid);
        }, std::string("does not accept field: ") + forbidden);
    }

    for (const auto size : {47U, 49U}) {
        auto invalid = request4654;
        invalid["decoded_hex"] = payload_hex(tdx::Bytes(size, 0));
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(invalid);
        }, "exactly 48");
    }
    auto invalid_gate = request4651;
    invalid_gate["decoded_hex"] = payload_hex(tdx::Bytes(8, 0));
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(invalid_gate);
    }, "offset 2 must be 0xffffffff");
    auto invalid_previous = retained_request;
    invalid_previous["previous"]["unexpected"] = 1;
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(invalid_previous);
    }, "does not accept field: unexpected");
    auto emitted_previous = retained_request;
    emitted_previous["previous"]["bodies_emitted"] = true;
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(emitted_previous);
    }, "bodies_emitted must be false");
    auto short_companion = request4655;
    short_companion["companion_hex"] = payload_hex(tdx::Bytes(45, 0));
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(short_companion);
    }, "exactly 46");
    auto bad_shape = request4655;
    bad_shape["raw_hex"] = payload_hex(tdx::Bytes(56, 0));
    require_error([&] {
        (void)tdx::server_detail::query_level2_project(bad_shape);
    }, "at least 57");

    for (const auto* field : {"decoded_hex", "raw_hex"}) {
        auto missing = request4654;
        missing.as_object().erase(field);
        require_error([&] {
            (void)tdx::server_detail::query_level2_project(missing);
        }, std::string(field) + " is required");
    }

    const auto rendered = result4654.dump(-1) + first4651.dump(-1) +
                          result4655.dump(-1);
    require(rendered.find("decoded_hex") == std::string::npos &&
                rendered.find("companion_hex") == std::string::npos &&
                rendered.find("raw_hex") == std::string::npos,
            "dual snapshot HTTP results never echo inline body fields");
}

}  // namespace

int main() {
    try {
        test_happy_paths_and_non_retention();
        test_strict_request_contract();
        test_1801_1802_host_projection_http_contract();
        test_1804_host_projection_http_contract();
        test_1803_18031_record_projection_http_contract();
        test_4654_4651_4655_project_http_contracts();
        std::cout << "server Level2 project API tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "server Level2 project API tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
