#pragma once

#include "tdx/fund_analytics.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tdx::fund_analytics_detail {

using RowNormalizer = std::optional<Json> (*)(const Json& row);

enum class SortPolicy {
    none,
    date_ascending,
    month_ascending,
    report_date_descending,
};

std::optional<Json> normalize_risk_row(const Json& row);
std::optional<Json> normalize_risk_history_row(const Json& row);
std::optional<Json> normalize_monthly_risk_row(const Json& row);
std::optional<Json> normalize_monthly_history_row(const Json& row);
std::optional<Json> normalize_selection_skill_row(const Json& row);
std::optional<Json> normalize_reported_holdings_row(const Json& row);
std::optional<Json> normalize_reported_industry_row(const Json& row);
std::optional<Json> normalize_reported_security_row(const Json& row);
std::optional<Json> normalize_holdings_stability_row(const Json& row);
std::optional<Json> normalize_holding_industry_row(const Json& row);
std::optional<Json> normalize_holding_history_row(const Json& row);
std::optional<Json> normalize_position_estimate_row(const Json& row);
std::optional<Json> normalize_market_position_row(const Json& row);

struct ViewDefinition {
    const char* view;
    const char* request_id;
    const char* source_file;
    bool paged;
    bool requires_fund;
    const char* title;
    RowNormalizer normalize;
    SortPolicy sort;
};

inline constexpr std::array<ViewDefinition, 13> view_definitions{{
    {"risk", "500030", "jj_jzfx_fxsy.xml", true, false, "基金收益风险",
     normalize_risk_row, SortPolicy::none},
    {"risk-history", "500031", "jj_jzfx_fxsy.xml", false, true,
     "基金与基准日收益", normalize_risk_history_row, SortPolicy::date_ascending},
    {"monthly-risk", "500032", "jj_jzfx_ydfx.xml", true, false,
     "基金月度风险", normalize_monthly_risk_row, SortPolicy::none},
    {"monthly-history", "500033", "jj_jzfx_ydfx.xml", false, true,
     "基金与基准月收益", normalize_monthly_history_row, SortPolicy::month_ascending},
    {"selection-skill", "500035", "jj_jzfx_zsxg.xml", true, false,
     "基金择时选股能力", normalize_selection_skill_row, SortPolicy::none},
    {"reported-holdings", "500050", "jj_ccfx_dqcc.xml", true, false,
     "基金公开报告期持仓", normalize_reported_holdings_row, SortPolicy::none},
    {"reported-holding-industries", "500051", "jj_ccfx_dqcc.xml", false, true,
     "基金报告期行业持仓", normalize_reported_industry_row, SortPolicy::none},
    {"reported-holding-securities", "500052", "jj_ccfx_dqcc.xml", false, true,
     "基金报告期股票持仓", normalize_reported_security_row, SortPolicy::none},
    {"holdings-stability", "500055", "jj_ccfx_qjcc.xml", true, false,
     "基金持仓稳定性", normalize_holdings_stability_row, SortPolicy::none},
    {"holding-industries", "500056", "jj_ccfx_qjcc.xml", false, true,
     "基金区间行业持仓", normalize_holding_industry_row, SortPolicy::none},
    {"holding-history", "500057", "jj_ccfx_qjcc.xml", false, true,
     "基金报告期持仓", normalize_holding_history_row,
     SortPolicy::report_date_descending},
    {"position-estimates", "500060", "jj_cwgs.xml", true, false,
     "基金仓位估算", normalize_position_estimate_row, SortPolicy::none},
    {"market-position-history", "500062", "jj_cwgs_qsc.xml", false, false,
     "全市场基金仓位", normalize_market_position_row, SortPolicy::date_ascending},
}};

struct StyleDefinition {
    std::string_view code;
    std::string_view name;
};

inline constexpr std::array<StyleDefinition, 18> style_definitions{{
    {"", "全部"}, {"005001", "普通股票型"}, {"005002", "复制指数型"},
    {"005003", "增强指数型"}, {"005004", "平衡混合型"},
    {"005005", "偏股混合型"}, {"005006", "偏债混合型"},
    {"005007", "保本型"}, {"005008", "中短期纯债型"},
    {"005009", "长期纯债型"}, {"005010", "一级债基"},
    {"005011", "二级债基"}, {"005012", "债券指数型"},
    {"005013", "基金型"}, {"005014", "货币型"},
    {"005015", "短期理财债券型"}, {"005016", "贵金属商品"},
    {"005017", "其他商品"},
}};

struct BenchmarkDefinition {
    const char* code;
    const char* name;
};

inline constexpr std::array<BenchmarkDefinition, 3> benchmark_definitions{{
    {"999999", "上证指数"},
    {"000300", "沪深300"},
    {"000905", "中证500"},
}};

const ViewDefinition* view_definition(std::string_view view);
const StyleDefinition* style_definition(std::string_view code);
Json views_document();
Json benchmark_document(int benchmark);

struct CivilDate {
    int year{};
    int month{};
    int day{};
};

std::filesystem::path native_path(const std::string& value);
CivilDate today_local();
std::string date_text(const CivilDate& value);
CivilDate shift_months(CivilDate value, int months);
CivilDate shift_years(CivilDate value, int years);
CivilDate latest_full_fund_report(const CivilDate& value);
std::string previous_full_fund_report(const std::string& value);
std::string date_days_ago(int days);
bool digits(const std::string& value);
std::string compact_date(std::string value, std::string_view name);
std::string display_date(const std::string& value);
std::string now_text();
const Json* field(const Json& row, std::string_view key);
std::string text(const Json& row, std::string_view key);
std::optional<double> number(const Json& row, std::string_view key);
Json number_json(const std::optional<double>& value);
Json scaled_number_json(const Json& row, std::string_view key, double scale);
Json optional_date(const std::string& value);
int integer(const std::string& raw, std::string_view name,
            int minimum, int maximum);
Json managers(const std::string& raw);
Json fund_document(const Json& row);
Json common_fund_record(const Json& row);
bool transient_error(const std::string& message);
std::string cache_key(const FundAnalyticsQuery& query);

}  // namespace tdx::fund_analytics_detail
