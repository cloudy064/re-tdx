#include "server_catalog_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/registry.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tdx::server_detail {

namespace {

bool is_http_path(const std::string& path) {
    return !path.empty() && path.front() == '/';
}

std::vector<std::string> api_paths(const std::string& endpoints) {
    std::vector<std::string> paths;
    for (auto endpoint : split(endpoints, ',')) {
        endpoint = trim(std::move(endpoint));
        if (is_http_path(endpoint)) paths.push_back(std::move(endpoint));
    }
    return paths;
}

bool is_post_only_path(const std::string& path) {
    return path == "/api/v1/formulas/context-import" ||
           path == "/api/v1/formulas/cloud-calc/batch" ||
           path == "/api/v1/formulas/strategy/scan" ||
           path == "/api/v1/formulas/strategy/backtest" ||
           path == "/api/v1/level2/build" ||
           path == "/api/v1/level2/decode" ||
           path == "/api/v1/level2/project" ||
           path == "/api/v1/market/disclosures/archive";
}

bool is_dual_method_path(const std::string& path) {
    return path == "/api/v1/formulas/evaluate" ||
           path == "/api/v1/formulas/cloud-calc" ||
           path == "/api/v1/formulas/scan" ||
           path == "/api/v1/formulas/backtest" ||
           path == "/api/v1/pools/evaluate";
}

std::vector<std::string> api_methods(const std::string& path) {
    if (is_post_only_path(path)) return {"post"};
    if (is_dual_method_path(path)) return {"get", "post"};
    return {"get"};
}

Json api_operations(const std::string& endpoints) {
    Json operations = Json::array();
    for (const auto& path : api_paths(endpoints)) {
        for (const auto& method : api_methods(path)) {
            Json operation = Json::object();
            operation["method"] = method;
            operation["path"] = path;
            operations.push_back(std::move(operation));
        }
    }
    return operations;
}

}  // namespace

Json feature_document() {
    Json document = Json::object();
    document["schema"] = "tdx-tool-features-v1";
    document["native_cpp"] = true;
    document["python_runtime"] = false;
    Json features = Json::array();
    for (const auto& command : command_registry()) {
        Json item = Json::object();
        item["command"] = command.name;
        item["title"] = command.title;
        item["category"] = command.category;
        item["description"] = command.description;
        item["uses_network"] = command.uses_network;
        item["api_endpoint"] = command.api_endpoint;
        auto operations = api_operations(command.api_endpoint);
        item["surface"] = operations.size() == 0 ? "cli-only" : "http";
        item["api_operations"] = std::move(operations);
        item["implemented"] = true;
        features.push_back(std::move(item));
    }
    document["count"] = static_cast<std::uint64_t>(features.size());
    document["features"] = std::move(features);
    return document;
}

