#include "server_formula_internal.hpp"

#include "tdx/common.hpp"

#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

template <typename Callback>
void require_throws(Callback&& callback, const char* message) {
    try {
        callback();
    } catch (const tdx::Error&) {
        return;
    }
    throw tdx::Error(message);
}

tdx::Json context_template() {
    auto result = tdx::Json::object();
    result["schema"] = "tdx-formula-explicit-context-v1";
    result["automatic_market_context"] = false;
    result["formula_scalar_bindings"] = tdx::Json::object();
    result["series"] = tdx::Json::object();
    result["series"]["SIGNALS_QS#102#0"] = tdx::Json::object();
    result["series"]["SIGNALS_QS#102#0"]["2026-08-14|15:00"] = nullptr;
    result["_template"] = tdx::Json::object();
    result["_template"]["schema"] =
        "tdx-formula-explicit-context-template-v1";
    result["_template"]["binding_count"] = 1;
    result["_template"]["scalar_binding_count"] = 0;
    result["_template"]["series_binding_count"] = 1;
    result["_template"]["stamp_count"] = 1;
    result["_template"]["bindings"] = tdx::Json::array();
    auto binding = tdx::Json::object();
    binding["name"] = "SIGNALS_QS#102#0";
    binding["source_kind"] = "broker-private";
    binding["value_shape"] = "date-time-series";
    binding["required"] = true;
    result["_template"]["bindings"].push_back(std::move(binding));
    return result;
}

tdx::Json capture(bool include_value = true) {
    auto result = tdx::Json::object();
    result["schema"] = "tdx-formula-caller-context-capture-v1";
    result["capture_id"] = "http-owned-001";
    result["ownership_confirmed"] = true;
    result["formula_scalar_bindings"] = tdx::Json::object();
    result["records"] = tdx::Json::array();
    auto record = tdx::Json::object();
    record["stamp"] = "2026-08-14|15:00";
    record["values"] = tdx::Json::object();
    if (include_value) record["values"]["SIGNALS_QS#102#0"] = 7.0;
    result["records"].push_back(std::move(record));
    return result;
}

tdx::Json request(bool include_value = true) {
    auto result = tdx::Json::object();
    result["template"] = context_template();
    result["capture"] = capture(include_value);
    return result;
}

}  // namespace

int main() {
    try {
        const auto result =
            tdx::server_detail::query_formula_context_import(request());
        require(result.at("schema").as_string() ==
                    "tdx-formula-explicit-context-v1" &&
                    result.at("series").at("SIGNALS_QS#102#0")
                        .at("2026-08-14|15:00").as_number() == 7.0,
                "HTTP context-import returns a directly reusable context");
        const auto& metadata = result.at("_capture_import");
        require(metadata.at("complete").as_bool() &&
                    !metadata.at("request_body_retained").as_bool() &&
                    !metadata.at("capture_document_retained").as_bool() &&
                    !metadata.at("file_accessed").as_bool() &&
                    !metadata.at("path_accepted").as_bool() &&
                    !metadata.at("sdk_called").as_bool() &&
                    !metadata.at("subscription_sent").as_bool() &&
                    !metadata.at("account_accessed").as_bool() &&
                    !metadata.at("order_sent").as_bool() &&
                    metadata.at("network_requests").as_number() == 0.0,
                "HTTP context-import preserves retention and side-effect boundaries");

        require_throws([&] {
            (void)tdx::server_detail::query_formula_context_import(request(false));
        }, "HTTP strict mode rejects incomplete capture");
        auto partial = request(false);
        partial["allow_partial"] = true;
        const auto partial_result =
            tdx::server_detail::query_formula_context_import(partial);
        require(!partial_result.at("_capture_import").at("complete").as_bool() &&
                    partial_result.at("_capture_import")
                        .at("missing_series_point_count").as_number() == 1.0,
                "HTTP allow_partial returns bounded diagnostics");

        for (const auto* field : {
                 "path", "file", "url", "endpoint", "token", "handle"}) {
            auto unsafe = request();
            unsafe[field] = "secret";
            require_throws([&] {
                (void)tdx::server_detail::query_formula_context_import(unsafe);
            }, "HTTP context-import rejects path/network/credential-shaped fields");
        }
        auto wrong_partial = request();
        wrong_partial["allow_partial"] = 1;
        require_throws([&] {
            (void)tdx::server_detail::query_formula_context_import(wrong_partial);
        }, "HTTP allow_partial is strictly boolean");
        auto unowned = request();
        unowned["capture"]["ownership_confirmed"] = false;
        require_throws([&] {
            (void)tdx::server_detail::query_formula_context_import(unowned);
        }, "HTTP context-import requires caller ownership confirmation");

        std::cout << "server formula context import tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
