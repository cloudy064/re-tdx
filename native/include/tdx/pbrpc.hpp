#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"
#include "tdx/tqlex.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tdx {

inline constexpr std::size_t pbrpc_default_max_assembled_bytes =
    128u * 1024u * 1024u;
inline constexpr std::size_t pbrpc_maximum_assembled_bytes =
    pbrpc_default_max_assembled_bytes;

struct PbrpcResponse {
    std::int32_t code{};
    std::string message;
    std::int32_t rpc_id{};
    std::int32_t start_pos{};
    std::int32_t total_len{};
    std::int32_t ret_byte_num{};
    Bytes ret_byte;
};

struct PbrpcConfig {
    std::string source_file;
    std::string entry;
    std::string request_id;
    std::string module;
    std::vector<std::string> placeholders;
    std::string body;
};

struct PbrpcRequestSpec {
    std::string entry;
    std::string module;
    Json request;
    std::string source_file;
};

struct PbrpcQueryResult {
    Bytes data;
    std::int32_t rpc_id{};
    int rounds{};
};

using PbrpcTransport = std::function<Bytes(const std::string&, const Bytes&, int)>;

Bytes encode_pbrpc_varint(std::uint64_t value);
Bytes encode_pbrpc_varint_field(std::uint32_t field, std::uint64_t value);
Bytes encode_pbrpc_bytes_field(std::uint32_t field, const Bytes& value);
Bytes encode_pbrpc_bytes_field(std::uint32_t field, const std::string& value);
Bytes build_pbrpc_request(
    const std::string& module, const std::string& request_json,
    std::int32_t rpc_id = 0, std::int32_t start_pos = 0,
    const std::string& charset = "1", std::int32_t target = 0,
    const std::string& sso_token = {});
PbrpcResponse parse_pbrpc_response(const Bytes& data);

std::vector<PbrpcConfig> inventory_pbrpc_configs(const std::filesystem::path& root);
Json pbrpc_configs_document(const std::filesystem::path& root);
std::pair<std::string, Json> parse_pbrpc_descriptor(
    std::string body, const std::map<std::string, std::string>& replacements = {});
PbrpcRequestSpec find_pbrpc_config_spec(
    const std::filesystem::path& root, const std::string& request_id,
    const std::string& entry = {}, const std::string& source_file = {},
    const std::vector<std::string>& body_contains = {},
    const std::map<std::string, std::string>& replacements = {});
void set_pbrpc_request_value(Json& request, const std::string& name, Json value);

PbrpcQueryResult query_pbrpc_raw(
    const std::string& entry, const std::string& module, const Json& request,
    const std::string& base_url = cloud_endpoints::tqlex,
    int timeout_ms = 15000, int max_rounds = 32, int retry_delay_ms = 150,
    const PbrpcTransport& transport = {},
    std::size_t max_assembled_bytes = pbrpc_default_max_assembled_bytes);
Json decode_pbrpc_result(const Bytes& data);
Json execute_pbrpc_config(
    const std::filesystem::path& root, const std::string& request_id,
    const std::map<std::string, std::string>& replacements = {},
    const std::map<std::string, std::string>& overrides = {},
    const std::string& entry = {}, const std::string& module = {},
    const std::string& source_file = {},
    const std::vector<std::string>& body_contains = {},
    const std::string& base_url = cloud_endpoints::tqlex,
    int timeout_ms = 15000, int max_rounds = 32, int retry_delay_ms = 150,
    std::size_t max_assembled_bytes = pbrpc_default_max_assembled_bytes);
int command_cloud_pbrpc(const std::vector<std::string>& args);

}  // namespace tdx
