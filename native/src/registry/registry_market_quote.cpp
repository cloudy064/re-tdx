#include "tdx/registry_internal.hpp"

#include "tdx/block_trades.hpp"
#include "tdx/corporate.hpp"
#include "tdx/funds.hpp"
#include "tdx/level2.hpp"
#include "tdx/market.hpp"
#include "tdx/minute.hpp"
#include "tdx/options.hpp"

namespace tdx::registry_detail {

// Level2 sessions, K-line, instruments, options and expansion quotes.
void append_market_quote_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"level2 build", "L2 请求离线构造", "授权行情研究",
         "离线构造已恢复的 direct 1363/1364/1373/1374、TdxW 1369/1371、SDK redirect 请求字节、fnReqData 单证券逻辑调用、fnSubscribeData 批量逻辑计划及宿主回调路由元数据；不登录、不调用 SDK、不连接、不投递消息。",
         false, "/api/v1/level2/build", command_level2_build},
        {"level2 decode", "L2 捕获离线解码", "授权行情研究",
         "解析合法会话捕获的逐笔、千档、价位队列、SDK 回调 ABI 封套、tpbus 111/112 推送及 tpbus-115 批容器、已授权 sdk-json-4653 分时和未知 PB wire；115 只保留子体长度、SHA-256 与有界摘要，不回显原始 body/document，不读取文件、不调用 SDK、不执行回调、不投递消息、不联网。",
         false, "/api/v1/level2/decode", command_level2_decode},
        {"level2 project", "L2 宿主行情状态离线投影", "授权行情研究",
         "将已授权的 1807/18071 回调快照投影到上一行情状态，把 1801/1802 回调投影为宿主 20B 记录、1803 投影为深度 13B 记录、18031 投影为队列 6B 记录、1804 投影为 105 个宿主槽，或离线投影 4654/4651 双快照与 4655 companion/raw 的替换、保留或条件候选；不调用 SDK、不投递宿主消息、不联网、不绕过授权。",
         false, "/api/v1/level2/project", command_level2_project},
        {"level2 session", "L2 会话只读预检", "授权行情研究",
         "核对本机 TdxW/tpbus 进程、模块和版本；不附加、不订阅、不读取票据。",
         false, "/api/v1/level2/status", command_level2_session},
        {"level2 preflight", "L2 SDK/TPBus 离线预检", "授权行情研究",
         "按显式原始配置只读检查 SDK DLL/PE 导出，或验证 TPBus 4650 捕获的精确 raw shape 与未解析宿主门禁；不读取配置或凭证、不运行 handler、不加载 SDK、不联网。",
         false, "CLI only", command_level2_preflight},
        {"market kline", "多周期与复权 K 线", "K 线",
         "通过 7709/0x052D、扩展市场 7727/0x23FF 或本地 DAY/LC1 读取多周期 K 线，并保留持仓量。",
         true, "/api/v1/kline", command_market_kline},
        {"market instruments", "扩展市场合约目录", "公开行情",
         "通过 7727 的 0x23F0/0x23F5 获取期货、港股等扩展市场的合约代码和名称。",
         true, "/api/v1/market/instruments", command_market_instruments},
        {"market options", "期权合约与标的映射", "公开行情",
         "从 7727 扩展目录识别商品与股指期权，规范化认购/认沽、行权价、标的、行权风格和定价族。",
         true, "/api/v1/market/options", command_market_options},
        {"market option-expiry", "期权精确到期日", "公开行情",
         "读取 TDX 产品规则与节假日资源，复现商品/股指期权到期日计算。",
         true, "/api/v1/market/option-expiry", command_market_option_expiry},
        {"market option-chain", "期权链与波动率曲面", "公开行情",
         "批量复用 7727 会话获取整条期权链，计算 IV、Greeks、持仓量、Put/Call 比率和最大痛点。",
         true, "/api/v1/market/option-chain", command_market_option_chain},
        {"market option-volatility", "期权历史与隐含波动率", "公开行情",
         "按 TQQCalc.dll 与 TdxW IVOLAT 约定复现历史/隐含波动率、模型价格与 Greeks，并自动解析到期日。",
         true, "/api/v1/market/option-volatility", command_market_option_volatility},
        {"market expansion-quote", "扩展市场实时行情", "公开行情",
         "通过 7727 的 0x23FA 获取期货、港股等扩展品种的现价、五档、成交量和持仓量。",
         true, "/api/v1/market/snapshot", command_market_expansion_quote},
        {"market expansion-timeline", "扩展市场分时", "公开行情",
         "通过 7727 的 0x240B/0x240C 获取期货、港股等扩展品种的当日或历史分时、均价和持仓量。",
         true, "/api/v1/market/expansion-timeline", command_market_expansion_timeline},
        {"market expansion-trades", "扩展市场逐笔成交", "公开行情",
         "通过 7727 的 0x23FC/0x2406 获取当日或历史逐笔，并解释买卖方向、增仓和开平性质。",
         true, "/api/v1/market/expansion-trades", command_market_expansion_trades},
        {"market watch", "L1 持续行情与重连", "公开行情",
         "持久复用 7709 会话读取 0x0547 快照与五档，按变化输出 JSONL；服务端以共享 Hub 和 SSE 向多个浏览器分发。",
         true, "/api/v1/market/stream", command_market_watch},
        {"market block-trades", "大宗交易分析", "公开行情",
         "聚合大宗成交、意向申报、营业部排行和月度行业成交，并展开动态明细。",
         true, "/api/v1/market/block-trades", command_market_block_trades},
        {"market funds", "分时段主力资金", "公开行情",
         "通过 PBRPC 200340→200341 获取市场、研究行业与个股的分时段主力资金。",
         true, "/api/v1/market/funds", command_market_funds}
    });
}

}  // namespace tdx::registry_detail
