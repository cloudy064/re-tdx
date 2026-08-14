#include "tdx/registry_internal.hpp"

#include "tdx/announcement_signals.hpp"
#include "tdx/block_rotation.hpp"
#include "tdx/capital_strength.hpp"
#include "tdx/commodity_links.hpp"
#include "tdx/consensus.hpp"
#include "tdx/corporate.hpp"
#include "tdx/exchange_supervision.hpp"
#include "tdx/intelligence.hpp"
#include "tdx/leverage.hpp"
#include "tdx/limit_ladder.hpp"
#include "tdx/limit_quality.hpp"
#include "tdx/limit_review.hpp"
#include "tdx/market.hpp"
#include "tdx/panorama.hpp"
#include "tdx/ranking.hpp"
#include "tdx/research.hpp"
#include "tdx/reverse_repo.hpp"
#include "tdx/roadshows.hpp"
#include "tdx/session_turnover.hpp"
#include "tdx/strong_stocks.hpp"
#include "tdx/threshold_stocks.hpp"

namespace tdx::registry_detail {

// Consensus, capital, limit sessions, supervision and connect flows.
void append_market_session_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"market consensus", "一致预期与研报", "公开资料",
         "读取九类一致预期、评级、业绩增速和价格阶段榜单，并按股票展开机构评级、目标价、三年预测和研报正文。",
         true, "/api/v1/market/consensus", command_market_consensus},
        {"market panorama", "个性数据全景", "公开资料",
         "类型化整合量化评级、资金流向、风险、财报、预告、机构预测、送配、龙虎榜、两融和增减持十类全市场/单票 JSN。",
         true, "/api/v1/market/panorama", command_market_panorama},
        {"market finance", "批量财务基础信息", "公开资料",
         "通过 0x0010 批量获取股本、EPS、资产负债、收入利润和报告日期。",
         true, "/api/v1/market/finance", command_market_finance},
        {"market capital", "股本变迁与除权", "公开资料",
         "通过 0x000F 或本地加密 GBBQ 获取除权除息、股本变化、增发、回购和重整事件。",
         true, "/api/v1/market/capital", command_market_capital},
        {"market limits", "特殊涨跌停表", "公开行情",
         "通过 0x0452 分页扫描特殊品种涨跌停价，作为普通交易规则的覆盖层。",
         true, "/api/v1/market/limits", command_market_limits},
        {"market research", "机构调研与活动", "公开资料",
         "聚合机构调研、互动问答、行业热度、监管事件和知名机构，并按动态键展开详情。",
         true, "/api/v1/market/research", command_market_research},
        {"market roadshows", "上市公司路演", "公开资料",
         "读取无 ReqId 的 TQLEX KV 路由，提供全市场路演日历及单票历史。",
         true, "/api/v1/market/roadshows", command_market_roadshows},
        {"market snapshot", "行情快照", "公开行情",
         "通过 0x054C 批量获取最新价、昨收、成交量额等行情字段。",
         true, "/api/v1/market/snapshot", command_market_snapshot},
        {"market speed", "涨速与五档快照", "公开行情",
         "通过 0x053E 批量获取服务端涨速、最新行情和买卖五档。",
         true, "/api/v1/market/speed", command_market_speed},
        {"market ranking", "全市场实时排序", "公开行情",
         "通过 0x054B 获取涨速、涨幅、成交额、封单额、开盘抢筹等服务端榜单。",
         true, "/api/v1/market/ranking", command_market_ranking},
        {"market limit-quality", "涨停与竞价质量", "公开行情",
         "合并封板榜、五档、在线统计和竞价序列，计算封流比、封昨比、封单衰减与抢筹复核。",
         true, "/api/v1/market/limit-quality", command_market_limit_quality},
        {"market limit-review", "涨跌停复盘", "公开资料",
         "类型化还原通达信非实时涨跌停原因、涨停基因、年度行为、市场历史和指定日成员。",
         true, "/api/v1/market/limit-review", command_market_limit_review},
        {"market session-turnover", "开盘与盘后成交", "公开行情",
         "读取 A 股或 ETF 统计日总成交、开盘成交和盘后成交，并按金额、占比或价格变化排序。",
         true, "/api/v1/market/session-turnover", command_market_session_turnover},
        {"market block-rotation", "行业/概念/地区/风格板块轮动", "公开行情",
         "读取四类板块上次异动、平均周期及周/月/季/年上涨下跌异动数和区间涨幅。",
         true, "/api/v1/market/block-rotation", command_market_block_rotation},
        {"market limit-ladder", "行业/概念连板天梯", "公开行情",
         "读取研究行业与概念板块的封板、炸板、昨板、连板、最高高度、总高度和晋级率。",
         true, "/api/v1/market/limit-ladder", command_market_limit_ladder},
        {"market threshold-stocks", "百元股与千亿市值", "公开行情",
         "读取百元股、千亿市值的历史家数、入围跌出、日期名单和家数走势图。",
         true, "/api/v1/market/threshold-stocks", command_market_threshold_stocks},
        {"market capital-strength", "DDX 资金强势", "公开行情",
         "读取五日、十日、二十日、三十日和近三月 DDX 前百榜，并聚合跨周期共振和单票命中。",
         true, "/api/v1/market/capital-strength", command_market_capital_strength},
        {"market strong-stocks", "强势股生命周期", "公开资料",
         "读取历史强势股连板区间，并按区间展开每日涨幅、成交额、涨停原因和全市场涨跌停温度。",
         true, "/api/v1/market/strong-stocks", command_market_strong_stocks},
        {"market commodity-links", "商股联动与涨价题材", "公开资料",
         "读取商品价格、多周期涨跌、关联股票与行业/ETF，并展开涨价题材、历史驱动事件及单票关系。",
         true, "/api/v1/market/commodity-links", command_market_commodity_links},
        {"market announcement-signals", "公告精选与风险提示", "公开资料",
         "读取通达信公告精选、风险提示公告、公告 PDF 链接及单票公告前后表现历史。",
         true, "/api/v1/market/announcement-signals", command_market_announcement_signals},
        {"market reverse-repo", "国债逆回购收益", "公开行情",
         "结合当日交收日历和公开 L1 年化利率，计算指定本金的毛收益、手续费、净收益与可用/可取日。",
         true, "/api/v1/market/reverse-repo", command_market_reverse_repo},
        {"market exchange-supervision", "交易所监管观察期", "公开资料",
         "读取当前及历史监管证券，复现监管起止价区间收益，并关联公告 PDF 与当前公开 L1 行情。",
         true, "/api/v1/market/exchange-supervision", command_market_exchange_supervision},
        {"market margin", "融资融券", "公开资料",
         "读取市场两融、转融资/转融券序列、十三类证券排行，并按证券展开近三月余额与余量历史。",
         true, "/api/v1/market/margin", command_market_margin},
        {"market stock-connect", "沪深港通", "公开资料",
         "读取陆港通日周资金、当前与季度持仓、增减仓历史快照、分类资金和单日十大活跃股，并按证券展开历史持股。",
         true, "/api/v1/market/stock-connect", command_market_stock_connect},
        {"market intelligence", "市场关注与事件风险", "公开资料",
         "聚合全市场关注度、价值关注、风险观察、潜在爆雷、失信被执行对象、事件驱动、部委要闻和热点股票关系图。",
         true, "/api/v1/market/intelligence", command_market_intelligence}
    });
}

}  // namespace tdx::registry_detail