Json openapi_document() {
    Json result = Json::object();
    result["openapi"] = "3.0.3";
    Json info = Json::object();
    info["title"] = "tdx-tool read-only API";
    info["version"] = "1.0.0";
    info["description"] = "Pure C++ localhost API; no arbitrary command execution.";
    result["info"] = std::move(info);
    Json paths = Json::object();
    const auto add_operation = [&](const std::string& path,
                                   const std::string& method,
                                   const std::string& summary) {
        if (!is_http_path(path)) return;
        Json operation = Json::object();
        operation["summary"] = summary;
        paths[path][method] = std::move(operation);
    };
    // The command registry is the authoritative feature-to-endpoint mapping.
    // Seed OpenAPI from it so a newly exposed command cannot silently disappear
    // from the web capability browser.  The explicit catalog below supplies the
    // richer endpoint-specific summaries and covers non-command resources.
    for (const auto& command : command_registry()) {
        for (const auto& endpoint : api_paths(command.api_endpoint)) {
            for (const auto& method : api_methods(endpoint))
                add_operation(endpoint, method, command.title);
        }
    }
    for (const auto& [path, summary] : std::map<std::string, std::string>{
             {"/api/v1/health", "服务状态和本地数据计数"},
             {"/api/v1/features", "已实现的原生功能目录"},
             {"/api/v1/blocks", "按名称、代码或 ID 查询板块和成分股"},
             {"/api/v1/securities", "按代码或名称搜索本地证券"},
             {"/api/v1/market/securities", "0x044D/0x044E 服务端完整证券目录"},
             {"/api/v1/market/instruments", "7727 扩展市场期货/港股合约目录"},
             {"/api/v1/market/options", "7727 商品/股指期权目录、标的与定价模型映射"},
             {"/api/v1/market/option-expiry", "按 TDX 产品规则与节假日解析期权到期日"},
             {"/api/v1/market/option-chain", "批量计算期权链、IV、Greeks 与持仓结构"},
             {"/api/v1/market/option-volatility", "复现 TDX 历史波动率与隐含波动率"},
             {"/api/v1/market/expansion-timeline", "7727 扩展市场当日/历史分时"},
             {"/api/v1/market/expansion-trades", "7727 扩展市场当日/历史逐笔与开平性质"},
             {"/api/v1/securities/blocks", "按市场和代码反查所属板块"},
             {"/api/v1/formulas", "查询 TCalc 内置及显式启用的 PriGS 用户公式；origin=all|system|user 过滤来源"},
             {"/api/v1/formulas/icons", "查看随程序发布的 DRAWICON 图标资源清单"},
             {"/api/v1/formulas/drawicon-strip.png", "获取随程序发布的透明 DRAWICON 精灵图"},
             {"/api/v1/formulas/drawicon-strip.bmp", "获取从 RT_BITMAP/2060 恢复的原始 BMP"},
             {"/api/v1/formulas/signal-image", "安全读取 DRAWBMP/DRAWGBK 使用的 T0002/signals 本地图片"},
             {"/api/v1/formulas/coverage", "查看公式解释器覆盖、依赖和未来函数"},
             {"/api/v1/formulas/context-template", "生成券商私有信号、授权 L2 序列或调用方宿主 raw 标量模板；纯标量模板跳过 K 线获取，不获取、推导或伪造值"},
             {"/api/v1/formulas/context-import", "把调用方自有的内联捕获按模板精确时间戳物化为公式显式上下文；严格拒绝路径、未知 binding 与未确认所有权，不调用 SDK、订阅、账户、委托或网络"},
             {"/api/v1/formulas/audit", "在 A 股或扩展市场真实 K 线上审计全部可运行公式"},
             {"/api/v1/formulas/cloud-calc", "审计或 POST 复算单行 TBigData calc/calcref 表格计算列"},
             {"/api/v1/formulas/cloud-calc/template", "从选定 TBigData CFG 依赖图生成最小可编辑输入模板"},
             {"/api/v1/formulas/evaluate", "GET 执行内置公式；POST 可执行用户源码，或用 formula + context 执行内置公式并返回稀疏绘图 IR；未来函数在 allow_future=true 时可用有界 successive-prefix future_replay 显示历史重绘，仍禁止扫描、回测、账户和委托；adjust=none|qfq|hfq 与 TQFLAG 使用同一复权上下文；完整 FINANCE/FINVALUE/DYNAINFO 标量上下文可显式关闭自动解析"},
             {"/api/v1/formulas/backtest", "GET 回测内置、POST 回测自定义专家系统 ENTERLONG/EXITLONG 信号；adjust/anchor_date 选择复权；point_in_time_finance=1 启用严格财务时点"},
             {"/api/v1/formulas/scan", "GET 扫描内置、POST 扫描自定义条件公式；adjust/anchor_date 选择逐证券复权；workers=1..16 控制有界首轮抓取；point_in_time_finance=1 启用严格财务时点"},
             {"/api/v1/formulas/strategy/scan", "POST 多公式同证券同日期时间 K 线组合扫描；adjust/anchor_date 选择逐证券复权；workers=1..16 控制有界首轮抓取"},
             {"/api/v1/formulas/strategy/backtest", "POST 多公式下一共同 K 线开盘等权组合回测与逐股归因；adjust/anchor_date 选择逐证券复权；workers=1..16 控制有界首轮抓取"},
             {"/api/v1/formulas/calculate", "以纯 C++ 计算常用指标"},
             {"/api/v1/pools", "只读检查本地 TPool 股票池；HTTP 仅返回 TDX 根相对目录和 source，不回显服务器绝对根路径"},
             {"/api/v1/pools/history", "按池、节点、类型与日期只读查询 TPool 每日 .dat/.log 历史"},
             {"/api/v1/pools/evaluate", "只读计算本地 TPool 规则与流程候选；GET source 只接受并返回 TDX 根相对 XML 路径"},
             {"/api/v1/cloud/routes", "诊断 reqformat=1 旧云服务路由与宿主兼容性"},
             {"/api/v1/cloud/variants", "审计 TQLEX/PBRPC 模板语义变体与类型化 C++ 命令覆盖"},
             {"/api/v1/level2/status", "只读核对本机合法 L2 会话运行前提"},
             {"/api/v1/level2/build", "离线构造已恢复的 Level2 请求体、fnReqData 单证券调用计划、批量订阅计划与宿主回调路由计划；不连接、不调用 SDK、不投递消息"},
             {"/api/v1/level2/decode", "离线归一化内联 Level2 捕获、tpbus-115 批容器、SDK 回调 ABI 封套或已授权 SDK JSON（含 sdk-json-4653 分时）；115 子体只保留长度、SHA-256 与有界摘要，不回显原始 body/document，不读取文件、不调用 SDK、不执行回调、不投递消息、不联网"},
             {"/api/v1/level2/project", "离线投影 1807/18071 宿主行情状态、1801/1802 宿主 20B 记录、1803 深度 13B 记录、18031 队列 6B 记录及 1804 宿主槽；不调用 SDK、不投递消息、不联网、不绕过授权"},
             {"/api/v1/market/snapshot", "获取 A 股或扩展市场最新行情快照"},
             {"/api/v1/market/speed", "获取 A 股服务端涨速、最新行情和买卖五档"},
             {"/api/v1/market/depth", "获取个股买卖五档"},
             {"/api/v1/market/stream", "以 SSE 推送共享持久 L1 会话的快照与五档增量"},
             {"/api/v1/market/stream/status", "查看 L1 行情 Hub 订阅、重连和上游会话状态"},
             {"/api/v1/market/finance", "获取个股财务基础信息"},
             {"/api/v1/market/capital", "获取个股股本变迁与除权事件"},
             {"/api/v1/market/limits", "获取或查询特殊品种涨跌停限制表"},
             {"/api/v1/market/block-trades", "获取大宗成交、意向申报、营业部和行业分析"},
             {"/api/v1/market/ownership", "获取股东/董监高变动、六类增减持榜、五市场股东人数、承诺不减持、股权质押、统计和机构"},
             {"/api/v1/market/forecasts", "获取行业/A股/港股业绩预告统计与详情"},
             {"/api/v1/market/disclosures", "只读获取财报预约、变更、实际披露日、快报与近一月摘要；backfill_announcements=1 回补指定证券近一年正式报告"},
             {"/api/v1/market/disclosures/archive", "经 X-TDX-Action 确认后，将完整披露观察增量写入 TDX 根内固定归档路径"},
              {"/api/v1/market/foreign-alerts", "获取当前外资持股预警及选定股票状态历史"},
              {"/api/v1/market/panorama", "获取通达信十类个性数据全景或单票聚合"},
             {"/api/v1/market/institution-analysis", "获取二十五类机构持仓、投资参股、举牌、养老金、浮筹和基金全景"},
             {"/api/v1/market/active-funds", "获取基金季报股票汇总及股票持有基金详情"},
             {"/api/v1/market/etf-flows", "获取股票与行业 ETF 持股、申赎和交易资金"},
             {"/api/v1/market/institution-lhb", "获取四周期机构席位排行及股票异动详情"},
             {"/api/v1/market/active-lhb", "获取三周期活跃龙虎榜、累计买卖额及单票历史异动"},
             {"/api/v1/market/state-owned-reform", "获取国企改革四类分组、控制人逻辑与重组预期"},
             {"/api/v1/market/ratings", "获取港美股与一级研究行业评级及历史"},
             {"/api/v1/market/repurchases", "获取股份回购方案、月度年度统计和港股逐笔历史"},
             {"/api/v1/market/tender-offers", "获取要约收购进度、价格、拟定与实际规模、期限、过户、退市标记和单票历史"},
             {"/api/v1/market/funds", "获取市场、行业或个股分时段主力资金"},
             {"/api/v1/market/futures-issuance", "获取期货统计、IPO/债券发行人、定向增发、配股生命周期与优先股条款"},
             {"/api/v1/market/holder", "获取股东跨股票持仓和选定股票报告期历史"},
             {"/api/v1/market/lhb", "获取龙虎榜事件、分析视图和营业部席位明细"},
             {"/api/v1/market/consensus", "获取九类一致预期榜单及单票机构研报明细"},
             {"/api/v1/market/research", "获取机构调研、互动、监管、行业及知名机构活动"},
             {"/api/v1/market/roadshows", "获取无 ReqId TQLEX KV 路由中的全市场及单票路演"},
             {"/api/v1/market/industry-profile", "获取研究行业机构持仓历史、股东结构和逐股画像"},
             {"/api/v1/market/unlocks", "获取近期限售解禁事件、批次和具体解禁股东"},
             {"/api/v1/market/valuation", "获取市场指数 PE/PB、历史百分位和关联基金"},
             {"/api/v1/market/relative-valuation", "获取指数相对基准的 PE/PB/PS 估值比和历史分位"},
             {"/api/v1/market/equity-valuation", "获取个股/行业 PE 与 PB-ROE 估值模型"},
             {"/api/v1/market/flow-followup", "获取两融/北向历史及四组资金分档模型"},
             {"/api/v1/market/abnormal-moves", "获取十九类交易异常与可能停牌核查证券"},
             {"/api/v1/market/abnormal-details", "获取异常证券摘要与单证券上榜原因"},
             {"/api/v1/market/anomaly-risk", "获取异常波动统计与异动停牌风险"},
             {"/api/v1/market/profit-gaps", "获取业绩披露后的利润断层及安全分"},
             {"/api/v1/market/index-volatility", "获取指数已实现波动率、均值和历史分位"},
             {"/api/v1/market/total-return-gap", "比较价格指数与全收益指数的区间收益"},
             {"/api/v1/market/fund-analytics", "获取基金风险、能力、公开报告期持仓、持仓稳定性及仓位估算"},
             {"/api/v1/market/auction", "获取个股开盘与收盘集合竞价序列"},
             {"/api/v1/market/ranking", "获取全市场实时服务端排序"},
             {"/api/v1/market/limit-quality", "获取涨停封单与竞价质量聚合"},
             {"/api/v1/market/limit-review", "获取通达信非实时涨跌停复盘、年度行为和历史成员"},
             {"/api/v1/market/session-turnover", "获取 A 股或 ETF 开盘、盘后及总成交排行"},
             {"/api/v1/market/block-rotation", "获取行业、概念、地区和风格板块轮动统计"},
             {"/api/v1/market/limit-ladder", "获取研究行业与概念板块连板天梯统计"},
             {"/api/v1/market/threshold-stocks", "获取百元股与千亿市值历史、日期名单及入围跌出"},
             {"/api/v1/market/capital-strength", "获取五周期 DDX 资金强势榜、跨周期共振与单票命中"},
             {"/api/v1/market/strong-stocks", "获取强势股连板生命周期及区间逐日涨停原因与市场温度"},
             {"/api/v1/market/commodity-links", "获取商品行情、涨价题材、驱动事件及关联股票/行业/ETF"},
             {"/api/v1/market/announcement-signals", "获取公告精选、风险提示公告、PDF 链接及单票前后表现历史"},
             {"/api/v1/market/reverse-repo", "获取国债逆回购实时年化利率、净收益与资金可用/可取日"},
             {"/api/v1/market/exchange-supervision", "获取当前及历史交易所监管观察期、区间收益与公告原文"},
             {"/api/v1/market/convertible-bonds", "获取可转债/可交换债定价、发行申购与客户端投影、条款、历史及待发方案对账"},
             {"/api/v1/market/bond-reference", "查询全市场债券或按信用评级、利率类型、类别筛选条款与付息序列"},
             {"/api/v1/market/economic-indicators", "获取经济指标当前值、历史曲线及关联股票"},
             {"/api/v1/market/strategic-themes", "获取战略主题大类、主题、成分股及入选逻辑"},
             {"/api/v1/market/theme-library", "获取统一主题库、逐股纳入原因及主题指数历史"},
             {"/api/v1/market/thematic-opportunities", "获取行业区域机会组、遗留客户端主题、逐股逻辑、页面语义冲突及爆炒复盘"},
             {"/api/v1/market/hot-history", "读取本地多年K线历史热点区间、主题、区间/峰值收益和完整分析"},
             {"/api/v1/market/historical-securities", "读取本地当前及历史证券兼容名称，与当前证券目录对账；资源来源仅返回TDX根相对路径"},
             {"/api/v1/market/index-events", "读取本地上证、恒生和美股指数图重大事件、交易日对齐及正文recid；资源来源仅返回TDX根相对路径"},
             {"/api/v1/market/calendar", "获取财经、公司重大事项、配股节点、境内与美股IPO及北交所申购详情"},
             {"/api/v1/market/hk-events", "获取港股分红、权益披露、每日沽空统计与上市申请"},
             {"/api/v1/market/hk-actions", "解密本地港股历史公司行动与客户端原生累计复权因子；资源来源仅返回TDX根相对路径"},
             {"/api/v1/market/hk-finance", "解密本地港股财务缓存，类型化核心财务、股息、估值与币种折算标记并保留TdxW原生绑定；资源来源仅返回TDX根相对路径"},
             {"/api/v1/market/hk-short-history", "获取 7727 港股历史沽空股数并与每日事件表对账"},
             {"/api/v1/market/special-attention", "获取股权分散、潜在ST、摘星摘帽、立案调查和商誉风险"},
             {"/api/v1/market/fund-statistics", "获取基金发行、分红、收益、市场规模、ETF申赎和上市统计"},
             {"/api/v1/market/fund-reference", "读取本地基金份额、交易参考值、已发布净值、ETF/LOF跟踪标的及可空原生生命周期；资源来源仅返回TDX根相对路径"},
             {"/api/v1/market/fund-calendar", "获取基金开放、分红等事件日历"},
             {"/api/v1/market/specialized-metrics", "获取银行、券商和保险公司专项经营指标"},
             {"/api/v1/market/company-changes", "获取证券/公司更名、指数、股权、实控人与行业变更"},
             {"/api/v1/market/financial-screen", "获取五板财务快照与小盘成长三年复合增速股票池"},
             {"/api/v1/market/financial-insights", "获取十五类客户端特色财务筛选线索"},
             {"/api/v1/market/gdr", "获取GDR与A股映射、折算价和溢价率"},
             {"/api/v1/market/equity-performance", "获取全A股5/20/60日、本月和年初至今表现快照"},
             {"/api/v1/market/corporate-orders", "获取上市公司招投标、中标与重大合同"},
             {"/api/v1/market/event-impact", "获取重大事件前后三地指数冲击"},
             {"/api/v1/market/global-performance", "获取主要指数与海外中资证券多周期表现"},
             {"/api/v1/market/shareholder-signals", "获取牛散目录与逐人持仓、机构增持、股东收缩、小市值机构及调研信号"},
             {"/api/v1/market/recent-watch", "获取绩价背离、涉外经营与ST业绩预盈关注表"},
             {"/api/v1/market/patent-statistics", "获取上市公司本期专利申请、授权及累计授权数量"},
             {"/api/v1/market/overview-factors", "获取通达信客户端大盘分析因素及利好、中性、利空快照"},
             {"/api/v1/market/benchmark-analysis", "获取个股/行业牛熊阶段、停复牌和次新股相对基准表现"},
              {"/api/v1/market/margin", "获取融资融券市场、转融通、排行与单票历史"},
             {"/api/v1/market/stock-connect", "获取沪深港通资金、持仓与单票历史"},
             {"/api/v1/market/intelligence", "获取关注度、价值关注、风险预警、失信被执行对象、事件及股票关系图"},
             {"/api/v1/market/technical-signals", "获取九转、RPS、趋势、模型、竞价、盘中/T+0机会及六类盘面因子等三十二类选股，或按 market+code 反查单票"},
             {"/api/v1/market/factors", "获取因子目录、完整关系矩阵、单票反查、市场广度/共现及看板—直接成员对账"},
             {"/api/v1/market/block-backtest", "获取板块及服务端选定成分股的区间历史表现"},
             {"/api/v1/market/stats", "获取封单、竞价、涨停、tipinfo，以及 valuation=1 时的四项原生估值"},
             {"/api/v1/market/professional", "查询官方单期/多季度专业财务、个股、板块和市场交易序列"},
             {"/api/v1/market/trades", "获取当日或历史 L1 成交明细"},
             {"/api/v1/minute", "获取在线或本地一分钟线"},
             {"/api/v1/kline", "按 market+code 或 block 精确查询分时及多周期 K 线"},
             {"/api/v1/security/profile", "获取单票机构历史和十大流通股东"},
             {"/api/v1/cloud/workflows", "列出内置 TQLEX/PBRPC 主从工作流"},
             {"/api/v1/cloud/workflow", "执行固定的云端主表到明细关联工作流"},
             {"/api/v1/pbrpc/configs", "列出启用的 PBRPC protobuf 配置"},
             {"/api/v1/pbrpc/query", "按已启用配置和可下调的组装字节预算执行 PBRPC RpcID 查询"},
             {"/api/v1/tqlex/configs", "列出启用的 TQLEX JSON 配置"},
             {"/api/v1/tqlex/query", "按已启用配置执行 TQLEX JSON 查询；HTTP page_size=1..5000、max_pages=1..20，全页合并最多 50000 行，单页固定执行 1 页"},
              {"/api/v1/jsn/security", "横向查询个股的全部 JSN 关联"},
              {"/api/v1/jsn/catalog", "浏览已下载 JSN 资源目录"},
              {"/api/v1/jsn/variants", "审计 XML/CFG JSN 模板、下载状态与类型化 C++ 覆盖"},
              {"/api/v1/jsn/discovery", "比较 JSN 指纹基线、字段结构与动态资源键"},
              {"/api/v1/jsn/candidates", "沿 CFG refunit 主表生成安全的动态资源候选队列"},
             {"/api/v1/jsn/resource", "探测或下载指定 JSN 资源"},
             {"/api/v1/industry/tree", "读取多级行业与板块树"},
             {"/api/v1/install", "盘点本地 EXE 和 DLL"},
         }) {
         for (const auto& method : api_methods(path))
             add_operation(path, method, summary);
    }
    Json pbrpc_budget_parameter = Json::object();
    pbrpc_budget_parameter["name"] = "max_assembled_bytes";
    pbrpc_budget_parameter["in"] = "query";
    pbrpc_budget_parameter["required"] = false;
    pbrpc_budget_parameter["description"] =
        "调用方可下调的完整分片组装字节预算；默认及硬上限均为 134217728 字节";
    Json pbrpc_budget_schema = Json::object();
    pbrpc_budget_schema["type"] = "integer";
    pbrpc_budget_schema["minimum"] = 1;
    pbrpc_budget_schema["maximum"] = 134217728;
    pbrpc_budget_schema["default"] = 134217728;
    pbrpc_budget_parameter["schema"] = std::move(pbrpc_budget_schema);
    Json pbrpc_parameters = Json::array();
    pbrpc_parameters.push_back(std::move(pbrpc_budget_parameter));
    paths["/api/v1/pbrpc/query"]["get"]["parameters"] =
        std::move(pbrpc_parameters);
    Json formula_post = Json::object();
    formula_post["summary"] =
        "在本地纯 C++ 解释器中执行 JSON body 提交的源码，或无 source 时执行内置 formula 与显式 context；未来函数需 allow_future=true，future_replay 可选 max_observations=1..64、max_events=1..10000，仅比较逐步揭示 K 线后既有历史点的变化，禁止扫描、回测、账户、委托、SDK 和订阅；adjust/anchor_date 可选择复权上下文；context.automatic_market_context=false 要求完整提供 FINANCE/FINVALUE/DYNAINFO context_bindings_required";
    paths["/api/v1/formulas/evaluate"]["post"] = std::move(formula_post);
    Json formula_context_import_post = Json::object();
    formula_context_import_post["summary"] =
        "严格校验内联 template/capture 并按精确 DATE|TIME 对齐；默认拒绝缺失点，allow_partial 仅输出诊断；不接受路径、不保留捕获封套、不调用 SDK、订阅、账户、委托或网络";
    paths["/api/v1/formulas/context-import"]["post"] =
        std::move(formula_context_import_post);
    Json cloud_calc_post = Json::object();
    cloud_calc_post["summary"] =
        "以内联行和可选公开 L1/财务上下文复算选定 TBigData CFG，不接受服务器文件路径";
    paths["/api/v1/formulas/cloud-calc"]["post"] = std::move(cloud_calc_post);
    Json cloud_calc_batch_post = Json::object();
    cloud_calc_batch_post["summary"] =
        "批量复算 1—128 个内联行，共享去重后的公开 L1/财务请求，不接受服务器文件路径";
    paths["/api/v1/formulas/cloud-calc/batch"]["post"] =
        std::move(cloud_calc_batch_post);
    Json pool_evaluate_post = Json::object();
    pool_evaluate_post["summary"] =
        "以内联 XML 只读执行 TPool 规则、过滤、跨证券排名和流程投影，不接受服务器文件路径";
    paths["/api/v1/pools/evaluate"]["post"] = std::move(pool_evaluate_post);
    // These Level2 operations are POST-only: request bodies can be moderately
    // sized and must never be reflected into a URL, cache or server path.
    Json level2_build_post = Json::object();
    level2_build_post["summary"] =
        "离线构造 direct、TdxW 1369/1371 内部 IPC、SDK redirect 请求字节、SDK 1807 待解析订阅计划、sdk-fnreqdata-plan / sdk-fnreqdata-18031-plan 单证券逻辑调用、内联 document 的 fnSubscribeData 批量逻辑计划，或 sdk-callback-route-plan 宿主消息路由元数据；拒绝路径，不打开会话、不调用 SDK、不连接、不投递消息";
    paths["/api/v1/level2/build"]["post"] = std::move(level2_build_post);
    Json level2_decode_post = Json::object();
    level2_decode_post["summary"] =
        "解码 payload_hex 内联捕获（含 tpbus-115 批容器）、sdk-callback-invocation ABI 封套或归一化 document 内联的已授权 SDK JSON（含 sdk-json-4653 分时）；115 子体只保留长度、SHA-256 与有界摘要；拒绝文件路径，处理后不保留原始输入、不回显原始 body/document，不调用 SDK、不执行 SDK 回调、不投递宿主消息、不联网";
    paths["/api/v1/level2/decode"]["post"] = std::move(level2_decode_post);
    Json level2_project_post = Json::object();
    level2_project_post["summary"] =
        "用 sdk-quote-transition 将精确 380 B 1807/18071 授权回调快照投影到 previous 宿主行情状态；用 sdk-1801-host-projection/sdk-1802-host-projection 投影 20 B 宿主记录、sdk-1803-depth-record-projection 投影 13 B 深度记录、sdk-18031-queue-record-projection 投影 6 B 队列记录，或用 sdk-1804-host-projection 投影 105 个宿主槽；sdk-4654-dual-snapshot-transition、sdk-4651-dual-snapshot-transition 与 sdk-4655-companion-raw-transition 仅对显式内联双快照做离线替换/保留/候选投影；不接受路径或上传，不回显原始输入，不调用 SDK、不投递宿主消息、不联网、不绕过授权";
    paths["/api/v1/level2/project"]["post"] =
        std::move(level2_project_post);
    Json scan_post = Json::object();
    scan_post["summary"] = "扫描 JSON body 提交的自定义条件公式源码；workers=1..16 有界并发获取行情与复权输入";
    paths["/api/v1/formulas/scan"]["post"] = std::move(scan_post);
    Json backtest_post = Json::object();
    backtest_post["summary"] = "回测 JSON body 提交的自定义专家公式源码";
    paths["/api/v1/formulas/backtest"]["post"] = std::move(backtest_post);
    Json strategy_scan_post = Json::object();
    strategy_scan_post["summary"] =
        "按 all、any 或 at-least 在同一证券同一日期时间 K 线上组合多个条件公式；workers=1..16 有界并发抓取";
    paths["/api/v1/formulas/strategy/scan"]["post"] =
        std::move(strategy_scan_post);
    Json strategy_backtest_post = Json::object();
    strategy_backtest_post["summary"] =
        "以收盘信号、下一共同 K 线开盘调仓回测固定股票池，并返回逐股收益归因；workers=1..16 有界并发抓取";
    paths["/api/v1/formulas/strategy/backtest"]["post"] =
        std::move(strategy_backtest_post);
    result["paths"] = std::move(paths);
    return result;
}


}  // namespace tdx::server_detail
