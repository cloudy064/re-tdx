#pragma once

#include "tdx/json.hpp"
#include "tdx/pbrpc.hpp"
#include "tdx/tqlex.hpp"

#include <filesystem>
#include <vector>

namespace tdx {

Json cloud_variant_coverage_document(
    const std::vector<TqlexConfig>& tqlex_configs,
    const std::vector<PbrpcConfig>& pbrpc_configs,
    bool gaps_only = false);
Json cloud_variant_coverage_document(const std::filesystem::path& root,
                                     bool gaps_only = false);
int command_recon_cloud_variants(const std::vector<std::string>& args);

}  // namespace tdx
