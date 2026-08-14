#include "tdx/recon.hpp"

#include "recon_contract_internal.hpp"
#include "tdx/common.hpp"

namespace tdx {

using namespace recon_contract_detail;

Json evaluate_api_contract_response(const std::string& contract_id,
                                    int status,
                                    const std::string& content_type,
                                    const std::string& body,
                                    const Json& context) {
    Json result = Json::object();
    result["id"] = contract_id;
    result["passed"] = true;
    result["status"] = status;
    result["content_type"] = content_type;
    result["response_bytes"] = static_cast<std::uint64_t>(body.size());
    result["assertions"] = Json::array();

    const bool homepage = contract_id == "homepage";
    const bool html_shell = homepage || contract_id == "formula-workbench-route";
    const int expected_status = contract_id == "invalid-market" ? 400 : 200;
    add_assertion(result, "http_status", status == expected_status,
                  expected_status, status);
    if (html_shell) {
        const bool html = lower_ascii(content_type).find("text/html") != std::string::npos;
        add_assertion(result, "content_type", html, "text/html", content_type);
        const bool mount = body.find("id=\"app\"") != std::string::npos ||
                           body.find("id='app'") != std::string::npos;
        add_assertion(result, "svelte_mount", mount, "HTML element #app", mount);
        if (!homepage) {
            const bool root_relative_script =
                body.find("src=\"/assets/") != std::string::npos ||
                body.find("src='/assets/") != std::string::npos;
            const bool root_relative_style =
                body.find("href=\"/assets/") != std::string::npos ||
                body.find("href='/assets/") != std::string::npos;
            add_assertion(result, "root_relative_script", root_relative_script,
                          "/assets/*.js", root_relative_script);
            add_assertion(result, "root_relative_style", root_relative_style,
                          "/assets/*.css", root_relative_style);
        }
        return result;
    }
    if (contract_id == "formula-drawicon-sprite") {
        const bool png_type = lower_ascii(content_type).find("image/png") !=
                              std::string::npos;
        add_assertion(result, "content_type", png_type, "image/png", content_type);
        const bool signature = body.size() > 100 &&
            static_cast<unsigned char>(body[0]) == 0x89 &&
            body.substr(1, 3) == "PNG" && body[4] == '\r' && body[5] == '\n' &&
            static_cast<unsigned char>(body[6]) == 0x1A && body[7] == '\n';
        add_assertion(result, "png_signature", signature, true, signature);
        return result;
    }
    if (contract_id == "market-stream-live") {
        const bool event_stream = lower_ascii(content_type).find("text/event-stream") !=
                                  std::string::npos;
        add_assertion(result, "content_type", event_stream,
                      "text/event-stream", content_type);
        const auto data_start = body.find("data: ");
        const auto data_end = data_start == std::string::npos
            ? std::string::npos : body.find('\n', data_start);
        Json event;
        bool parsed_event = false;
        if (data_start != std::string::npos) {
            try {
                event = Json::parse(body.substr(data_start + 6,
                    data_end == std::string::npos ? std::string::npos :
                    data_end - data_start - 6));
                parsed_event = true;
            } catch (...) {}
        }
        add_assertion(result, "sse_data_json", parsed_event, true, parsed_event);
        if (!parsed_event) return result;
        add_assertion(result, "schema",
                      string_is(member(event, "schema"),
                                "tdx-market-l1-stream-event-v1"),
                      "tdx-market-l1-stream-event-v1",
                      value_or_null(member(event, "schema")));
        add_assertion(result, "initial_snapshot",
                      string_is(member(event, "type"), "snapshot"), "snapshot",
                      value_or_null(member(event, "type")));
        add_assertion(result, "security_code",
                      string_is(member_path(event, {"record", "code"}), "000001"),
                      "000001", value_or_null(member_path(event, {"record", "code"})));
        add_assertion(result, "persistent_upstream_session",
                      bool_is(member_path(event,
                          {"source", "session", "persistent"}), true), true,
                      value_or_null(member_path(event,
                          {"source", "session", "persistent"})));
        const bool configured_pool = string_is(member_path(event,
                {"source", "session", "endpoint_source"}),
                "connect.cfg:hqhost-primary-first") &&
            bool_is(member_path(event,
                {"source", "session", "primary_configured"}), true) &&
            number_is(member_path(event,
                {"source", "session", "endpoint_pool_size"}), 3);
        add_assertion(result, "configured_endpoint_pool", configured_pool,
                      "connect.cfg primary-first pool of 3", configured_pool);
        add_assertion(result, "local_sse_delivery",
                      string_is(member(event, "delivery"), "local-sse"), "local-sse",
                      value_or_null(member(event, "delivery")));
        add_assertion(result, "permission_boundary",
                      bool_is(member_path(event,
                          {"fast_hq_boundary", "fast_hq_subscribe_used"}), false), false,
                      value_or_null(member_path(event,
                          {"fast_hq_boundary", "fast_hq_subscribe_used"})));
        return result;
    }

    const bool json_type = lower_ascii(content_type).find("application/json") !=
                           std::string::npos;
    add_assertion(result, "content_type", json_type, "application/json", content_type);
    Json document;
    bool parsed = false;
    try {
        document = Json::parse(body);
        parsed = true;
    } catch (...) {}
    add_assertion(result, "valid_json", parsed, true, parsed);
    if (!parsed) return result;

    if (validate_formula_contract(contract_id, document, result) ||
        validate_market_contract(contract_id, document, context, result))
        return result;
    throw Error("unknown API contract id: " + contract_id);
}

}  // namespace tdx
