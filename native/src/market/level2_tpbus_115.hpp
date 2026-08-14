#pragma once

#include "tdx/common.hpp"

#include <filesystem>
#include <string>

namespace tdx::level2_detail {

// Command-only bounded file reader for the tpbus-115 decode surface.
Bytes read_level2_tpbus_115_cli_input(const std::filesystem::path& path,
                                      const std::string& encoding);

}  // namespace tdx::level2_detail
