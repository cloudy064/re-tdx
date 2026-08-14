#include "tdx/registry_internal.hpp"

#include "tdx/bond_reference.hpp"
#include "tdx/convertible_bonds.hpp"
#include "tdx/economic_indicators.hpp"
#include "tdx/etf_flows.hpp"
#include "tdx/hot_history.hpp"
#include "tdx/historical_securities.hpp"
#include "tdx/index_events.hpp"
#include "tdx/market.hpp"
#include "tdx/professional_data.hpp"
#include "tdx/security_directory.hpp"
#include "tdx/strategic_themes.hpp"
#include "tdx/thematic_opportunities.hpp"
#include "tdx/theme_library.hpp"

namespace tdx::registry_detail {

// Market depth, security directories and thematic catalogs.
void append_market_reference_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"market depth", "买卖五档", "公开行情",
         "通过 0x0547 获取买卖五档、档位量和买一/卖一金额。",
         true, "/api/v1/market/depth", command_market_depth},
        {"market etf-flows", "ETF 持股与资金动向", "公开资料",
         "聚合股票与研究行业的 ETF 持股规模、宽基/主题申赎净流入和周交易资金。",
         true, "/api/v1/market/etf-flows", command_market_etf_flows},
        {"market securities", "服务端证券目录", "公开行情",
         "通过 0x044E 获取市场证券数，再用 0x044D 分页下载股票、指数、ETF、债券和可转债目录。",
         true, "/api/v1/market/securities", command_market_securities},
        {"market professional", "单期/多季度专业财务与交易数据", "公开资料",
         "下载并校验通达信官方 gpcw/gp 数据包，查询 FINVALUE、GPJYVALUE 与 SCJYVALUE 原始字段和历史。",
         true, "/api/v1/market/professional", command_market_professional},
        {"market convertible-bonds", "可转债定价、发行投影、条款及待发方案", "公开资料 + L1",
         "聚合待发方案及客户端投影、已发申购/新债筛选、已上市可转债与可交换债双投影、定价、票息和动态条款，并关联正股及保留来源差异。",
         true, "/api/v1/market/convertible-bonds", command_market_convertible_bonds},
        {"market bond-reference", "债券条款资料库", "公开资料",
         "按信用评级、利率类型或债券类别懒加载条款、评级、完整付息序列，并对账政策性金融债沪深投影。",
         true, "/api/v1/market/bond-reference", command_market_bond_reference},
        {"market economic-indicators", "经济指标与关联股票", "公开资料",
         "获取价格与景气指标的当前值、环比/同比、完整历史曲线和相关股票，并按需补充公开 L1 行情。",
         true, "/api/v1/market/economic-indicators", command_market_economic_indicators},
        {"market strategic-themes", "战略主题关系", "板块与行业",
         "获取 26 个战略主题大类、598 个内部主题、成分股及逐股入选逻辑。",
         true, "/api/v1/market/strategic-themes", command_market_strategic_themes},
        {"market theme-library", "统一主题库", "板块与行业",
         "获取统一主题、区域经济、国企系、公司系和参股持股快照，以及逐股纳入原因和主题指数历史。",
         true, "/api/v1/market/theme-library", command_market_theme_library},
        {"market thematic-opportunities", "行业区域机会与爆炒复盘", "板块与行业",
         "获取一带一路细分行业/核心区域分组、带页面标签冲突的遗留客户端主题、逐股投资逻辑，以及近期已爆炒和正在爆炒股票。",
         true, "/api/v1/market/thematic-opportunities", command_market_thematic_opportunities},
        {"market hot-history", "K线历史热点区间", "客户端本地缓存",
         "严格读取 speczshot，恢复多年热点证券、K线起止区间、区间/峰值收益、驱动主题和完整分析文本。",
         true, "/api/v1/market/hot-history", command_market_hot_history},
        {"market historical-securities", "历史证券兼容名称", "客户端本地缓存",
         "严格读取 pttab，查询当前及历史证券的市场、六位代码和完整名称，并与当前证券目录对账。",
         true, "/api/v1/market/historical-securities", command_market_historical_securities},
        {"market index-events", "指数图重大事件注记", "客户端本地缓存",
         "严格读取 speczsevent 双表，恢复上证、恒生、纳指图上的事件发生日、目标交易日、正文 recid 和原生跳转目标。",
         true, "/api/v1/market/index-events", command_market_index_events}
    });
}

}  // namespace tdx::registry_detail
