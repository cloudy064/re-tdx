#pragma once

#include "tdx/json.hpp"

#include <filesystem>

namespace tdx::server_detail {

void project_local_catalog_resource_paths(
    Json& document, const std::filesystem::path& root);

}  // namespace tdx::server_detail
