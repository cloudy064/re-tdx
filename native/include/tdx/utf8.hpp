#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace tdx {

std::string utf8_prefix(std::string_view value, std::size_t max_bytes);

}  // namespace tdx
