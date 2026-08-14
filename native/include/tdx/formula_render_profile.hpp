#pragma once

#include "tdx/json.hpp"

#include <filesystem>

namespace tdx {

// Describe the native TdxW annotation font selected by formula renderers.
// When a TDX root is supplied, only the non-sensitive display keys from
// T0002/user.ini are read; an empty/missing root returns recovered defaults.
Json formula_render_environment_document(const std::filesystem::path& root = {});

}  // namespace tdx
