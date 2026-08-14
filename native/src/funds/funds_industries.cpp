#include "funds_internal.hpp"

namespace tdx {
IntradayFundsService::IntradayFundsService(std::filesystem::path root,
    const BlockData& blocks, Fetcher fetcher)
    : root_(std::move(root)), securities_(blocks.securities), fetcher_(std::move(fetcher)) {
    std::map<std::string, const Block*> index;
    for (const auto& block : blocks.blocks) index[block.block_id] = &block;
    for (const auto& member : blocks.members) {
        if (member.family != "research-industry") continue;
        const auto block = index.find(member.block_id);
        if (block == index.end() || block->second->level != 1 || block->second->block_code.rfind("881", 0) != 0) continue;
        ResearchIndustry industry{"1", block->second->block_code, block->second->name};
        industries_[industry.code] = industry; security_industries_[{member.market_id, member.code}] = std::move(industry);
    }
}
const ResearchIndustry* IntradayFundsService::industry_for_security(int market_id,
    const std::string& code) const {
    const auto found = security_industries_.find({market_id, code});
    return found == security_industries_.end() ? nullptr : &found->second;
}
}  // namespace tdx
