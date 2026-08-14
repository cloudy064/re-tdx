#include "blocks_internal.hpp"

#include <algorithm>
#include <filesystem>
#include <set>
#include <tuple>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {
namespace detail = block_detail;

std::pair<std::vector<Block>, std::vector<BlockMember>> parse_infoharbor(
    std::string_view text,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    std::vector<Block> blocks;
    std::vector<BlockMember> members;
    Block* current = nullptr;
    int current_count = 0;
    auto finish = [&] {
        if (!current) return;
        if (!current->declared_count ||
            *current->declared_count != current_count)
            throw Error(current->block_id +
                        ": declared/parsed member count mismatch");
        current->member_count = current_count;
    };
    std::size_t line_number = 0;
    for (const auto& line : detail::text_lines(text)) {
        ++line_number;
        if (line.empty()) continue;
        if (line.front() == '#') {
            finish();
            const auto fields = split(std::string_view(line).substr(1), ',');
            const auto underscore = fields.empty()
                ? std::string::npos : fields[0].find('_');
            if (fields.size() != 7 || underscore == std::string::npos)
                throw Error("infoharbor line " + std::to_string(line_number) +
                            ": invalid header");
            const auto family = detail::infoharbor_family(
                fields[0].substr(0, underscore));
            const auto name = fields[0].substr(underscore + 1);
            const auto stable = fields[2].empty() ? name : fields[2];
            blocks.push_back(Block{
                family + ":" + stable, family,
                detail::block_family_name(family), fields[2], name, fields[0],
                "", 1, true,
                detail::strict_int(fields[1].empty() ? "0" : fields[1],
                                   "member count"),
                0, fields[3], fields[4], "infoharbor_block.dat"});
            current = &blocks.back();
            current_count = 0;
            continue;
        }
        if (!current)
            throw Error("infoharbor member appears before a header");
        for (const auto& token : split(line, ',')) {
            if (token.empty()) continue;
            const auto hash = token.find('#');
            if (hash == std::string::npos)
                throw Error("invalid infoharbor member: " + token);
            const int market_id = detail::strict_int(
                token.substr(0, hash), "member market");
            const auto code = token.substr(hash + 1);
            const auto known = securities.find({market_id, code});
            const Security security = known == securities.end()
                ? detail::unresolved_security(market_id, code) : known->second;
            members.push_back(BlockMember{
                current->block_id, current->family, current->family_name,
                current->block_code, current->name, security.security_id(),
                security.market_id, security.market, security.code,
                security.name, "direct", !security.name.empty()});
            ++current_count;
        }
    }
    finish();
    return {std::move(blocks), std::move(members)};
}

BlockData load_blocks(const fs::path& root,
                      const std::set<std::string>& families) {
    const auto cache = root / "T0002" / "hq_cache";
    if (!fs::is_directory(cache))
        throw Error("TDX hq_cache directory is missing: " + path_utf8(cache));
    BlockData result;
    for (const auto& market : detail::block_markets) {
        const auto path = cache / std::string(market.tnf_name);
        if (!fs::is_regular_file(path))
            throw Error("security master is missing: " + path_utf8(path));
        for (auto security : parse_tnf(read_bytes(path), market.id))
            result.securities[{market.id, security.code}] = std::move(security);
    }
    if (families.count("industry") ||
        families.count("research-industry")) {
        auto blocks = parse_industry_catalog(
            decode_gbk(read_bytes(cache / "tdxzs3.cfg")));
        blocks.erase(std::remove_if(
            blocks.begin(), blocks.end(), [&](const Block& block) {
                return !families.count(block.family);
            }), blocks.end());
        const auto assignments = parse_industry_assignments(
            decode_gbk(read_bytes(cache / "tdxhy.cfg")));
        auto members = build_industry_members(
            blocks, assignments, result.securities);
        result.blocks.insert(result.blocks.end(), blocks.begin(), blocks.end());
        result.members.insert(
            result.members.end(), members.begin(), members.end());
    }
    if (families.count("concept") || families.count("style") ||
        families.count("index")) {
        auto [blocks, members] = parse_infoharbor(
            decode_gbk(read_bytes(cache / "infoharbor_block.dat")),
            result.securities);
        std::set<std::string> selected;
        for (const auto& block : blocks)
            if (families.count(block.family)) selected.insert(block.block_id);
        blocks.erase(std::remove_if(
            blocks.begin(), blocks.end(), [&](const Block& block) {
                return !selected.count(block.block_id);
            }), blocks.end());
        members.erase(std::remove_if(
            members.begin(), members.end(), [&](const BlockMember& member) {
                return !selected.count(member.block_id);
            }), members.end());
        result.blocks.insert(result.blocks.end(), blocks.begin(), blocks.end());
        result.members.insert(
            result.members.end(), members.begin(), members.end());
    }
    std::sort(result.blocks.begin(), result.blocks.end(),
              [](const Block& left, const Block& right) {
                  return std::tie(left.family, left.block_code, left.name) <
                         std::tie(right.family, right.block_code, right.name);
              });
    std::sort(result.members.begin(), result.members.end(),
              [](const BlockMember& left, const BlockMember& right) {
                  return std::tie(left.family, left.block_code,
                                  left.block_name, left.market_id, left.code) <
                         std::tie(right.family, right.block_code,
                                  right.block_name, right.market_id, right.code);
              });
    return result;
}

}  // namespace tdx
