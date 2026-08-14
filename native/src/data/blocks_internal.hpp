#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::block_detail {

struct BlockFamilyDefinition {
    std::string_view key;
    std::string_view display_name;
};

inline constexpr std::array<BlockFamilyDefinition, 5> block_families{{
    {"industry", "通达信行业"},
    {"research-industry", "研究行业"},
    {"concept", "概念板块"},
    {"style", "风格板块"},
    {"index", "指数板块"},
}};

struct MarketDefinition {
    int id;
    std::string_view code;
    std::string_view display_name;
    std::string_view tnf_name;
};

inline constexpr std::array<MarketDefinition, 3> block_markets{{
    {0, "SZ", "深圳", "szs.tnf"},
    {1, "SH", "上海", "shs.tnf"},
    {2, "BJ", "北京", "bjs.tnf"},
}};

const BlockFamilyDefinition* find_block_family(std::string_view key);
const MarketDefinition* find_block_market(int market_id);
std::string block_family_name(std::string_view key);
std::vector<std::string> text_lines(std::string_view text);
int strict_int(const std::string& text, std::string_view context);
int tdx_atol(const std::string& text);
std::string fixed_ascii(const std::uint8_t* data, std::size_t size);
std::string fixed_gbk(const std::uint8_t* data, std::size_t size);
Security unresolved_security(int market_id, std::string code);
std::string parent_key(const std::string& source_key);
int source_level(const std::string& source_key);
std::string infoharbor_family(std::string_view prefix);

}  // namespace tdx::block_detail
