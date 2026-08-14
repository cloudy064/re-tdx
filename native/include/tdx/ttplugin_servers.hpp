#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

Json ttplugin_server_config_document(
    const std::vector<std::filesystem::path>& inputs,
    const std::vector<std::string>& urls = {},
    const std::vector<std::string>& sections = {});
int command_recon_ttplugin_servers(const std::vector<std::string>& args);

}  // namespace tdx
