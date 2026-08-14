#pragma once

#include "tdx/exchange_funds.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::exchange_fund_detail {

struct ResourceDefinition {
    std::string_view resource;
    std::string_view kind;
};

inline constexpr std::array<ResourceDefinition, 22> resource_catalog{{
    {"list/func_etfhq101.jsn", "etf-performance"},
    {"list/func_tlfeyxetf101_1.jsn", "etf-share-ranking"},
    {"list/gxjty_etfjj101.jsn", "etf-scale-flow"},
    {"list/gxjty_etfjj102.jsn", "commodity-etf"},
    {"list/gxjty_etfjj103.jsn", "cash-arbitrage"},
    {"list/gxjty_etfjj104.jsn", "cash-yield"},
    {"list/gxjty_etfjj105.jsn", "etf-scale-flow"},
    {"list/gxjty_etfjj106.jsn", "etf-scale-flow"},
    {"list/gxjty_etfjj107.jsn", "etf-scale-flow"},
    {"list/gxjty_lofjj101.jsn", "lof"},
    {"list/gxjty_lofjj102.jsn", "lof"},
    {"list/gxjty_lofjj103.jsn", "lof"},
    {"list/gxjty_lofjj104.jsn", "lof"},
    {"list/gxjty_lofjj105.jsn", "lof"},
    {"list/gxjty_lofjj106.jsn", "lof"},
    {"list/gxjty_lofjj107.jsn", "lof"},
    {"list/gxjty_fbjj101.jsn", "closed-fund"},
    {"list/gxjty_fbjj102.jsn", "closed-fund"},
    {"list/gxjty_xjgl101.jsn", "cash-management-calendar"},
    {"list/gxjty_xjgl104.jsn", "cash-management-calendar"},
    {"list/func_reits101_1.jsn", "reit-issued"},
    {"list/func_reits102_1.jsn", "reit-pipeline"},
}};

struct KindDefinition {
    std::string_view kind;
    std::string_view view;
    std::string_view label;
    int rank;
};

inline constexpr std::array<KindDefinition, 11> kind_catalog{{
    {"etf-performance", "etf-performance", "ETF多周期表现", 0},
    {"etf-share-ranking", "etf-share-ranking", "ETF份额与净流入榜", 1},
    {"etf-scale-flow", "etf-scale-flow", "ETF规模与份额变化", 2},
    {"commodity-etf", "commodity-etf", "商品ETF及跟踪标的", 3},
    {"lof", "lof", "LOF净值、配置与申赎", 4},
    {"closed-fund", "closed-fund", "封闭式基金折溢价", 5},
    {"cash-arbitrage", "cash-arbitrage", "货币ETF套利", 6},
    {"cash-yield", "cash-yield", "场内货币基金收益", 7},
    {"cash-management-calendar", "cash-management-calendar", "现金管理申赎日历", 8},
    {"reit-pipeline", "reits-pipeline", "待发行REITs", 9},
    {"reit-issued", "reits-issued", "已发行REITs", 10},
}};

const KindDefinition* find_kind(std::string_view kind);
const KindDefinition* find_view(std::string_view view);
const std::vector<std::string>& resource_names();
std::string resource_kind(std::string_view resource);
std::string kind_label(std::string_view kind);
std::string view_kind(std::string_view view);
bool valid_view(std::string_view view);
int kind_rank(std::string_view kind);

const Json* field(const Json& row, std::string_view name);
std::string text_value(const Json& row, std::string_view name);
std::optional<double> number_value(const Json& row, std::string_view name);
Json number(const Json& row, std::string_view name);
Json change_pct(const std::optional<double>& current,
                const std::optional<double>& reference);
bool digits(const std::string& value);
int integer_value(const Json& row, std::string_view name, int fallback = -1);
int parsed_market(const std::string& value);
Json security_document(
    int source_market, const std::string& code, const std::string& source_name,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json reference_instrument(const Json& row);
std::pair<std::optional<double>, std::optional<double>> price_range(
    const std::string& value);
std::string now_text();
std::filesystem::path native_path(const std::string& value);
Json load_local_resource_rows(const std::filesystem::path& root,
                              const std::string& resource);
const Json& document_for(const Json& documents, std::string_view resource);
Json source_summary(const Json& document, std::size_t normalized_rows);
int bounded(const std::string& value, std::string_view name,
            int low, int high);
Json quote_for(const Json& security,
               const std::map<std::pair<int, std::string>, Json>& quotes);

}  // namespace tdx::exchange_fund_detail
