#pragma once

#include "tdx/technical_signals.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tdx::technical_signals_detail {

struct ModelStrategyDefinition {
    const char* view;
    const char* xg_name;
    const char* title;
    const char* rule;
};

struct AuctionStrategyDefinition {
    const char* view;
    const char* request_id;
    const char* title;
};

struct FactorSignalDefinition {
    const char* view;
    const char* flag;
    const char* title;
    const char* first_code;
    const char* first_label;
    const char* first_polarity;
    const char* second_code;
    const char* second_label;
    const char* second_polarity;
    const char* rule;
};

struct OpportunityDefinition {
    const char* view;
    const char* flag;
    const char* title;
    const char* rule;
    bool safety_filter;
};

inline constexpr std::array<ModelStrategyDefinition, 7> model_strategies{{
    {"model-new-high", "CXGXG", "创新高", "昨日实际流通市值大于30亿元，且当日最高价等于上市以来最高价"},
    {"two-day-event", "SJQUXG_QLR", "前两日事件驱动", "实际流通市值大于30亿元，开盘较昨收上涨超过1%，收盘高于开盘且较昨收上涨超过4%"},
    {"liquidity-space", "LDXKJXG", "流动空间", "昨日成交额大于15亿元，并按昨日实际换手率是否达到5%应用客户端三日/两日涨幅门槛"},
    {"limit-break", "ZTKBXG", "涨停开板", "昨日实际流通市值大于30亿元，当日曾涨停后开板，且开盘至合格开板时成交额大于4亿元"},
    {"low-turnover-chase", "FLOW_UP_LOW_XG", "低换手追涨", "三日成交额均值大于18亿元、昨日实际换手率低于5%的前50名池中，取当日成交额前30且涨幅前6"},
    {"high-turnover-chase", "FLOW_UP_HIGH_XG", "高换手追涨", "三日成交额均值大于18亿元、昨日实际换手率高于5%的前50名池中，取当日成交额前20且涨幅前6"},
    {"high-liquidity-enhance", "GLDXZQ_XG", "高流动性增强", ""},
}};

inline constexpr std::array<AuctionStrategyDefinition, 7> auction_strategies{{
    {"weak-limit-reversal", "200400", "烂板转强"},
    {"failed-limit-reversal", "200401", "炸板转强"},
    {"auction-bottom-reversal", "200402", "竞价止跌"},
    {"upper-shadow-engulf", "200403", "预吞上影"},
    {"auction-volume-spike", "200404", "竞价爆量"},
    {"limit-up-gap", "200405", "涨停高开"},
    {"five-minute-volume-surge", "200406", "5分钟陡增"},
}};

inline constexpr std::array<FactorSignalDefinition, 6> factor_signals{{
    {"accumulation-surge", "1", "积突信号", "积", "量价连续上扬并创新高", "positive", "突", "积信号后再次满足条件", "positive", "积：连续数分钟成交量、均价上扬，现价创新高且振幅、涨速满足条件；突：在积信号基础上再次满足上述条件。"},
    {"new-high-low-120", "2", "120日新高低", "高", "创120日新高", "positive", "低", "创120日新低", "negative", "高：创120日新高；低：创120日新低。"},
    {"ma120-cross", "3", "120日均线上下", "上", "上穿120日均线", "positive", "下", "跌破120日均线", "negative", "上：上穿120日均线；下：跌破120日均线。"},
    {"ma-break-reclaim", "4", "均线破立", "破", "一阴跌破5/10/20日均线", "negative", "立", "一阳上穿5/10/20日均线", "positive", "破：一阴跌破5、10、20日均线；立：一阳上穿5、10、20日均线。"},
    {"pullback-rally", "5", "缩量踩放量拉", "踩", "缩量回踩5日线", "negative", "拉", "放量上涨", "positive", "踩：缩量回踩5日线，且回踩前有一定涨幅；拉：放量上涨。"},
    {"ma-support-pressure", "6", "均线托压", "托", "5/10/20日均线向上形成价托", "positive", "压", "5/10/20日均线向下形成价压", "negative", "托：5、10、20日均线扭转向上形成封闭三角形；压：三条均线扭转向下形成封闭三角形。"},
}};

inline constexpr std::array<OpportunityDefinition, 2> opportunity_signals{{
    {"intraday-opportunity", "1", "个股机会", "基于分时级别数据计算，要求安全分大于60。", true},
    {"t0-opportunity", "2", "T+0机会", "基于分时级别数据计算，筛选标的支持T+0交易。", false},
}};

inline constexpr std::array<std::string_view, 32> technical_signal_views{{
    "nine-turn", "rps-stock", "rps-block", "new-high", "new-low",
    "breakout", "strong-start", "trend-up", "trend-down", "event-driven",
    "model-new-high", "two-day-event", "liquidity-space", "limit-break",
    "low-turnover-chase", "high-turnover-chase", "high-liquidity-enhance",
    "weak-limit-reversal", "failed-limit-reversal", "auction-bottom-reversal",
    "upper-shadow-engulf", "auction-volume-spike", "limit-up-gap",
    "five-minute-volume-surge", "accumulation-surge", "new-high-low-120",
    "ma120-cross", "ma-break-reclaim", "pullback-rally",
    "ma-support-pressure", "intraday-opportunity", "t0-opportunity",
}};

std::filesystem::path native_path(const std::string& value);
std::string now_text();
const Json* field(const Json& row, std::string_view key);
const Json* prefix_field(const Json& row, std::string_view prefix);
std::string text(const Json& row, std::string_view key);
std::optional<double> number_value(const Json* value);
std::optional<double> number(const Json& row, std::string_view key);
Json number_json(const std::optional<double>& value);
std::string date_text(std::string raw);
std::string time_text(std::string raw);
int market_id(std::string value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(const Json& row, const BlockData& blocks);
Json block_document(const Json& row, const BlockData& blocks);
void add_rps_period(
    Json& periods,
    int days,
    const Json& row,
    std::string_view rps,
    std::string_view growth);
const ModelStrategyDefinition* model_strategy(const std::string& view);
const AuctionStrategyDefinition* auction_strategy(const std::string& view);
const FactorSignalDefinition* factor_signal(const std::string& view);
const OpportunityDefinition* opportunity_signal(const std::string& view);
Json factor_signal_semantics(
    const FactorSignalDefinition& definition,
    const std::string& signal_code);
std::string factor_time_text(std::string raw);
bool known_view(const std::string& view);
bool passes_client_filter(
    const Json& record,
    const TechnicalSignalsQuery& query);
Json client_filters(const TechnicalSignalsQuery& query);
int bounded(
    const std::string& raw,
    std::string_view name,
    int minimum,
    int maximum);
int board_id(const std::string& raw);
std::string board_name(int id);
std::string cache_key(const TechnicalSignalsQuery& query);
Json parameters_document(const TechnicalSignalsQuery& query);
std::string selected_board(int selected_market, const std::string& code);
Json enrich_factor_quotes(
    const std::filesystem::path& root,
    Json& records,
    const BlockData& blocks,
    int timeout_ms);

}  // namespace tdx::technical_signals_detail
