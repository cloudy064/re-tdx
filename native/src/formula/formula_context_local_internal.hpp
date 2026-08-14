#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace tdx::formula_context_detail {

void bind_blocksetnum(Json &context, const std::filesystem::path &root,
                      const BlockData &data,
                      const std::vector<std::string> &bindings);

void bind_local_security_metadata(
    Json &context, Json &symbols, const std::filesystem::path &root,
    const std::string &market, const std::string &code,
    const std::set<std::string> &dependencies);

} // namespace tdx::formula_context_detail
