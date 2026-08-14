#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace tdx::formula_context_detail {

void bind_security_stat_functions(
    Json& context, Json& symbols, const std::filesystem::path& root,
    int market_id, const std::string& code,
    const std::set<std::string>& dependencies, int timeout_ms);

void bind_public_market_summary_functions(
    Json& context, const std::filesystem::path& root, int market_id,
    const std::string& code, const std::set<std::string>& dependencies,
    const std::vector<std::string>& main_bindings,
    const BlockData* block_data, int timeout_ms);

}  // namespace tdx::formula_context_detail
