#include "tdx/registry_internal.hpp"

#include "tdx/block_backtest.hpp"
#include "tdx/equity_valuation.hpp"
#include "tdx/factors.hpp"
#include "tdx/flow_followup.hpp"
#include "tdx/fund_analytics.hpp"
#include "tdx/ownership.hpp"
#include "tdx/ratings.hpp"
#include "tdx/relative_valuation.hpp"
#include "tdx/repurchases.hpp"
#include "tdx/stats.hpp"
#include "tdx/technical_signals.hpp"
#include "tdx/tender_offers.hpp"
#include "tdx/trades.hpp"
#include "tdx/unlocks.hpp"
#include "tdx/valuation.hpp"

namespace tdx::registry_detail {

// Technical signals, factors, ownership actions and valuation models.
void append_market_valuation_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"market technical-signals", "通达信技术选股", "公开行情",
         "获取九转、RPS、趋势、模型、竞价、盘中/T+0机会及六类盘面因子等三十二类选股，支持单票反查及快照差异。",
         true, "/api/v1/market/technical-signals", command_market_technical_signals},
        {"market factors", "因子研究中心", "公开行情",
         "获取普通/形态因子目录、完整关系矩阵、单票反查、市场广度/共现、双视图对账与原子快照差异。",
         true, "/api/v1/market/factors", command_market_factors},
        {"market fund-analytics", "基金风险与能力分析", "公开行情",
         "获取基金风险收益、月度波动、择时选股能力、公开报告期行业/股票持仓、持仓稳定性及仓位估算十三类视图。",
         true, "/api/v1/market/fund-analytics", command_market_fund_analytics},
        {"market block-backtest", "板块历史回测", "公开行情",
         "按区间比较行业、概念、风格、地域板块，并展开服务端选定的板块成分股表现。",
         true, "/api/v1/market/block-backtest", command_market_block_backtest},
        {"market ratings", "港美股与行业评级", "公开资料",
         "聚合港股、美股机构评级/目标价及一级研究行业看多看空统计，并按证券或行业展开历史。",
         true, "/api/v1/market/ratings", command_market_ratings},
        {"market ownership", "股权变动分析", "公开资料",
         "聚合股东及董监高变动、六类增减持榜、五市场股东人数、承诺不减持、股权质押风险、统计和质押机构，并展开动态历史。",
         true, "/api/v1/market/ownership", command_market_ownership},
        {"market repurchases", "股份回购分析", "公开资料",
         "聚合 A 股回购方案、月度与年度统计，并展开港股单票逐笔回购历史。",
         true, "/api/v1/market/repurchases", command_market_repurchases},
        {"market tender-offers", "要约收购事件", "公开资料",
         "读取全市场要约收购历史、当前进度、拟定与实际股数/比例/资金、期限、过户、退市标记及收购目的，并支持单票反查。",
         true, "/api/v1/market/tender-offers", command_market_tender_offers},
        {"market stats", "在线统计资源", "公开行情",
         "通过 0x06B9 下载 zhb.zip，解析封单、竞价与涨停，并可按 TdxW 原生口径联算动态/静态/TTM 市盈率和 MRQ 市净率。",
         true, "/api/v1/market/stats", command_market_stats},
        {"market trades", "L1 成交明细", "公开行情",
         "通过 0x0FC5/0x0FC6 获取当日或历史成交明细、分钟聚合与竞价撮合。",
         true, "/api/v1/market/trades", command_market_trades},
        {"market unlocks", "限售解禁日历", "公开资料",
         "读取近期限售解禁计划，合并同日批次并按事件展开具体解禁股东。",
         true, "/api/v1/market/unlocks", command_market_unlocks},
        {"market valuation", "市场指数估值", "公开资料",
         "读取市场指数 PE/PB、历史百分位、完整日序列和关联 ETF。",
         true, "/api/v1/market/valuation", command_market_valuation},
        {"market relative-valuation", "指数相对估值", "公开资料",
         "比较目标指数与基准指数的 PE/PB/PS 估值比、历史分位和每日走势。",
         true, "/api/v1/market/relative-valuation", command_market_relative_valuation},
        {"market equity-valuation", "个股与行业估值模型", "公开资料",
         "复现个股及一级行业 PE 历史/预期估值和 PB-ROE 回归带五类模型。",
         true, "/api/v1/market/equity-valuation", command_market_equity_valuation},
        {"market flow-followup", "资金信号后续表现", "公开资料",
         "提供两融/北向逐日历史及融资、融券、北向净流入/净买入四组分档模型，隔离上游占位值。",
         true, "/api/v1/market/flow-followup", command_market_flow_followup}
    });
}

}  // namespace tdx::registry_detail
