#pragma once

#include "server_formula_internal.hpp"

#include "tdx/json.hpp"

#include <filesystem>

namespace tdx::server_detail {

Json query_tpool_catalog(const std::filesystem::path& root,
                         const RequestTarget& target);
Json query_tpool_history(const std::filesystem::path& root,
                         const RequestTarget& target);
Json query_tpool_file_evaluation(const std::filesystem::path& root,
                                 const RequestTarget& target,
                                 const Json& formulas);

}  // namespace tdx::server_detail
