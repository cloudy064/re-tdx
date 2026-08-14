#pragma once

#include "tdx/cloud_endpoints.hpp"
#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tdx {

struct TqlexConfig {
    std::string source_file;
    std::string entry;
    std::string request_format;
    std::string request_id;
    std::vector<std::string> placeholders;
    std::string body;
};

struct TqlexRequestSpec {
    std::string entry;
    Json request;
    std::string source_file;
};

using TqlexTransport = std::function<Bytes(const std::string&, const Bytes&, int)>;

std::vector<TqlexConfig> inventory_cloud_configs(
    const std::filesystem::path& root, const std::string& request_format);
std::vector<TqlexConfig> inventory_tqlex_configs(const std::filesystem::path& root);
Json tqlex_configs_document(const std::filesystem::path& root);
Json parse_tqlex_config_body(
    std::string body, const std::map<std::string, std::string>& replacements = {},
    int page = 0, int page_size = 20);
TqlexRequestSpec find_tqlex_config_spec(
    const std::filesystem::path& root, const std::string& request_id,
    const std::string& entry = {}, const std::string& source_file = {},
    const std::vector<std::string>& body_contains = {},
    const std::map<std::string, std::string>& replacements = {},
    int page = 0, int page_size = 20);
void set_tqlex_request_value(Json& request, const std::string& name, Json value);
Json query_tqlex(
    const std::string& entry, const Json& request,
    const std::string& base_url = cloud_endpoints::tqlex,
    int timeout_ms = 15000, const TqlexTransport& transport = {});
Json query_tqlex_all_pages(
    const std::string& entry, const Json& request, int start_page = 0,
    int page_size = 20, int max_pages = 100,
    const std::string& base_url = cloud_endpoints::tqlex,
    int timeout_ms = 15000, const TqlexTransport& transport = {});
Json execute_tqlex_config(
    const std::filesystem::path& root, const std::string& request_id,
    const std::map<std::string, std::string>& replacements = {},
    const std::map<std::string, std::string>& overrides = {},
    const std::string& entry = {}, const std::string& source_file = {},
    const std::vector<std::string>& body_contains = {}, bool all_pages = false,
    int page = -1, int page_size = 0, int max_pages = 100,
    const std::string& base_url = cloud_endpoints::tqlex,
    int timeout_ms = 15000);
int command_cloud_tqlex(const std::vector<std::string>& args);

}  // namespace tdx
