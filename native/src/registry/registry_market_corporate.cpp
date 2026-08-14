#include "tdx/registry_internal.hpp"

#include "tdx/benchmark_analysis.hpp"
#include "tdx/calendar.hpp"
#include "tdx/company_changes.hpp"
#include "tdx/corporate_orders.hpp"
#include "tdx/curated_data.hpp"
#include "tdx/employees.hpp"
#include "tdx/equity_performance.hpp"
#include "tdx/event_impact.hpp"
#include "tdx/exchange_funds.hpp"
#include "tdx/financial_insights.hpp"
#include "tdx/financial_screen.hpp"
#include "tdx/fund_calendar.hpp"
#include "tdx/fund_reference.hpp"
#include "tdx/fund_statistics.hpp"
#include "tdx/gdr.hpp"
#include "tdx/global_performance.hpp"
#include "tdx/hk_actions.hpp"
#include "tdx/hk_events.hpp"
#include "tdx/hk_finance.hpp"
#include "tdx/overview_factors.hpp"
#include "tdx/patent_statistics.hpp"
#include "tdx/recent_watch.hpp"
#include "tdx/shareholder_signals.hpp"
#include "tdx/special_attention.hpp"
#include "tdx/special_situations.hpp"
#include "tdx/specialized_metrics.hpp"

