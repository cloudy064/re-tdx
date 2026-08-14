#pragma once

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace tdx::formula_context_detail {

inline constexpr std::array<std::string_view, 14> industry_context_symbols{
    "HYBLOCK",   "HYZSCODE",   "HYSYL",      "HYSJL",       "LEVEL1HYBLOCK",
    "MOREHYBLOCK", "HY_INDEXA", "HY_INDEXADV", "HY_INDEXC",   "HY_INDEXDEC",
    "HY_INDEXH", "HY_INDEXL",  "HY_INDEXO",  "HY_INDEXV",
};

inline constexpr std::array<std::string_view, 6> block_metadata_symbols{
    "INBLOCK", "FGBLOCK", "FGBLOCKNUM", "GNBLOCKNUM", "ZSBLOCK", "ZSBLOCKNUM",
};

inline constexpr std::array<std::string_view, 10> index_series_symbols{
    "ADVANCE", "DECLINE", "INDEXA", "INDEXADV", "INDEXC",
    "INDEXDEC", "INDEXH", "INDEXL", "INDEXO", "INDEXV",
};

struct IndustrySeriesBinding {
    std::string_view symbol;
    std::string_view field;
    bool carry_forward_near_zero;
};

inline constexpr std::array<IndustrySeriesBinding, 8> industry_series_bindings{{
    {"HY_INDEXO", "open", true},
    {"HY_INDEXH", "high", true},
    {"HY_INDEXL", "low", true},
    {"HY_INDEXC", "close", true},
    {"HY_INDEXA", "amount", false},
    {"HY_INDEXV", "volume", true},
    {"HY_INDEXADV", "extra_1", false},
    {"HY_INDEXDEC", "extra_2", false},
}};

inline constexpr std::array<std::string_view, 33> tdx_region_names{
    "",         "黑龙江",   "新疆板块", "吉林板块", "甘肃板块", "辽宁板块",
    "青海板块", "北京板块", "陕西板块", "天津板块", "广西板块", "河北板块",
    "广东板块", "河南板块", "宁夏板块", "山东板块", "上海板块", "山西板块",
    "深圳板块", "湖北板块", "福建板块", "湖南板块", "江西板块", "四川板块",
    "安徽板块", "重庆板块", "江苏板块", "云南板块", "浙江板块", "贵州板块",
    "海南板块", "西藏板块", "内蒙板块",
};

struct BroadIndexBinding {
    std::uint32_t source;
    std::string_view industry_index;
};

inline constexpr std::array<BroadIndexBinding, 13> broad_index_bindings{{
    {880001, "880091"}, {880002, "880091"}, {880003, "880091"},
    {880007, "880091"}, {999999, "880092"}, {1, "880092"},
    {888, "880092"},    {399001, "880093"}, {399006, "880096"},
    {399300, "880097"}, {300, "880097"},    {688, "880098"},
    {899050, "880099"},
}};

template <std::size_t Size>
bool has_any_dependency(const std::set<std::string> &dependencies,
                        const std::array<std::string_view, Size> &catalog) {
    for (const auto name : catalog)
        if (dependencies.count(std::string(name)))
            return true;
    return false;
}

} // namespace tdx::formula_context_detail
