#include "industry_profile_internal.hpp"

#include <algorithm>
#include <tuple>

namespace tdx {

IndustryProfileService::IndustryProfileService(const BlockData& data)
    : securities_(data.securities) {
    std::map<std::string, std::string> codes_by_id;
    for (const auto& block : data.blocks) {
        if (block.family != "research-industry" || !detail::industry_profile::valid_research_industry_code(block.block_code))
            continue;
        blocks_[block.block_code] = block;
        codes_by_id[block.block_id] = block.block_code;
    }
    for (const auto& [code, block] : blocks_) {
        const auto parent = codes_by_id.find(block.parent_block_id);
        const auto parent_code = parent == codes_by_id.end() ? "" : parent->second;
        parents_[code] = parent_code;
        if (!parent_code.empty()) children_[parent_code].push_back(code);
    }
    for (auto& [code, children] : children_) {
        std::sort(children.begin(), children.end(), [&](const auto& left, const auto& right) {
            return std::tie(blocks_.at(left).source_key, left) <
                   std::tie(blocks_.at(right).source_key, right);
        });
    }
    for (const auto& member : data.members) {
        if (member.family != "research-industry" || !blocks_.count(member.block_code)) continue;
        const auto key = std::make_pair(member.market_id, member.code);
        block_members_[member.block_code].insert(key);
        security_blocks_[key].push_back(member.block_code);
    }
    for (auto& [key, codes] : security_blocks_) {
        std::stable_sort(codes.begin(), codes.end(), [&](const auto& left, const auto& right) {
            return blocks_.at(left).level > blocks_.at(right).level;
        });
    }
}

}  // namespace tdx

