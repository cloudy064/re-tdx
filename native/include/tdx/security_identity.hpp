#pragma once

#include <algorithm>
#include <string_view>

namespace tdx {

// Public TDX block indexes use Shanghai market id 1 on the 0x052D K-line
// wire.  An explicit bj:/2: prefix still takes precedence at the call site.
inline bool is_tdx_block_index_code(std::string_view code) {
    return code.size() == 6 &&
           (code.substr(0, 3) == "880" || code.substr(0, 3) == "881") &&
           std::all_of(code.begin(), code.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           });
}

}  // namespace tdx
