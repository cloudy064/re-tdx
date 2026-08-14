#include "blocks_internal.hpp"

#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace tdx {
namespace detail = block_detail;

std::vector<Security> parse_tnf(const Bytes& data, int market_id) {
    const auto* market = detail::find_block_market(market_id);
    if (!market) throw Error("unsupported TNF market ID");
    if (data.size() < 50) throw Error("TNF file is shorter than its 50-byte header");
    std::size_t record_size = 0;
    std::size_t name_offset = 0;
    if ((data.size() - 50) % 360 == 0) {
        record_size = 360;
        name_offset = 31;
    } else if ((data.size() - 50) % 314 == 0) {
        record_size = 314;
        name_offset = 23;
    } else {
        throw Error("unsupported TNF file length: " + std::to_string(data.size()));
    }
    std::vector<Security> result;
    result.reserve((data.size() - 50) / record_size);
    for (std::size_t offset = 50; offset < data.size(); offset += record_size) {
        const auto* record = data.data() + offset;
        auto code = detail::fixed_ascii(record, 6);
        if (code.empty()) continue;
        double trade_unit = 0.0;
        std::uint8_t tdx_category = 0;
        std::optional<std::uint8_t> price_precision;
        if (record_size == 360) {
            if (record[76] <= 8) price_precision = record[76];
            const auto parsed = static_cast<double>(read_f32_le(record + 78));
            if (std::isfinite(parsed) && parsed > 0.0) trade_unit = parsed;
            tdx_category = record[282];
        }
        result.push_back(Security{
            market_id, std::string(market->code), std::string(market->display_name),
            std::move(code), detail::fixed_gbk(record + name_offset, 32),
            trade_unit, tdx_category, price_precision});
    }
    return result;
}

std::vector<Block> parse_industry_catalog(std::string_view text) {
    struct Raw {
        std::string family;
        std::string name;
        std::string code;
        std::string key;
        bool leaf{};
    };
    std::vector<Raw> raw;
    std::set<std::string> seen;
    std::size_t line_number = 0;
    for (const auto& line : detail::text_lines(text)) {
        ++line_number;
        if (line.empty()) continue;
        const auto fields = split(line, '|');
        if (fields.size() != 6)
            throw Error("tdxzs3.cfg line " + std::to_string(line_number) +
                        ": expected 6 fields");
        if (fields[2] != "2" && fields[2] != "12") continue;
        if (!seen.insert(fields[5]).second)
            throw Error("duplicate industry source key: " + fields[5]);
        raw.push_back(Raw{
            fields[2] == "2" ? "industry" : "research-industry",
            fields[0], fields[1], fields[5], detail::strict_int(fields[4], "leaf") != 0});
    }
    std::map<std::string, std::string> ids;
    for (const auto& item : raw) ids[item.key] = item.family + ":" + item.key;
    std::vector<Block> result;
    result.reserve(raw.size());
    for (const auto& item : raw) {
        const auto parent = ids.find(detail::parent_key(item.key));
        result.push_back(Block{
            ids[item.key], item.family, detail::block_family_name(item.family),
            item.code, item.name, item.key,
            parent == ids.end() ? "" : parent->second,
            detail::source_level(item.key), item.leaf, std::nullopt, 0, "", "",
            "tdxzs3.cfg+tdxhy.cfg"});
    }
    return result;
}

std::vector<IndustryAssignment> parse_industry_assignments(std::string_view text) {
    std::vector<IndustryAssignment> result;
    std::size_t line_number = 0;
    for (const auto& line : detail::text_lines(text)) {
        ++line_number;
        if (line.empty()) continue;
        const auto fields = split(line, '|');
        if (fields.size() != 6)
            throw Error("tdxhy.cfg line " + std::to_string(line_number) +
                        ": expected 6 fields");
        result.push_back(IndustryAssignment{
            detail::strict_int(fields[0], "market ID"), fields[1], fields[2], fields[5]});
    }
    return result;
}

std::vector<BlockMember> build_industry_members(
    std::vector<Block>& blocks,
    const std::vector<IndustryAssignment>& assignments,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    std::map<std::string, Block*> by_source;
    for (auto& block : blocks) by_source[block.source_key] = &block;
    std::vector<BlockMember> result;
    std::map<std::string, int> counts;
    for (const auto& assignment : assignments) {
        const std::array<std::pair<std::string, std::string>, 2> assigned_families{{
            {"industry", assignment.industry_code},
            {"research-industry", assignment.research_industry_code},
        }};
        for (const auto& [family, assigned] : assigned_families) {
            for (std::size_t length = 3; length <= assigned.size(); length += 2) {
                const auto prefix = assigned.substr(0, length);
                const auto found = by_source.find(prefix);
                if (found == by_source.end() || found->second->family != family) continue;
                const auto known = securities.find({assignment.market_id, assignment.code});
                const Security security = known == securities.end()
                    ? detail::unresolved_security(assignment.market_id, assignment.code)
                    : known->second;
                const auto& block = *found->second;
                result.push_back(BlockMember{
                    block.block_id, block.family, block.family_name, block.block_code,
                    block.name, security.security_id(), security.market_id,
                    security.market, security.code, security.name,
                    prefix == assigned ? "direct" : "descendant", !security.name.empty()});
                ++counts[block.block_id];
            }
        }
    }
    for (auto& block : blocks) block.member_count = counts[block.block_id];
    return result;
}

}  // namespace tdx
