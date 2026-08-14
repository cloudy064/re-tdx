#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <set>
#include <string>

namespace tdx::formula_context_detail {

void bind_block_metadata(
    Json& context, Json& symbols, const BlockData& data,
    const std::string& market, const std::string& code,
    const std::set<std::string>& dependencies);

void bind_block_code_functions(
    Json& context, const BlockData& data, const std::filesystem::path& root,
    const std::string& market, const std::string& code,
    const std::set<std::string>& dependencies);

}  // namespace tdx::formula_context_detail
