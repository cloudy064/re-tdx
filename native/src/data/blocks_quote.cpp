#include "tdx/blocks_quote.hpp"

#include "tdx/security_identity.hpp"

#include <sstream>
#include <vector>

namespace tdx {
namespace {

std::vector<const Block*> exact_matches(
    const BlockData& data,
    const std::string& query,
    const std::string Block::* field) {
    std::vector<const Block*> result;
    for (const auto& block : data.blocks)
        if (lower_ascii(block.*field) == query) result.push_back(&block);
    return result;
}

const Block& unique_match(const std::vector<const Block*>& matches,
                          std::string_view query) {
    if (matches.empty()) throw Error("block not found: " + std::string(query));
    if (matches.size() == 1) return *matches.front();
    std::ostringstream message;
    message << "block is ambiguous: " << query << "; use a block_id: ";
    for (std::size_t index = 0; index < matches.size(); ++index) {
        if (index) message << ", ";
        message << matches[index]->block_id;
    }
    throw Error(message.str());
}

}  // namespace

BlockQuoteTarget resolve_block_quote_target(const BlockData& data,
                                             std::string_view raw_query) {
    const auto query = lower_ascii(trim(std::string(raw_query)));
    if (query.empty()) throw Error("block query is required");

    auto matches = exact_matches(data, query, &Block::block_id);
    if (matches.empty()) matches = exact_matches(data, query, &Block::block_code);
    if (matches.empty()) matches = exact_matches(data, query, &Block::name);
    const auto& block = unique_match(matches, raw_query);
    if (!is_tdx_block_index_code(block.block_code))
        throw Error("block has no supported public K-line code: " +
                    block.block_id + " (" + block.block_code + ")");
    return {1, "sh", block.block_code, block};
}

}  // namespace tdx
