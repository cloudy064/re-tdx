#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace tdx::pe_detail {

// Read only the named-export table from an on-disk PE image. The image is
// never mapped or loaded as executable code.
std::vector<std::string> read_pe_export_names(
    const std::filesystem::path& path);

}  // namespace tdx::pe_detail
