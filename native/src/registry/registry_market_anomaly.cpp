#include "tdx/registry_internal.hpp"

#include "tdx/abnormal_details.hpp"
#include "tdx/abnormal_moves.hpp"
#include "tdx/anomaly_risk.hpp"
#include "tdx/auction.hpp"
#include "tdx/index_volatility.hpp"
#include "tdx/lhb.hpp"
#include "tdx/profit_gaps.hpp"
#include "tdx/total_return_gap.hpp"

namespace tdx::registry_detail {

// LHB, auctions, abnormal moves and risk/volatility gaps.
void append_market_anomaly_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"market lhb", "龙虎榜事件与席位", "公开行情",
         "聚合八张龙虎榜视图，并按股票或事件展开营业部买卖席位明细。",
         true, "/api/v1/market/lhb", command_market_lhb},
        {"market auction", "集合竞价序列", "公开行情",
         "通过 0x056A 获取开盘与收盘集合竞价价格、匹配量和未匹配方向。",
         true, "/api/v1/market/auction", command_market_auction},
        {"market abnormal-moves", "交易异常与停牌核查", "公开行情",
         "获取十九类交易异常证券，支持六个市场分支和最新可用交易日回退。",
         true, "/api/v1/market/abnormal-moves", command_market_abnormal_moves},
        {"market abnormal-details", "异常证券摘要与上榜原因", "公开行情",
         "获取异常证券刷新时间、上游准确率/条数，并将单证券上榜原因、区间和累计量额结构化。",
         true, "/api/v1/market/abnormal-details", command_market_abnormal_details},
        {"market anomaly-risk", "异常波动与停牌风险", "公开行情",
         "获取个股相对分类指数的3/10/30日异常统计，以及异动停牌、复牌和触发预警。",
         true, "/api/v1/market/anomaly-risk", command_market_anomaly_risk},
        {"market profit-gaps", "利润断层", "公开行情",
         "获取业绩预告、快报或定期报告披露后的价格断层，并复现客户端安全分过滤。",
         true, "/api/v1/market/profit-gaps", command_market_profit_gaps},
        {"market index-volatility", "指数已实现波动率", "公开行情",
         "获取指数滚动已实现波动率目录、历史序列以及单指数均值和历史分位。",
         true, "/api/v1/market/index-volatility", command_market_index_volatility},
        {"market total-return-gap", "全收益指数收益差", "公开行情",
         "比较价格指数与对应全收益指数的区间收益，量化股息再投资贡献。",
         true, "/api/v1/market/total-return-gap", command_market_total_return_gap}
    });
}

}  // namespace tdx::registry_detail