namespace tdx::registry_detail {

// Corporate calendars, disclosures, financials and performance.
void append_market_corporate_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"market calendar", "财经与公司日历", "公开资料",
         "聚合宏观数据、热点会议、公司重大事项、境内IPO辅导/审核/申购/发行、北交所申购详情、美股IPO阶段及板块资讯。",
         true, "/api/v1/market/calendar", command_market_calendar},
        {"market employees", "员工、高管与持股计划", "公开资料",
         "读取员工、研发、人均指标、高管年薪和员工持股计划，并支持单票展开。",
         true, "/api/v1/market/employees", command_market_employees},
        {"market hk-events", "港股事件库", "公开资料",
         "聚合港股分红派息、权益披露、沽空统计和上市申请，并统一换算原始万/千单位。",
         true, "/api/v1/market/hk-events", command_market_hk_events},
        {"market hk-actions", "港股历史公司行动与复权因子", "客户端本地缓存",
         "解密hkqxinfo/hkqxinfo2，恢复长期分红、送股、供股、拆细/合股事件及客户端原生累计复权因子；资源来源仅返回TDX根相对路径。",
         true, "/api/v1/market/hk-actions", command_market_hk_actions},
        {"market hk-finance", "港股本地财务缓存", "客户端本地缓存",
         "解密hkcwdata，类型化股本、资产、营收、利润、股息、每股值、估值与币种折算标记，并保留全部TdxW原生字段；资源来源仅返回TDX根相对路径。",
         true, "/api/v1/market/hk-finance", command_market_hk_finance},
        {"market hk-short-history", "港股历史沽空量", "公开行情 + 公开资料",
         "读取 7727 港股日K辅助字段中的历史沽空股数，并与 GGRL104 每日沽空表逐日对账。",
         true, "/api/v1/market/hk-short-history", command_market_hk_short_history},
        {"market special-situations", "并购重组、三板转板与特殊事项", "公开资料 + L1",
         "聚合吸收合并、B转H现金选择权和主要指数市值管理阈值预警，并按公开L1复算溢价。",
         true, "/api/v1/market/special-situations", command_market_special_situations},
        {"market exchange-funds", "场内基金表现、套利与REITs", "公开资料 + L1",
         "聚合ETF多周期表现、货币ETF套利/收益和REITs发行项目，并按需补充公开L1复算。",
         true, "/api/v1/market/exchange-funds", command_market_exchange_funds},
        {"market fund-reference", "本地基金份额、净值与ETF/LOF标的", "客户端本地缓存",
         "严格读取specjjdata/specetfdata/speclofdata，恢复基金份额、交易参考值、已发布净值、ETF/LOF跟踪标的和原生生命周期状态。",
         true, "/api/v1/market/fund-reference", command_market_fund_reference},
        {"market curated-data", "客户端精选数据", "公开资料",
         "类型化传媒娱乐、低估值袖珍股、高分红、破净国企、转融券余额、港股表现、分红募资和拟回购统计。",
         true, "/api/v1/market/curated-data", command_market_curated_data},
        {"market special-attention", "特别关注与风险线索", "公开资料",
         "类型化股权分散、潜在ST、摘星摘帽、被立案调查和商誉风险五类客户端表。",
         true, "/api/v1/market/special-attention", command_market_special_attention},
        {"market fund-statistics", "基金发行、收益与市场统计", "公开资料",
         "类型化新发基金、基金分红、股票型基金收益、基金与ETF规模、申赎和新上市基金。",
         true, "/api/v1/market/fund-statistics", command_market_fund_statistics},
        {"market fund-calendar", "基金事件日历", "公开资料",
         "类型化基金开放、分红等事件日期、内容和基金类别，支持场内与场外基金查询。",
         true, "/api/v1/market/fund-calendar", command_market_fund_calendar},
        {"market specialized-metrics", "银行券商保险专项经营指标", "公开资料",
         "类型化银行资本、资产质量与息差，券商月度经营及业务收入，以及保险偿付能力、业务质量与投资结构。",
         true, "/api/v1/market/specialized-metrics", command_market_specialized_metrics},
        {"market company-changes", "证券与公司变更库", "公开资料",
         "类型化证券/公司更名、指数调整、重大股权、实控人、行业和股权转让九类事实。",
         true, "/api/v1/market/company-changes", command_market_company_changes},
        {"market financial-screen", "五板财务筛选", "公开资料",
         "类型化五板财务横截面，并提供通达信小盘成长三年复合增速股票池。",
         true, "/api/v1/market/financial-screen", command_market_financial_screen},
        {"market financial-insights", "特色财务线索", "公开资料",
         "类型化质量、分红、投资、现金应收、利润预警、现金流、稳健成长、连续质量增长与利润突破十五类线索。",
         true, "/api/v1/market/financial-insights", command_market_financial_insights},
        {"market gdr", "GDR与A股映射", "公开资料",
         "获取GDR行情、币种、上市地、发行与兑换比例，并关联A股折算价和溢价率。",
         true, "/api/v1/market/gdr", command_market_gdr},
        {"market equity-performance", "A股多周期表现", "公开资料",
         "类型化全A股收盘价、成交额、5/20/60日、本月和年初至今表现横截面。",
         true, "/api/v1/market/equity-performance", command_market_equity_performance},
        {"market corporate-orders", "招投标与重大合同", "公开资料",
         "类型化上市公司招投标、中标及重大合同，统一金额、营收占比和公告原文链接。",
         true, "/api/v1/market/corporate-orders", command_market_corporate_orders},
        {"market event-impact", "重大事件指数冲击", "公开资料",
         "对齐重大事件前后上证指数、恒生指数与纳斯达克指数表现，支持事件类型和日期筛选。",
         true, "/api/v1/market/event-impact", command_market_event_impact},
        {"market global-performance", "主要指数与海外中资表现", "公开资料",
         "类型化主要指数和海外中资证券的多周期表现，并以扩展市场目录补齐证券名称。",
         true, "/api/v1/market/global-performance", command_market_global_performance},
        {"market shareholder-signals", "牛散、机构与调研信号", "公开资料",
         "类型化牛散目录与逐人持仓，并提供机构增持、股东收缩、小市值专业机构增持、利润成长与机构调研活跃度。",
         true, "/api/v1/market/shareholder-signals", command_market_shareholder_signals},
        {"market recent-watch", "近期业绩与涉外经营关注", "公开资料",
         "类型化绩价背离、涉外经营敞口和ST业绩预盈三类近期关注表。",
         true, "/api/v1/market/recent-watch", command_market_recent_watch},
        {"market patent-statistics", "上市公司专利统计", "公开资料",
         "类型化深沪京公司本期专利申请、授权及累计授权数量，并保留来源总数与分类合计差异。",
         true, "/api/v1/market/patent-statistics", command_market_patent_statistics},
        {"market overview-factors", "大盘影响因素", "公开资料",
         "类型化资金、两融、ETF、货币、股指期货、外围市场、估值和股债收益差等客户端大盘判断。",
         true, "/api/v1/market/overview-factors", command_market_overview_factors},
        {"market benchmark-analysis", "牛熊阶段与相对基准分析", "公开资料",
         "还原十三段指数地标周期的个股/行业相对表现，以及停复牌和次新股基准表现。",
         true, "/api/v1/market/benchmark-analysis", command_market_benchmark_analysis}
    });
}

}  // namespace tdx::registry_detail
