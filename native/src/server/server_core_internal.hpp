#pragma once

#include "server_formula_internal.hpp"
#include "server_http_internal.hpp"
#include "server_state_internal.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace tdx::server_detail {

void attach_security_metadata(
    const ApiState& state,
    Json& document,
    const std::string& market,
    const std::string& code);
FormulaHttpState formula_http_state(const ApiState& state);
std::filesystem::path from_utf8(const std::string& value);
std::string url_decode(std::string_view value);
RequestTarget parse_target(std::string_view raw);
Json block_json(const Block& item);
Json member_json(const BlockMember& item);
Json health_document(const ApiState& state);
std::string normalize_market(std::string market);

Json query_cloud_workflow_api(
    const ApiState& state,
    const RequestTarget& target);
Json query_minute(const ApiState& state, const RequestTarget& target);
Json query_jsn_security(const ApiState& state, const RequestTarget& target);
Json query_security_profile(const ApiState& state, const RequestTarget& target);
Json query_industry_tree(const ApiState& state, const RequestTarget& target);
Json query_jsn_resource(
    const ApiState& state,
    const RequestTarget& target,
    bool allow_write);
Json query_blocks(const ApiState& state, const RequestTarget& target);
Json query_security_blocks(const ApiState& state, const RequestTarget& target);
Json query_securities(const ApiState& state, const RequestTarget& target);

std::string content_type_for(const std::filesystem::path& path);
HttpResponse formula_signal_image_response(
    const ApiState& state,
    const RequestTarget& target);
bool path_starts_with(
    const std::filesystem::path& path,
    const std::filesystem::path& root);
HttpResponse static_response(const ApiState& state, const RequestTarget& target);
HttpResponse route(
    const ApiState& state,
    const RequestTarget& target,
    bool allow_write = false,
    const Json* request_body = nullptr);

}  // namespace tdx::server_detail
