#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

Json cloud_routes_document(const std::filesystem::path& root,
                           const std::string& entry_filter = {},
                           const std::string& source_filter = {});
int command_cloud_routes(const std::vector<std::string>& args);

}  // namespace tdx
