#pragma once

#include "tdx/institution_analysis.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::institution_analysis_detail {

enum class Layout {
    holdings, pensions, float_structure, crowded_oversold, exclusive_funds,
    notable_private_funds, national_team, social_security_summary,
    development_bank, named_holding, named_holding_summary, stake_building
};

struct ViewSpec {
    const char* id;
    const char* label;
    const char* resource;
    Layout layout;
    bool total_ratio_divide_100{};
};

inline constexpr std::array<ViewSpec, 25> view_catalog{{
    {"all", "全部机构", "list/func_cgfx101_1.jsn", Layout::holdings},
    {"brokers", "券商", "list/func_cgfx102_1.jsn", Layout::holdings},
    {"insurers", "保险", "list/func_cgfx103_1.jsn", Layout::holdings},
    {"social-security", "社保", "list/func_cgfx104_1.jsn", Layout::holdings},
    {"private-funds", "私募", "list/func_cgfx105_1.jsn", Layout::holdings},
    {"public-funds", "公募", "list/func_cgfx106_1.jsn", Layout::holdings},
    {"exclusive-funds", "基金独门", "list/func_tbgz108_1.jsn", Layout::exclusive_funds},
    {"notable-private-funds", "知名私募", "list/func_jgcg108_1.jsn", Layout::notable_private_funds},
    {"banks", "银行", "list/func_cgfx107_1.jsn", Layout::holdings},
    {"finance-companies", "财务公司", "list/func_cgfx108_1.jsn", Layout::holdings},
    {"annuities", "年金", "list/func_cgfx109_1.jsn", Layout::holdings},
    {"general-corporates", "一般法人", "list/func_cgfx110_1.jsn", Layout::holdings},
    {"qfii", "QFII", "list/func_cgfx111_1.jsn", Layout::holdings},
    {"trusts", "信托", "list/func_cgfx112_1.jsn", Layout::holdings},
    {"special-corporates", "特殊法人", "list/func_cgfx113_1.jsn", Layout::holdings},
    {"pensions", "养老金", "list/func_cgfx114_1.jsn", Layout::pensions},
    {"float-structure", "浮筹与机构持股结构", "list/func_cgfx115_1.jsn", Layout::float_structure},
    {"crowded-oversold", "前期抱团股超跌", "list/func_cgfx116_1.jsn", Layout::crowded_oversold},
    {"northbound", "北向资金", "list/func_cgfx117_1.jsn", Layout::holdings, true},
    {"national-team", "汇金证金", "list/func_tzcg104_1.jsn", Layout::national_team},
    {"social-security-summary", "社保流通股东汇总", "list/func_sbltgd101_1.jsn", Layout::social_security_summary},
    {"development-bank-holdings", "国开系参股持仓", "list/func_tzcg105_1.jsn", Layout::development_bank},
    {"wutong-holdings", "梧桐树系持股", "list/func_tzcg106_1.jsn", Layout::named_holding},
    {"zhongke-huitong-holdings", "中科汇通持股", "list/func_tzcg109_1.jsn", Layout::named_holding_summary},
    {"stake-building", "被举牌", "list/func_tzcg108_1.jsn", Layout::stake_building},
}};

const std::array<ViewSpec, 25>& views();
const ViewSpec* find_view(const std::string& id);
const char* layout_name(Layout value);
const Json* value_ptr(const Json& object, std::string_view name);
Json copy_value(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
bool parse_number(const Json& object, std::string_view name, double& result);
Json number_or_null(const Json& object, std::string_view name, double scale = 1.0);
Json quotient_or_null(double numerator, double denominator);
int market_id(const std::string& value);
std::string market_name(int value);
std::string market_prefix(int value);
bool six_digits(const std::string& value);
Json fields_for(Layout layout);
Json catalog_document();
const Json& document_for(const Json& documents, std::string_view resource);
Json holding_data(const ViewSpec& view, const Json& raw);
Json normalize_data(const ViewSpec& view, const Json& raw);
Json normalize_row(const ViewSpec& view, const Json& raw,
                   const std::map<std::pair<int, std::string>, Security>& securities);
bool row_matches(const Json& row, const InstitutionAnalysisQuery& options,
                 int selected_market);
double data_number(const Json& row, std::string_view key);
void sort_special_records(const ViewSpec& view, Json& rows);
Json section_summary(const ViewSpec& view, const Json& rows);
Json section_document(
    const ViewSpec& view, const Json& source,
    const InstitutionAnalysisQuery& options,
    const std::map<std::pair<int, std::string>, Security>& securities);
std::string now_text();
int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum);
std::filesystem::path from_utf8(const std::string& value);

}  // namespace tdx::institution_analysis_detail
