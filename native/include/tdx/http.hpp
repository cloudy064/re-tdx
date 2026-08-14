#pragma once

#include "tdx/common.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx {

struct HttpResult {
    int status{};
    std::string content_type;
    Bytes body;
};

HttpResult http_get(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers = {},
    int timeout_ms = 15000, std::size_t maximum_response_bytes = 64 * 1024 * 1024);
HttpResult http_post(
    const std::string& url, const Bytes& body,
    const std::vector<std::pair<std::string, std::string>>& headers = {},
    int timeout_ms = 15000, std::size_t maximum_response_bytes = 64 * 1024 * 1024);
std::string url_encode(std::string_view value);

}  // namespace tdx
