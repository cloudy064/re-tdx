#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace tdx {

namespace detail {
using JsnFetchAttempt = std::function<Json()>;
Json retry_jsn_rows_fetch(const JsnFetchAttempt& attempt,
                          int max_attempts = 3,
                          int retry_delay_ms = 250);
Json resilient_jsn_rows_fetch(const std::vector<std::string>& cache_keys,
                              const JsnFetchAttempt& attempt,
                              int max_attempts = 3,
                              int retry_delay_ms = 250);
}  // namespace detail

Json jsn_source_metadata(const Json& document);
Json jsn_sources_health(const Json& sources);

Json transfer_jsn_resource(const std::string& resource,
                           const std::string& prefix,
                           const std::filesystem::path& output_root,
                           bool download,
                           int timeout_ms = 10000,
                           const std::filesystem::path& root = {},
                           const std::vector<std::string>& hosts = {});
Json fetch_jsn_resource_rows(const std::string& resource,
                             const std::string& prefix = "bi",
                             int timeout_ms = 10000,
                             const std::filesystem::path& root = {},
                             const std::vector<std::string>& hosts = {});
Json fetch_jsn_resources_rows(const std::vector<std::string>& resources,
                              const std::string& prefix = "bi",
                              int timeout_ms = 10000,
                              const std::filesystem::path& root = {},
                              const std::vector<std::string>& hosts = {});
int command_jsn_download(const std::vector<std::string>& args);

}  // namespace tdx
