#include "server_level2_internal.hpp"
#include "server_catalog_internal.hpp"

#include "tdx/common.hpp"

#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_error(const std::function<void()>& action,
                   const std::string& expected) {
    try {
        action();
    } catch (const tdx::Error& error) {
        if (std::string(error.what()).find(expected) != std::string::npos)
            return;
        throw tdx::Error("unexpected Level2 API error: " +
                         std::string(error.what()));
    }
    throw tdx::Error("expected Level2 API error containing: " + expected);
}

}  // namespace

int main() {
    try {
        auto direct_request = tdx::Json::object();
        direct_request["format"] = "direct";
        direct_request["market"] = 0;
        direct_request["code"] = "000001";
        direct_request["kind"] = "transaction";
        direct_request["cursor"] = 0x11223344;
        direct_request["count"] = 1500;
        const auto direct = tdx::server_detail::query_level2_build(
            direct_request);
        require(direct.at("schema").as_string() ==
                    "tdx-level2-direct-request-v1" &&
                direct.at("payload_hex").as_string() ==
                    "000000000000100010005405000030303030303144332211dc05" &&
                direct.at("variant").as_string() == "standard" &&
                !direct.at("initial").as_bool() &&
                direct.at("command").as_number() == 1364.0 &&
                direct.at("size").as_number() == 26.0 &&
                direct.at("network_requests").as_number() == 0.0 &&
                !direct.at("subscription_sent").as_bool(),
                "offline direct request API contract");

        auto initial_direct_request = direct_request;
        initial_direct_request["initial"] = true;
        const auto initial_direct = tdx::server_detail::query_level2_build(
            initial_direct_request);
        require(initial_direct.at("payload_hex").as_string() ==
                    "000000000000100010005305000030303030303144332211dc05" &&
                initial_direct.at("variant").as_string() == "initial" &&
                initial_direct.at("initial").as_bool() &&
                initial_direct.at("command").as_number() == 1363.0,
                "offline initial direct request API contract");

        auto sdk_request = tdx::Json::object();
        sdk_request["format"] = "sdk-4655";
        sdk_request["market"] = 0;
        sdk_request["code"] = "000001";
        sdk_request["want_number"] = 501;
        sdk_request["attach_info"] = true;
        const auto sdk = tdx::server_detail::query_level2_build(sdk_request);
        require(sdk.at("schema").as_string() ==
                    "tdx-level2-sdk-redirect-request-v1" &&
                sdk.at("function_id").as_number() == 4655.0 &&
                sdk.at("want_number_effective").as_number() == 80.0 &&
                sdk.at("size").as_number() == 46.0,
                "offline SDK request API contract");

        auto tdxw_1369_request = tdx::Json::object();
        tdxw_1369_request["format"] = "tdxw-1369";
        tdxw_1369_request["market"] = 0;
        tdxw_1369_request["code"] = "000001";
        tdxw_1369_request["count"] = 11;
        const auto tdxw_1369 = tdx::server_detail::query_level2_build(
            tdxw_1369_request);
        require(tdxw_1369.at("schema").as_string() ==
                    "tdx-level2-tdxw-ipc-request-v1" &&
                tdxw_1369.at("command").as_number() == 1369.0 &&
                tdxw_1369.at("data_type").as_number() == 1803.0 &&
                tdxw_1369.at("request_count").as_number() == 11.0 &&
                tdxw_1369.at("payload_hex").as_string() ==
                    "59050000303030303031000000000000000000000000000000000000000000000000000000010b00" &&
                tdxw_1369.at("size").as_number() == 40.0 &&
                !tdxw_1369.at("network_request_bytes").as_bool() &&
                !tdxw_1369.at("request_sent").as_bool(),
                "TdxW 1369 internal IPC API contract");

        auto tdxw_1371_request = tdx::Json::object();
        tdxw_1371_request["format"] = "tdxw-1371";
        tdxw_1371_request["market"] = 1;
        tdxw_1371_request["code"] = "600000";
        tdxw_1371_request["side_mode_raw"] = 1;
        tdxw_1371_request["selected_price"] = 10.5;
        tdxw_1371_request["cursor"] = -1;
        tdxw_1371_request["count"] = 5000;
        const auto tdxw_1371 = tdx::server_detail::query_level2_build(
            tdxw_1371_request);
        require(tdxw_1371.at("schema").as_string() ==
                    "tdx-level2-tdxw-ipc-request-v1" &&
                tdxw_1371.at("command").as_number() == 1371.0 &&
                tdxw_1371.at("data_type").as_number() == 18031.0 &&
                tdxw_1371.at("side_mode_raw").as_number() == 1.0 &&
                tdxw_1371.at("cursor").as_number() == -1.0 &&
                tdxw_1371.at("payload_hex").as_string() ==
                    "5b0501003630303030300000000000000000000000000000000000000000000000000000000100002841ffffffff8813" &&
                tdxw_1371.at("size").as_number() == 48.0,
                "TdxW 1371 internal IPC API contract");

        auto wrong_tdxw_field = tdxw_1369_request;
        wrong_tdxw_field["selected_price"] = 10.5;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(wrong_tdxw_field);
        }, "does not accept field: selected_price");

        auto invalid_tdxw_market = tdxw_1369_request;
        invalid_tdxw_market["market"] = 3;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(invalid_tdxw_market);
        }, "integer in 0..2");

        auto invalid_tdxw_count = tdxw_1371_request;
        invalid_tdxw_count["count"] = 5001;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(invalid_tdxw_count);
        }, "integer in 1..5000");

        auto invalid_tdxw_price = tdxw_1371_request;
        invalid_tdxw_price["selected_price"] =
            std::numeric_limits<double>::max();
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(invalid_tdxw_price);
        }, "positive float32");

        auto plan_request = tdx::Json::object();
        plan_request["format"] = "sdk-1807-plan";
        plan_request["payload_hex"] = "00303030303031";
        const auto plan = tdx::server_detail::query_level2_build(plan_request);
        require(plan.at("schema").as_string() ==
                    "tdx-level2-sdk-1807-request-plan-v1" &&
                plan.at("input").at("record_count").as_number() == 1.0 &&
                plan.at("input_size").as_number() == 7.0 &&
                !plan.at("file_accessed").as_bool() &&
                plan.at("network_requests").as_number() == 0.0 &&
                !plan.at("subscription_sent").as_bool(),
                "offline SDK 1807 plan API contract");

        auto batch_document = tdx::Json::object();
        batch_document["data_type"] = 1802;
        batch_document["symbols"] = tdx::Json::array();
        batch_document["symbols"].push_back("SZ000001");
        batch_document["symbols"].push_back("BJ430001");
        auto batch_request = tdx::Json::object();
        batch_request["format"] = "sdk-fnsubscribe-batch-plan";
        batch_request["document"] = batch_document;
        const auto batch = tdx::server_detail::query_level2_build(
            batch_request);
        require(batch.at("schema").as_string() ==
                    "tdx-level2-sdk-fnsubscribe-batch-call-plan-v1" &&
                batch.at("data_type").as_number() == 1802.0 &&
                batch.at("logical_arguments").at("symbol_list").as_string() ==
                    "SZ000001,BJ430001" &&
                batch.at("sdk_call").at("raw_abi_slots").size() == 6 &&
                batch.at("callback_correlation")
                        .at("registry_mode_raw").as_number() == 9.0 &&
                !batch.at("callback_correlation").at("ready").as_bool() &&
                !batch.at("sdk_call").at("invoked").as_bool() &&
                !batch.at("host_view_replayed").as_bool() &&
                !batch.at("security_resolver_invoked").as_bool() &&
                !batch.at("wire_bytes_built").as_bool() &&
                !batch.at("request_sent").as_bool() &&
                !batch.at("subscription_sent").as_bool() &&
                batch.at("input_encoding").as_string() == "inline-json" &&
                batch.at("input_size_limit").as_number() == 16.0 * 1024.0 &&
                !batch.at("input_retained").as_bool() &&
                !batch.at("file_accessed").as_bool(),
                "inline fnSubscribeData batch plan API contract");

        auto maximum_batch = batch_request;
        maximum_batch["document"]["symbols"] = tdx::Json::array();
        for (int index = 0; index < 100; ++index)
            maximum_batch["document"]["symbols"].push_back("SH600000");
        require(tdx::server_detail::query_level2_build(maximum_batch)
                    .at("symbol_count").as_number() == 100.0,
                "fnSubscribeData API accepts the exact 100-symbol bound");
        maximum_batch["document"]["symbols"].push_back("SH600000");
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(maximum_batch);
        }, "1..100");

        for (const auto* forbidden : {"input", "path", "url", "file"}) {
            auto request = batch_request;
            request[forbidden] = "forbidden";
            require_error([&] {
                (void)tdx::server_detail::query_level2_build(request);
            }, std::string("does not accept field: ") + forbidden);

            auto nested = batch_request;
            nested["document"][forbidden] = "forbidden";
            require_error([&] {
                (void)tdx::server_detail::query_level2_build(nested);
            }, std::string("does not accept field: ") + forbidden);
        }
        auto extra_batch_document = batch_request;
        extra_batch_document["document"]["extra"] = true;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(extra_batch_document);
        }, "does not accept field: extra");
        auto missing_batch_document = tdx::Json::object();
        missing_batch_document["format"] = "sdk-fnsubscribe-batch-plan";
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                missing_batch_document);
        }, "document is required");
        auto non_object_batch_document = batch_request;
        non_object_batch_document["document"] = tdx::Json::array();
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                non_object_batch_document);
        }, "inline JSON object");
        auto batch_1807 = batch_request;
        batch_1807["document"]["data_type"] = 1807;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(batch_1807);
        }, "sdk-1807-plan");
        auto invalid_batch_symbol = batch_request;
        invalid_batch_symbol["document"]["symbols"].as_array()[0] =
            "sz000001";
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                invalid_batch_symbol);
        }, "uppercase");
        auto invalid_batch_utf8 = batch_request;
        invalid_batch_utf8["document"]["symbols"].as_array()[0] =
            std::string("\xc0\xaf", 2);
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(invalid_batch_utf8);
        }, "valid UTF-8");
        auto empty_batch = batch_request;
        empty_batch["document"]["symbols"] = tdx::Json::array();
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(empty_batch);
        }, "1..100");
        auto oversized_batch = batch_request;
        oversized_batch["document"]["symbols"] = tdx::Json::array();
        oversized_batch["document"]["symbols"].push_back(
            std::string("SZ") + std::string(16U * 1024U, '0'));
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(oversized_batch);
        }, "16 KiB");

        auto fnreq_request = tdx::Json::object();
        fnreq_request["format"] = "sdk-fnreqdata-plan";
        fnreq_request["market"] = 1;
        fnreq_request["code"] = "600000";
        fnreq_request["data_type"] = 1801;
        fnreq_request["cursor_raw"] = 4294967295.0;
        fnreq_request["count"] = 1500;
        const auto fnreq =
            tdx::server_detail::query_level2_build(fnreq_request);
        require(fnreq.at("schema").as_string() ==
                    "tdx-level2-sdk-fnreqdata-call-plan-v1" &&
                fnreq.at("format").as_string() == "sdk-fnreqdata-plan" &&
                fnreq.at("logical_arguments").at("cursor_raw").as_number() ==
                    4294967295.0 &&
                fnreq.at("logical_arguments").at("request_count").as_number() ==
                    1500.0 &&
                fnreq.at("sdk_call").at("raw_abi_slots").size() == 12 &&
                !fnreq.at("sdk_call").at("invoked").as_bool() &&
                !fnreq.at("wire_bytes_built").as_bool() &&
                !fnreq.at("request_sent").as_bool() &&
                !fnreq.at("subscription_sent").as_bool() &&
                !fnreq.at("input_retained").as_bool() &&
                !fnreq.at("file_accessed").as_bool(),
                "inline fnReqData 1801 plan preserves u32 cursor and remains offline");

        auto fixed_fnreq = tdx::Json::object();
        fixed_fnreq["format"] = "sdk-fnreqdata-plan";
        fixed_fnreq["market"] = 0;
        fixed_fnreq["code"] = "000001";
        fixed_fnreq["data_type"] = 1803;
        const auto fixed_plan =
            tdx::server_detail::query_level2_build(fixed_fnreq);
        require(fixed_plan.at("window_arguments").as_string() ==
                    "fixed-0-and-1" &&
                fixed_plan.at("logical_arguments").at("cursor_raw").as_number() ==
                    0.0 &&
                fixed_plan.at("logical_arguments").at("request_count").as_number() ==
                    1.0,
                "fixed fnReqData type uses the recovered cursor/count constants");

        auto missing_fnreq_window = fnreq_request;
        missing_fnreq_window.as_object().erase("cursor_raw");
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                missing_fnreq_window);
        }, "require explicit cursor-raw and request-count");
        auto fixed_fnreq_override = fixed_fnreq;
        fixed_fnreq_override["cursor_raw"] = 0;
        fixed_fnreq_override["count"] = 1;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                fixed_fnreq_override);
        }, "fixed cursor 0");
        auto unknown_fnreq_type = fixed_fnreq;
        unknown_fnreq_type["data_type"] = 1807;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                unknown_fnreq_type);
        }, "1801, 1802, 1803, 1804, or 18071");

        auto fnreq_18031_request = tdx::Json::object();
        fnreq_18031_request["format"] = "sdk-fnreqdata-18031-plan";
        fnreq_18031_request["market"] = 0;
        fnreq_18031_request["code"] = "000001";
        fnreq_18031_request["side_mode_raw"] = 255;
        fnreq_18031_request["selected_price"] = 10.25;
        const auto fnreq_18031 =
            tdx::server_detail::query_level2_build(fnreq_18031_request);
        require(fnreq_18031.at("schema").as_string() ==
                    "tdx-level2-sdk-fnreqdata-18031-call-plan-v1" &&
                fnreq_18031.at("sdk_selector_raw").as_number() == 1.0 &&
                fnreq_18031.at("logical_arguments")
                        .at("selected_price_f64_bits_hex").as_string() ==
                    "0x4024800000000000" &&
                fnreq_18031.at("sdk_call").at("raw_abi_slots").size() == 12 &&
                !fnreq_18031.at("callback_correlation")
                    .at("uses_25_byte_local_registry").as_bool() &&
                !fnreq_18031.at("operation_executed").as_bool() &&
                !fnreq_18031.at("input_retained").as_bool() &&
                !fnreq_18031.at("file_accessed").as_bool(),
                "inline fnReqData 18031 plan preserves selector and promoted f32 bits");

        auto invalid_fnreq_price = fnreq_18031_request;
        invalid_fnreq_price["selected_price"] = 3.5e38;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(invalid_fnreq_price);
        }, "finite float32");
        auto invalid_fnreq_side = fnreq_18031_request;
        invalid_fnreq_side["side_mode_raw"] = 256;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(invalid_fnreq_side);
        }, "integer in 0..255");
        for (const auto* forbidden : {"input", "path", "url", "file"}) {
            auto request = fnreq_request;
            request[forbidden] = "forbidden";
            require_error([&] {
                (void)tdx::server_detail::query_level2_build(request);
            }, std::string("does not accept field: ") + forbidden);

            auto request_18031 = fnreq_18031_request;
            request_18031[forbidden] = "forbidden";
            require_error([&] {
                (void)tdx::server_detail::query_level2_build(request_18031);
            }, std::string("does not accept field: ") + forbidden);
        }

        auto callback_route_request = tdx::Json::object();
        callback_route_request["format"] = "sdk-callback-route-plan";
        callback_route_request["data_type"] = 1801;
        callback_route_request["registry_mode_raw"] = 9;
        const auto callback_route =
            tdx::server_detail::query_level2_build(callback_route_request);
        require(callback_route.at("schema").as_string() ==
                    "tdx-level2-sdk-callback-route-plan-v1" &&
                callback_route.at("format").as_string() ==
                    "sdk-callback-route-plan" &&
                callback_route.at("route_count").as_number() == 1.0 &&
                callback_route.at("routes").as_array()[0]
                    .at("message_id_raw").as_number() == 0x54e &&
                callback_route.at("routes").as_array()[0]
                    .at("api").as_string() == "SendMessageA" &&
                !callback_route.at("host_message_dispatch_attempted").as_bool() &&
                callback_route.at("host_messages_sent").as_number() == 0.0 &&
                !callback_route.at("wire_bytes_built").as_bool() &&
                callback_route.at("network_requests").as_number() == 0.0 &&
                !callback_route.at("request_sent").as_bool() &&
                !callback_route.at("subscription_sent").as_bool() &&
                !callback_route.at("input_retained").as_bool() &&
                !callback_route.at("file_accessed").as_bool() &&
                callback_route.at("offline").as_bool(),
                "inline SDK callback route plan API remains offline");
        auto callback_maximum_mode = callback_route_request;
        callback_maximum_mode["registry_mode_raw"] = 4294967295.0;
        require(tdx::server_detail::query_level2_build(callback_maximum_mode)
                    .at("callback_correlation")
                    .at("registry_mode_raw").as_number() == 4294967295.0,
                "callback route API accepts and preserves the u32 mode maximum");
        auto callback_oversized_mode = callback_route_request;
        callback_oversized_mode["registry_mode_raw"] = 4294967296.0;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                callback_oversized_mode);
        }, "integer in 0..4294967295");
        auto callback_fractional_mode = callback_route_request;
        callback_fractional_mode["registry_mode_raw"] = 8.5;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                callback_fractional_mode);
        }, "integer in 0..4294967295");

        auto callback_18031 = tdx::Json::object();
        callback_18031["format"] = "sdk-callback-route-plan";
        callback_18031["data_type"] = 18031;
        const auto global_route =
            tdx::server_detail::query_level2_build(callback_18031);
        require(global_route.at("routes").as_array()[0]
                    .at("message_id_raw").as_number() == 0x551 &&
                global_route.at("routes").as_array()[0]
                    .at("lparam").at("raw_value").as_number() == 2.0 &&
                !global_route.at("callback_correlation")
                    .at("uses_25_byte_local_registry").as_bool(),
                "18031 callback route uses host-global correlation without registry mode");

        auto callback_1807 = tdx::Json::object();
        callback_1807["format"] = "sdk-callback-route-plan";
        callback_1807["data_type"] = 1807;
        callback_1807["registry_mode_raw"] = 9;
        const auto unresolved_time_route =
            tdx::server_detail::query_level2_build(callback_1807);
        require(unresolved_time_route.at("route_count").as_number() == 3.0 &&
                !unresolved_time_route.at("host_time_advanced_branch")
                    .at("resolved").as_bool() &&
                unresolved_time_route.at("host_time_advanced_branch")
                    .at("conditional_alternatives_preserved").as_bool(),
                "omitted 1807 time branch preserves all three callback routes");
        callback_1807["host_time_advanced"] = false;
        require(tdx::server_detail::query_level2_build(callback_1807)
                    .at("route_count").as_number() == 1.0,
                "false 1807 time branch selects the ordinary route");
        callback_1807["host_time_advanced"] = true;
        require(tdx::server_detail::query_level2_build(callback_1807)
                    .at("route_count").as_number() == 2.0,
                "true 1807 time branch selects both synchronous routes");

        for (const auto* forbidden : {"input", "path", "url", "file"}) {
            auto request = callback_route_request;
            request[forbidden] = "forbidden";
            require_error([&] {
                (void)tdx::server_detail::query_level2_build(request);
            }, std::string("does not accept field: ") + forbidden);
        }
        auto callback_missing_mode = callback_route_request;
        callback_missing_mode.as_object().erase("registry_mode_raw");
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(callback_missing_mode);
        }, "requires registry-mode-raw");
        auto callback_18031_with_mode = callback_18031;
        callback_18031_with_mode["registry_mode_raw"] = 9;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                callback_18031_with_mode);
        }, "does not use registry-mode-raw");
        auto callback_wrong_time_type = callback_1807;
        callback_wrong_time_type["host_time_advanced"] = "true";
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                callback_wrong_time_type);
        }, "must be boolean");
        auto callback_time_on_1801 = callback_route_request;
        callback_time_on_1801["host_time_advanced"] = true;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                callback_time_on_1801);
        }, "only valid for SDK callback route 1807");
        auto callback_invalid_type = callback_route_request;
        callback_invalid_type["data_type"] = 1805;
        require_error([&] {
            (void)tdx::server_detail::query_level2_build(
                callback_invalid_type);
        }, "data_type must be 1801, 1802, 1803, 1804, 1807, 18031, or 18071");

        struct CallbackInvocationCase {
            int data_type;
            std::size_t body_size;
            const char* decoded_schema;
            std::uint32_t first_message;
            std::size_t route_count;
            bool uses_local_registry;
        };
        constexpr std::array<CallbackInvocationCase, 7>
            callback_invocation_cases{{
                {1801, 52, "tdx-level2-sdk-1801-v1", 0x54e, 1, true},
                {1802, 40, "tdx-level2-sdk-1802-v1", 0x54f, 1, true},
                {1803, 32016, "tdx-level2-sdk-1803-v1", 0x551, 1, true},
                {1804, 432, "tdx-level2-sdk-1804-v1", 0x54d, 1, true},
                {1807, 380, "tdx-level2-sdk-1807-v1", 0x54d, 3, true},
                {18031, 20012, "tdx-level2-sdk-18031-v1", 0x551, 1, false},
                {18071, 380, "tdx-level2-sdk-18071-v1", 0x54d, 1, true},
            }};
        const auto callback_invocation_request = [](
            const CallbackInvocationCase& fixture, std::size_t body_size) {
            auto request = tdx::Json::object();
            request["format"] = "sdk-callback-invocation";
            request["data_type"] = fixture.data_type;
            request["callback_arg5_raw"] =
                fixture.data_type == 1801 || fixture.data_type == 1802
                    ? 1.0
                    : static_cast<double>(
                          std::numeric_limits<std::int32_t>::min());
            request["callback_arg6_raw"] = 4294967295.0;
            if (fixture.uses_local_registry)
                request["registry_mode_raw"] = 9;
            request["payload_hex"] = std::string(body_size * 2, '0');
            request["limit"] = 1;
            return request;
        };

        for (const auto& fixture : callback_invocation_cases) {
            const auto invocation = tdx::server_detail::query_level2_decode(
                callback_invocation_request(fixture, fixture.body_size));
            require(invocation.at("schema").as_string() ==
                        "tdx-level2-sdk-callback-invocation-v1" &&
                    invocation.at("data_type").as_number() ==
                        fixture.data_type &&
                    invocation.at("decoded_document").at("schema")
                        .as_string() == fixture.decoded_schema &&
                    invocation.at("body_contract").at("exact_size_validated")
                        .as_bool() &&
                    invocation.at("route_plan").at("route_count")
                        .as_number() == fixture.route_count &&
                    invocation.at("route_plan").at("routes").as_array()[0]
                        .at("message_id_raw").as_number() ==
                            fixture.first_message &&
                    invocation.at("callback_correlation")
                        .at("uses_25_byte_local_registry").as_bool() ==
                            fixture.uses_local_registry &&
                    invocation.at("callback_arguments")
                        .at("callback_arg6_raw").as_number() ==
                            4294967295.0 &&
                    invocation.at("input_encoding").as_string() == "hex" &&
                    invocation.at("input_size").as_number() ==
                        fixture.body_size &&
                    invocation.at("input_size_limit").as_number() ==
                        384.0 * 1024.0 &&
                    !invocation.at("input_retained").as_bool() &&
                    !invocation.at("file_accessed").as_bool() &&
                    !invocation.at("callback_executed").as_bool() &&
                    !invocation.at("sdk_called").as_bool() &&
                    !invocation.at("host_message_dispatch_attempted")
                        .as_bool() &&
                    invocation.at("host_messages_sent").as_number() == 0.0 &&
                    invocation.at("network_requests").as_number() == 0.0 &&
                    !invocation.at("request_sent").as_bool() &&
                    !invocation.at("subscription_sent").as_bool() &&
                    invocation.at("offline").as_bool() &&
                    invocation.as_object().count("payload_hex") == 0 &&
                    invocation.at("input").as_object().count("payload_hex") == 0,
                    "SDK callback invocation HTTP contract is typed, offline, and non-retaining");
            if (fixture.data_type == 1807) {
                require(!invocation.at("route_plan")
                            .at("host_time_advanced_branch")
                            .at("resolved").as_bool() &&
                            invocation.at("route_plan")
                                .at("host_time_advanced_branch")
                                .at("conditional_alternatives_preserved")
                                .as_bool(),
                        "1807 callback invocation preserves all unresolved host-time routes");
            }
            if (fixture.data_type != 1801 && fixture.data_type != 1802) {
                require(!invocation.at("callback_arguments")
                            .at("callback_arg5_consumed_as_record_count")
                            .as_bool() &&
                            invocation.at("callback_arguments")
                                .at("callback_arg5_raw").as_number() ==
                                static_cast<double>(
                                    std::numeric_limits<std::int32_t>::min()),
                        "fixed callback invocation preserves arg5 without count semantics");
            }
        }

        auto callback_count_mismatch = callback_invocation_request(
            callback_invocation_cases[0], 52);
        callback_count_mismatch["callback_arg5_raw"] = 2;
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                callback_count_mismatch);
        }, "arg5 count requires 104 body bytes");
        for (const int invalid_count : {0, 7562}) {
            auto request = callback_invocation_request(
                callback_invocation_cases[0], 52);
            request["callback_arg5_raw"] = invalid_count;
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(request);
            }, "record count in 1..7561");
        }
        auto invalid_1802_count = callback_invocation_request(
            callback_invocation_cases[1], 40);
        invalid_1802_count["callback_arg5_raw"] = 9831;
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                invalid_1802_count);
        }, "record count in 1..9830");

        auto maximum_callback_count = callback_invocation_request(
            callback_invocation_cases[0], 7561U * 52U);
        maximum_callback_count["callback_arg5_raw"] = 7561;
        maximum_callback_count["limit"] = 0;
        const auto maximum_callback =
            tdx::server_detail::query_level2_decode(maximum_callback_count);
        require(maximum_callback.at("record_count").as_number() == 7561.0 &&
                    maximum_callback.at("input_size").as_number() ==
                        7561.0 * 52.0 &&
                    maximum_callback.at("decoded_document").at("records")
                        .size() == 0 &&
                    maximum_callback.at("decoded_document").at("truncated")
                        .as_bool(),
                "callback invocation accepts the maximum 1801 count under 384 KiB");

        for (std::size_t index = 2;
             index < callback_invocation_cases.size(); ++index) {
            const auto& fixture = callback_invocation_cases[index];
            for (const auto body_size : {
                     fixture.body_size - 1, fixture.body_size + 1}) {
                auto request = callback_invocation_request(
                    fixture, body_size);
                require_error([&] {
                    (void)tdx::server_detail::query_level2_decode(request);
                }, "requires exactly " + std::to_string(fixture.body_size) +
                       " body bytes");
            }
            auto doubled = callback_invocation_request(
                fixture, fixture.body_size * 2);
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(doubled);
            }, "requires exactly " + std::to_string(fixture.body_size) +
                   " body bytes");
        }

        auto invocation_18031_with_mode = callback_invocation_request(
            callback_invocation_cases[5], 20012);
        invocation_18031_with_mode["registry_mode_raw"] = 9;
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                invocation_18031_with_mode);
        }, "does not accept registry-mode-raw");
        auto invocation_missing_mode = callback_invocation_request(
            callback_invocation_cases[0], 52);
        invocation_missing_mode.as_object().erase("registry_mode_raw");
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                invocation_missing_mode);
        }, "requires registry-mode-raw except for 18031");

        for (const auto* forbidden : {
                 "input", "path", "url", "file", "document", "body",
                 "encoding", "endpoint", "xor_key", "max_fields",
                 "sample_bytes", "depth", "host_time_advanced", "market",
                 "code", "callback_key", "window_handle"}) {
            auto request = callback_invocation_request(
                callback_invocation_cases[0], 52);
            request[forbidden] = "forbidden";
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(request);
            }, std::string("does not accept field: ") + forbidden);
        }
        for (const auto* field : {
                 "callback_arg6_raw", "registry_mode_raw"}) {
            auto oversized = callback_invocation_request(
                callback_invocation_cases[0], 52);
            oversized[field] = 4294967296.0;
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(oversized);
            }, "integer in 0..4294967295");
            auto fractional = callback_invocation_request(
                callback_invocation_cases[0], 52);
            fractional[field] = 8.5;
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(fractional);
            }, "integer in 0..4294967295");
        }
        auto fractional_arg5 = callback_invocation_request(
            callback_invocation_cases[0], 52);
        fractional_arg5["callback_arg5_raw"] = 1.5;
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(fractional_arg5);
        }, "integer in -2147483648..2147483647");
        auto maximum_fixed_arg5 = callback_invocation_request(
            callback_invocation_cases[3], 432);
        maximum_fixed_arg5["callback_arg5_raw"] = 2147483647;
        require(tdx::server_detail::query_level2_decode(maximum_fixed_arg5)
                    .at("callback_arguments")
                    .at("callback_arg5_raw").as_number() == 2147483647.0,
                "fixed callback invocation preserves the int32 arg5 maximum");
        for (const auto* required : {
                 "data_type", "callback_arg5_raw", "callback_arg6_raw",
                 "payload_hex"}) {
            auto request = callback_invocation_request(
                callback_invocation_cases[0], 52);
            request.as_object().erase(required);
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(request);
            }, std::string(required) + " is required");
        }
        auto invalid_invocation_type = callback_invocation_request(
            callback_invocation_cases[0], 52);
        invalid_invocation_type["data_type"] = 1805;
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                invalid_invocation_type);
        }, "data_type must be 1801, 1802, 1803, 1804, 1807, 18031, or 18071");
        for (const int invalid_limit : {-1, 10001}) {
            auto request = callback_invocation_request(
                callback_invocation_cases[0], 52);
            request["limit"] = invalid_limit;
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(request);
            }, "integer in 0..10000");
        }

        auto decode_request = tdx::Json::object();
        decode_request["format"] = "sdk-1801";
        decode_request["payload_hex"] = std::string(52 * 2, '0');
        decode_request["limit"] = 1;
        const auto decoded = tdx::server_detail::query_level2_decode(
            decode_request);
        require(decoded.at("schema").as_string() ==
                    "tdx-level2-sdk-1801-v1" &&
                decoded.at("count").as_number() == 1.0 &&
                decoded.at("input_size").as_number() == 52.0 &&
                !decoded.at("input_retained").as_bool() &&
                !decoded.at("file_accessed").as_bool() &&
                decoded.at("offline").as_bool(),
                "inline hex decode API contract");

        auto tpbus_115_request = tdx::Json::object();
        tpbus_115_request["format"] = "tpbus-115";
        // outer len=7; discriminator=0 -> push 111; body len=1; body=aa.
        tpbus_115_request["payload_hex"] = "0700000000000100aa";
        tpbus_115_request["limit"] = 20;
        const auto tpbus_115 = tdx::server_detail::query_level2_decode(
            tpbus_115_request);
        const auto& tpbus_115_entry =
            tpbus_115.at("entries").as_array().front();
        require(tpbus_115.at("schema").as_string() ==
                    "tdx-level2-tpbus-115-batch-v1" &&
                tpbus_115.at("input_size").as_number() == 9.0 &&
                tpbus_115.at("inner").at("status").as_string() ==
                    "complete" &&
                tpbus_115_entry.at("mapped_push_type").as_number() ==
                    111.0 &&
                tpbus_115_entry.at("body_byte_size").as_number() == 1.0 &&
                tpbus_115_entry.at("body_sha256").as_string().size() == 64 &&
                !tpbus_115_entry.at("summary_available").as_bool() &&
                tpbus_115_entry.as_object().count("body") == 0 &&
                !tpbus_115.at("input_retained").as_bool() &&
                !tpbus_115.at("input_body_retained").as_bool() &&
                !tpbus_115.at("file_accessed").as_bool() &&
                !tpbus_115.at("event_bus_accessed").as_bool() &&
                !tpbus_115.at("sdk_called").as_bool() &&
                !tpbus_115.at("host_message_dispatch_attempted").as_bool() &&
                tpbus_115.at("network_requests").as_number() == 0.0 &&
                !tpbus_115.at("request_sent").as_bool() &&
                !tpbus_115.at("subscription_sent").as_bool() &&
                tpbus_115.at("offline").as_bool() &&
                !tpbus_115.at("entitlement_bypass").as_bool(),
                "tpbus-115 inline batch API stays bounded and offline");
        auto tpbus_115_path = tpbus_115_request;
        tpbus_115_path["path"] = "forbidden";
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(tpbus_115_path);
        }, "does not accept field: path");

        auto sdk_json_4653_request = tdx::Json::object();
        sdk_json_4653_request["format"] = "sdk-json-4653";
        sdk_json_4653_request["document"] = tdx::Json::parse(R"json({"Data":[
          {"datetime":"202608131459","closePrice":11248.459,
           "averagePrice":11229.999,"tradeVolume":4294967295,
           "reference_price":11200},
          {"datetime":"7","closePrice":-1250,
           "averagePrice":0,"tradeVolume":0}]})json");
        sdk_json_4653_request["limit"] = 1;
        const auto sdk_json_4653 = tdx::server_detail::query_level2_decode(
            sdk_json_4653_request);
        const auto& sdk_json_4653_header = sdk_json_4653.at("header");
        const auto& sdk_json_4653_record =
            sdk_json_4653.at("records").as_array().front();
        const auto expected_4653_close = static_cast<double>(
            static_cast<float>(11248.459) / 1000.0F);
        const auto expected_4653_average = static_cast<double>(
            static_cast<float>(11229.999) / 1000.0F);
        const auto expected_4653_reference = static_cast<double>(
            static_cast<float>(11200.0) / 1000.0F);
        require(sdk_json_4653.at("schema").as_string() ==
                    "tdx-level2-sdk-json-4653-v1" &&
                sdk_json_4653.at("format").as_string() == "sdk-json-4653" &&
                sdk_json_4653.at("function_id").as_number() == 4653.0 &&
                sdk_json_4653.at("native_header_size").as_number() == 35.0 &&
                sdk_json_4653.at("native_record_size").as_number() == 18.0 &&
                sdk_json_4653.at("count").as_number() == 2.0 &&
                sdk_json_4653.at("returned").as_number() == 1.0 &&
                sdk_json_4653.at("truncated").as_bool() &&
                sdk_json_4653.at("source_order").as_string() ==
                    "first-to-last" &&
                !sdk_json_4653.at("time_semantics_resolved").as_bool(),
                "SDK 4653 inline JSON schema and bounded source-order contract");
        require(sdk_json_4653_header.at("record_count_u16_raw").as_number() ==
                    2.0 &&
                sdk_json_4653_header.at("reference_price").as_number() ==
                    expected_4653_reference &&
                sdk_json_4653_record.at("source_index").as_number() == 0.0 &&
                sdk_json_4653_record.at("native_offset").as_number() == 35.0 &&
                sdk_json_4653_record.at("datetime_raw").as_string() ==
                    "202608131459" &&
                sdk_json_4653_record.at("datetime_right4_raw").as_string() ==
                    "1459" &&
                sdk_json_4653_record.at("time_u16_raw").as_number() ==
                    61.0 * 59.0 &&
                sdk_json_4653_record.at("close_price").as_number() ==
                    expected_4653_close &&
                sdk_json_4653_record.at("average_price").as_number() ==
                    expected_4653_average &&
                sdk_json_4653_record.at("trade_volume_u32_raw").as_number() ==
                    4294967295.0 &&
                sdk_json_4653_record.as_object().count("time") == 0 &&
                sdk_json_4653_record.as_object().count("time_seconds") == 0,
                "SDK 4653 surface preserves f32, u32, and opaque time fields");
        require(sdk_json_4653.at("input_encoding").as_string() ==
                    "inline-json" &&
                sdk_json_4653.at("input_size_limit").as_number() ==
                    384.0 * 1024.0 &&
                !sdk_json_4653.at("input_retained").as_bool() &&
                !sdk_json_4653.at("input_document_retained").as_bool() &&
                !sdk_json_4653.at("file_accessed").as_bool() &&
                !sdk_json_4653.at("file_retained").as_bool() &&
                !sdk_json_4653.at("source_file_retained").as_bool() &&
                !sdk_json_4653.at("sdk_called").as_bool() &&
                !sdk_json_4653.at("sdk_callback_invoked").as_bool() &&
                !sdk_json_4653.at("native_body_built").as_bool() &&
                !sdk_json_4653.at("wire_bytes_built").as_bool() &&
                !sdk_json_4653.at("network_request_bytes").as_bool() &&
                !sdk_json_4653.at("network_request_bytes_built").as_bool() &&
                sdk_json_4653.at("network_requests").as_number() == 0.0 &&
                !sdk_json_4653.at("request_built").as_bool() &&
                !sdk_json_4653.at("request_sent").as_bool() &&
                !sdk_json_4653.at("subscription_sent").as_bool() &&
                sdk_json_4653.at("offline").as_bool() &&
                !sdk_json_4653.at("entitlement_bypass").as_bool() &&
                sdk_json_4653.as_object().count("document") == 0 &&
                sdk_json_4653.as_object().count("native_body_hex") == 0,
                "SDK 4653 API does not retain input or manufacture side effects");

        auto sdk_json_4653_zero = sdk_json_4653_request;
        sdk_json_4653_zero["limit"] = 0;
        const auto sdk_json_4653_zero_result =
            tdx::server_detail::query_level2_decode(sdk_json_4653_zero);
        auto sdk_json_4653_maximum = sdk_json_4653_request;
        sdk_json_4653_maximum["limit"] = 10000;
        const auto sdk_json_4653_maximum_result =
            tdx::server_detail::query_level2_decode(sdk_json_4653_maximum);
        require(sdk_json_4653_zero_result.at("returned").as_number() == 0.0 &&
                    sdk_json_4653_zero_result.at("records").as_array().empty() &&
                    sdk_json_4653_zero_result.at("truncated").as_bool() &&
                    sdk_json_4653_maximum_result.at("returned").as_number() ==
                        2.0 &&
                    !sdk_json_4653_maximum_result.at("truncated").as_bool(),
                "SDK 4653 limit accepts the inclusive 0..10000 boundary");
        for (const int invalid_limit : {-1, 10001}) {
            auto request = sdk_json_4653_request;
            request["limit"] = invalid_limit;
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(request);
            }, "integer in 0..10000");
        }
        for (const auto* forbidden : {
                 "depth", "market", "code", "attach_info",
                 "repurchase_time", "input", "path", "url", "file",
                 "endpoint", "payload_hex", "body", "encoding", "xor_key",
                 "max_fields", "sample_bytes", "native_body"}) {
            auto request = sdk_json_4653_request;
            request[forbidden] = "forbidden";
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(request);
            }, std::string("does not accept field: ") + forbidden);
        }
        auto sdk_json_4653_wrong_shape = sdk_json_4653_request;
        sdk_json_4653_wrong_shape["document"] =
            tdx::Json::parse(R"json({"Data":{}})json");
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                sdk_json_4653_wrong_shape);
        }, "SDK 4653 Data must be an array");

        auto sized_4653_document = [](std::size_t datetime_size) {
            auto record = tdx::Json::object();
            record["datetime"] = std::string(datetime_size, '1');
            record["closePrice"] = 1;
            record["averagePrice"] = 1;
            record["tradeVolume"] = 1;
            record["reference_price"] = 1;
            auto data = tdx::Json::array();
            data.push_back(std::move(record));
            auto document = tdx::Json::object();
            document["Data"] = std::move(data);
            return document;
        };
        const auto empty_4653_document = sized_4653_document(0);
        const auto sized_4653_overhead =
            empty_4653_document.dump(-1).size();
        auto exact_4653_document = sized_4653_document(
            384U * 1024U - sized_4653_overhead);
        require(exact_4653_document.dump(-1).size() == 384U * 1024U,
                "SDK 4653 exact-size fixture");
        auto exact_4653_request = tdx::Json::object();
        exact_4653_request["format"] = "sdk-json-4653";
        exact_4653_request["document"] = exact_4653_document;
        exact_4653_request["limit"] = 0;
        const auto exact_4653_result =
            tdx::server_detail::query_level2_decode(exact_4653_request);
        require(exact_4653_result.at("input_size").as_number() ==
                    384.0 * 1024.0 &&
                exact_4653_result.at("input_compact_size").as_number() ==
                    384.0 * 1024.0 &&
                exact_4653_result.at("returned").as_number() == 0.0,
                "SDK 4653 accepts the inclusive 384 KiB API boundary");
        auto oversized_4653_request = exact_4653_request;
        oversized_4653_request["document"] = sized_4653_document(
            384U * 1024U - sized_4653_overhead + 1);
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                oversized_4653_request);
        }, "384 KiB");

        auto sdk_json_4655_request = tdx::Json::object();
        sdk_json_4655_request["format"] = "sdk-json-4655";
        sdk_json_4655_request["document"] = tdx::Json::parse(R"json({"Data":[
          {"transactionPrice":10.01,"transactionTime":9300100,
           "singleVolume":100,"transactionStatus":"B"},
          {"transactionPrice":10.02,"transactionTime":9300200,
           "singleVolume":200,"transactionStatus":"S"}]})json");
        sdk_json_4655_request["limit"] = 1;
        const auto sdk_json_4655 = tdx::server_detail::query_level2_decode(
            sdk_json_4655_request);
        require(sdk_json_4655.at("schema").as_string() ==
                    "tdx-level2-sdk-json-4655-v1" &&
                sdk_json_4655.at("records").as_array().front()
                        .at("side").as_string() == "sell" &&
                sdk_json_4655.at("records").as_array().front()
                        .at("time").as_string() == "09:30:02" &&
                sdk_json_4655.at("input_encoding").as_string() ==
                    "inline-json" &&
                sdk_json_4655.at("input_size_limit").as_number() ==
                    384.0 * 1024.0 &&
                !sdk_json_4655.at("input_retained").as_bool() &&
                !sdk_json_4655.at("file_accessed").as_bool() &&
                sdk_json_4655.at("network_requests").as_number() == 0.0 &&
                !sdk_json_4655.at("request_sent").as_bool() &&
                !sdk_json_4655.at("network_request_bytes").as_bool(),
                "SDK 4655 inline JSON API contract");

        auto sdk_json_4671_request = tdx::Json::object();
        sdk_json_4671_request["format"] = "sdk-json-4671";
        sdk_json_4671_request["document"] = tdx::Json::parse(R"json({"Data":{
          "buyList":[{"QUANTITY_":[100,250]}],
          "sellList":[{"QUANTITY_":[999]},{"QUANTITY_":[300,400]}],
          "padding":"secret-sdk-json-sentinel"}})json");
        sdk_json_4671_request["limit"] = 10;
        const auto sdk_json_4671 = tdx::server_detail::query_level2_decode(
            sdk_json_4671_request);
        require(sdk_json_4671.at("schema").as_string() ==
                    "tdx-level2-sdk-json-4671-v1" &&
                sdk_json_4671.at("buy_quantities_hands").as_array()[1]
                        .as_number() == 2.0 &&
                sdk_json_4671.at("sell_quantities_hands").as_array()[0]
                        .as_number() == 3.0 &&
                sdk_json_4671.dump(-1).find("secret-sdk-json-sentinel") ==
                    std::string::npos,
                "SDK 4671 inline JSON selection and non-retention contract");

        auto sdk_json_4680_request = tdx::Json::object();
        sdk_json_4680_request["format"] = "sdk-json-4680";
        sdk_json_4680_request["document"] = tdx::Json::parse(
            R"json({"Data":"{\"datetime\":\"20260801153000\",\"buyPrices\":[10.1,10.0],\"buyVolumes\":[1,2],\"sellPrices\":[10.2,10.3],\"sellVolumes\":[3,4]}"})json");
        sdk_json_4680_request["depth"] = 5;
        const auto sdk_json_4680 = tdx::server_detail::query_level2_decode(
            sdk_json_4680_request);
        require(sdk_json_4680.at("schema").as_string() ==
                    "tdx-level2-sdk-json-4680-v1" &&
                sdk_json_4680.at("requested_depth").as_number() == 5.0 &&
                sdk_json_4680.at("buy_levels").as_array().front()
                        .at("price").as_number() == 10.0 &&
                sdk_json_4680.at("sell_levels").as_array().front()
                        .at("volume_raw").as_number() == 3000.0,
                "SDK 4680 inline JSON depth and scaling contract");

        for (const auto* forbidden : {
                 "input", "path", "url", "file", "endpoint", "payload_hex",
                 "encoding", "xor_key", "max_fields", "sample_bytes"}) {
            auto request = sdk_json_4655_request;
            request[forbidden] = "forbidden";
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(request);
            }, std::string("does not accept field: ") + forbidden);
        }
        auto sdk_json_wrong_depth = sdk_json_4655_request;
        sdk_json_wrong_depth["depth"] = 5;
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                sdk_json_wrong_depth);
        }, "does not accept field: depth");
        auto sdk_json_wrong_limit = sdk_json_4680_request;
        sdk_json_wrong_limit["limit"] = 1;
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                sdk_json_wrong_limit);
        }, "does not accept field: limit");

        auto sdk_json_non_object = sdk_json_4655_request;
        sdk_json_non_object["document"] = tdx::Json::array();
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                sdk_json_non_object);
        }, "inline JSON object");
        auto sdk_json_missing_document = tdx::Json::object();
        sdk_json_missing_document["format"] = "sdk-json-4655";
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                sdk_json_missing_document);
        }, "document is required");
        auto sdk_json_missing_data = sdk_json_4655_request;
        sdk_json_missing_data["document"] = tdx::Json::object();
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                sdk_json_missing_data);
        }, "missing Data");
        auto sdk_json_wrong_shape = sdk_json_4655_request;
        sdk_json_wrong_shape["document"] =
            tdx::Json::parse(R"json({"Data":{}})json");
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                sdk_json_wrong_shape);
        }, "Data must be an array");

        for (const int invalid_limit : {-1, 10001}) {
            auto request = sdk_json_4655_request;
            request["limit"] = invalid_limit;
            require_error([&] {
                (void)tdx::server_detail::query_level2_decode(request);
            }, "integer in 0..10000");
        }
        auto sdk_json_invalid_depth = sdk_json_4680_request;
        sdk_json_invalid_depth["depth"] = 6;
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                sdk_json_invalid_depth);
        }, "depth must be 5 or 10");

        auto sized_document = [](std::size_t padding_size) {
            auto data = tdx::Json::object();
            data["padding"] = std::string(padding_size, 'x');
            auto document = tdx::Json::object();
            document["Data"] = std::move(data);
            return document;
        };
        const auto empty_sized_document = sized_document(0);
        const std::size_t sized_document_overhead =
            empty_sized_document.dump(-1).size();
        auto exact_document = sized_document(
            384U * 1024U - sized_document_overhead);
        require(exact_document.dump(-1).size() == 384U * 1024U,
                "SDK JSON exact-size fixture");
        auto exact_sdk_json_request = tdx::Json::object();
        exact_sdk_json_request["format"] = "sdk-json-4671";
        exact_sdk_json_request["document"] = exact_document;
        exact_sdk_json_request["limit"] = 0;
        const auto exact_sdk_json = tdx::server_detail::query_level2_decode(
            exact_sdk_json_request);
        require(exact_sdk_json.at("input_size").as_number() ==
                    384.0 * 1024.0 &&
                exact_sdk_json.at("buy_count").as_number() == 0.0 &&
                exact_sdk_json.at("sell_count").as_number() == 0.0,
                "SDK JSON 384 KiB inclusive API limit");
        auto oversized_sdk_json_request = exact_sdk_json_request;
        oversized_sdk_json_request["document"] = sized_document(
            384U * 1024U - sized_document_overhead + 1);
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(
                oversized_sdk_json_request);
        }, "384 KiB");

        auto correlation_request = tdx::Json::object();
        correlation_request["format"] = "sdk-correlation";
        correlation_request["payload_hex"] =
            "44332211887766550100363030303030000f07000009000000";
        correlation_request["limit"] = 1;
        const auto correlation = tdx::server_detail::query_level2_decode(
            correlation_request);
        require(correlation.at("schema").as_string() ==
                    "tdx-level2-sdk-correlation-registry-v1" &&
                correlation.at("record_count").as_number() == 1.0 &&
                correlation.at("records").as_array().front()
                        .at("code_ascii").as_string() == "600000" &&
                correlation.at("records").as_array().front()
                        .at("callback_policy").as_string() ==
                    "retained-after-matching-callback" &&
                correlation.at("input_size").as_number() == 25.0,
                "SDK callback correlation API contract");

        auto side_request = tdx::Json::object();
        side_request["format"] = "tcalc-order-side";
        side_request["payload_hex"] = std::string(104 * 2, '0');
        const auto side = tdx::server_detail::query_level2_decode(side_request);
        require(side.at("schema").as_string() ==
                    "tdx-level2-tcalc-order-side-v1" &&
                side.at("formula_scalar_bindings").at("ISBUYORDER")
                        .as_number() == 1.0,
                "TCalc type-104 API contract");

        auto path_request = decode_request;
        path_request["input"] = "capture.bin";
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(path_request);
        }, "does not accept field: input");

        auto odd_hex = decode_request;
        odd_hex["payload_hex"] = "abc";
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(odd_hex);
        }, "even number");

        auto wrong_option = decode_request;
        wrong_option["max_fields"] = 10;
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(wrong_option);
        }, "does not accept field: max_fields");

        auto oversized = decode_request;
        oversized["payload_hex"] = std::string((384 * 1024 + 1) * 2, '0');
        require_error([&] {
            (void)tdx::server_detail::query_level2_decode(oversized);
        }, "384 KiB");

        const auto openapi = tdx::server_detail::openapi_document();
        const auto& build_path = openapi.at("paths").at(
            "/api/v1/level2/build");
        const auto& decode_path = openapi.at("paths").at(
            "/api/v1/level2/decode");
        require(build_path.as_object().count("post") == 1 &&
                    build_path.as_object().count("get") == 0 &&
                    build_path.at("post").at("summary").as_string().find(
                        "fnSubscribeData") != std::string::npos &&
                    decode_path.as_object().count("post") == 1 &&
                    decode_path.as_object().count("get") == 0,
                "Level2 endpoints are documented as POST-only");

        std::cout << "server Level2 API tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
