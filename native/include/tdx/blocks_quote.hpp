#pragma once

#include "tdx/blocks.hpp"

#include <string>
#include <string_view>

namespace tdx {

struct BlockQuoteTarget {
    int market_id{};
    std::string market;
    std::string code;
    Block block;
};

BlockQuoteTarget resolve_block_quote_target(
    const BlockData& data, std::string_view query);

}  // namespace tdx
