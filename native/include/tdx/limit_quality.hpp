#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

Json compose_limit_quality_document(const Json& ranking_document,
                                    const Json& depth_document,
                                    const Json& stats_document,
                                    const Json& auction_document,
                                    int limit = 200);

Json fetch_market_limit_quality_document(const std::filesystem::path& root,
                                         int limit = 200,
                                         int auction_limit = 20,
                                         int timeout_ms = 10000,
                                         const BlockData* block_data = nullptr,
                                         const std::vector<std::string>& hosts = {});

int command_market_limit_quality(const std::vector<std::string>& args);

}  // namespace tdx
