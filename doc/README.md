# 通达信行情逆向 · 文档入口

> 目标：逆向通达信 PC 客户端，获得分时、K 线、最新价、盘口和 Level2 逐笔成交数据；最终运行时不依赖客户端进程、注入或本地 DLL。
>
> 范围：个人研究和自用，控制连接与请求数量，不外传或转售数据。

## 当前快照

2026-08-02：Phase 1 侦察继续，Phase 2 已完成网络传输层和一段应用
消息层静态分析。真实安装、7709 连接、两套 Asio、`tpbus -> TaApi`、
`FastHQ.Subscribe`、数据插件、认证加密和 `TJyaid` 设备身份辅助层已有
可复核证据；`TPool` 已确认为回调驱动的本地股票池/公式筛选引擎，
`TCalc` 的四类公式索引也已恢复并生成系统技术指标 CSV；另已从公开行情
缓存恢复行业、研究行业、概念、风格和指数板块及其证券成员关系，并闭合
`reqformat=11` 的 7709 静态资源协议和在线行业/主题—证券关联；主题投资
页面的 24 个战略大类、567 个内部主题 ID 和逐股入选逻辑也已工具化。
继续扫描列表 CFG 后又发现 521 个 XML 清单外的 JSN 候选，并已选择性
验证 171 个高价值主表；机构、龙虎榜、公司行动、大宗交易、股权关联、
融资融券、沪深港通、期货、发行统计、涨跌停、业绩预告、主动基金增持和
港股/行业评级均已有更新入口。除“股票→
十大股东→股东跨股票持仓”外，两融日期→分类→长期趋势、陆股通季度持仓、
期货/商品→股票、IPO 年份→行业→股票、经济指标→股票和事件→股票等链
也已闭合；竞价七信号已有独立聚合更新工具。Level2 方面已进一步恢复
`1364/1374` 逐笔成交/委托和 SDK `4655/4671/4680` 逐笔、队列、十档
结构，并实现离线请求生成与解析工具；TCalc type-31 的 184 字节订单流日序列及
13 个公式入口也已闭合为调用方授权上下文，当前仍等待合法授权会话动态标定。

与同花顺项目的关键差异：
- **无壳**：TDX 主进程是 VC++ 2010 原生编译，无 UPX/VMProtect 保护，静态分析门槛显著低于同花顺。
- **MFC 框架**：用 MFC 10.0 构建，而非 Qt，主窗口类和消息路由遵循经典 MFC 模式。
- **CEF Hybrid UI**：实际版本为 CEF 81 / Chromium 81，不是早期字符串
  推断的 CEF 49。
- **插件体系**：当前确认 `TCPlugins/`、`SEPlugins/`、`GNPlugins/`、
  `QHPlugins/`、`ZDPlugins/`；未发现 `PYPlugins/`。
- **treeid 路由系统**：内部使用 `http://www.treeid/...` 伪 URL 做组件间导航和通信。
- **tdx.com.cn 系列域名**：服务端通信走 `tdx.com.cn` 域名族，与 THS 的 `10jqka.com.cn` 体系独立。

## 建议阅读顺序

| 顺序 | 文档 | 用途 |
|---:|---|---|
| 1 | [当前状态与证据边界](00-overview/04-current-state.md) | 权威能力矩阵、已确认结论和未闭合边界 |
| 2 | [攻坚任务列表](06-next-steps/01-tdx-attack-plan.md) | 当前优先队列、任务分解与验收门 |
| 3 | [计划索引](06-next-steps/README.md) | 当前主线、冻结支线和历史计划状态 |
| 4 | [目标与范围](00-overview/01-goal-and-scope.md) | 逆向目标、技术路线与红线 |
| 5 | [整体架构](00-overview/02-architecture.md) | 初步架构推断、进程构成与技术分层 |
| 6 | [工具链与环境](00-overview/03-toolchain-environment.md) | 已确认可用的工具清单与命令速查 |
| 7 | [安装布局](01-recon/01-install-layout.md) | 通达信安装目录结构与核心文件 |
| 8 | [壳与保护机制](01-recon/02-packers-and-protection.md) | DIE 分析结果与自保护评估 |
| 9 | [模块地图](01-recon/03-module-map.md) | 关键 DLL 按功能分组与角色说明 |
| 10 | [方法论](04-methods/) | Frida 挂载、内存扫描、IDA headless 使用指南 |
| 11 | [归档索引](99-log/README.md) | 按日期和主题查找原始阶段记录 |

## 目录导航

| 目录 | 内容 |
|---|---|
| [00-overview](00-overview/) | 目标、架构、工具链和权威当前状态 |
| [01-recon](01-recon/) | 安装布局、保护机制和模块地图 |
| [02-engine](02-engine/) | 网络层、应用任务、数据插件与认证加密分析 |
| [03-memory-layout](03-memory-layout/) | 内存数据结构与布局（待填充） |
| [04-methods](04-methods/) | Frida、内存扫描和 IDA headless 方法 |
| [05-dead-ends](05-dead-ends/) | 已排除路线和失败经验，避免重复试错 |
| [06-next-steps](06-next-steps/README.md) | 当前主线、冻结分支和计划索引 |
| [90-scripts](90-scripts/) | 可运行脚本、IDA headless 报告和测试 |
| [99-log](99-log/README.md) | 按日期/主题组织的阶段归档 |

## 按任务进入

| 任务 | 入口 |
|---|---|
| 用单个 Python 文件直接调用通达信上游 | [tdx_api.py](../tdx_api.py)：`python tdx_api.py --help-all`，含行情、云查询、JSN和文件下载 |
| 查看最终请求通达信的地址、命令号、用途、输入和输出 | [通达信上游 API 调用速查](API.md) |
| 了解二进制基本信息 | [模块地图](01-recon/03-module-map.md) |
| 确认工具链可用 | [工具链与环境](00-overview/03-toolchain-environment.md) |
| 开始静态分析 | IDA 打开 `ida/TdxW.exe.i64`，参考 [攻坚任务 Phase 2](06-next-steps/01-tdx-attack-plan.md#phase-2-静态分析) |
| 查看网络层结果 | [UserComm 网络传输层](02-engine/01-network-layer.md) |
| 查看行情订阅入口 | [应用消息与数据插件层](02-engine/02-application-data-layer.md) |
| 查看认证算法 | [认证与加密辅助层](02-engine/03-auth-crypto-layer.md) |
| 获取系统/用户公式并查看连续标记、条件色线、带状填充及 T+0/ST/退市/期货期权状态 | 用户公式运行 `tdx-tool formulas user-library --root C:\new_tdx --scope combined --output formulas.json`；见 [TCalc 公式与指标引擎](02-engine/04-formula-engine.md)和[用户公式只读库](99-log/2026-08-12-native-user-formula-library.md)，展示证据见[公式展示语义](99-log/2026-08-09-native-formula-render-semantics.md)、[series 原生样式](99-log/2026-08-09-native-series-styles.md)与[证券状态宿主字段](99-log/2026-08-09-native-formula-security-status.md) |
| 获取行业/概念板块及成分证券 | [板块目录与证券成员关系](02-engine/05-block-membership.md) |
| 查看、评价或批量扫描本地形态模板 | 运行 `tdx-tool market shape-match --root C:\new_tdx`；单票用 `--view score --template-index N --market sz --code 000001`，板块扫描用 `--view scan --template-index N --block <板块ID>`，证据见[TDXDeep 形态匹配](99-log/2026-08-12-native-tdxdeep-shape-match.md) |
| 查看本地投资组合、流水、持仓成本与估值 | 运行 `tdx-tool market investment --root C:\new_tdx`；流水用 `--view transactions --portfolio <名称>`，持仓用 `--view holdings`，公开 L1/离线快照估值用 `--view valuation`，证据见[本地投资组合与费率](99-log/2026-08-12-native-investment-portfolio.md) |
| 生成带行情、广度和领涨/拖累的板块雷达 | 运行 `python doc/90-scripts/update_tdx_market.py --download`，协议与验证见[实时功能验证与优先级](02-engine/07-useful-live-features.md) |
| 查看可继续产品化的直连行情功能 | [实时功能验证与优先级](02-engine/07-useful-live-features.md) |
| 探索通达信自身的因子、异动、F10、资金和策略功能 | [客户端功能面与高价值数据入口](02-engine/08-client-feature-surface.md) |
| 更新在线行业、主题及其成分证券 | [JSN 静态资源协议](02-engine/09-jsn-resource-protocol.md) |
| 更新/查询战略主题及逐股入选逻辑 | 运行 `tdx-tool market strategic-themes --view catalog --limit 600 --refresh`，选择主题时用 `--view theme --theme-id <ID>`；证据见[纯 C++ 战略主题迁移](99-log/2026-08-07-native-strategic-themes.md) |
| 查询统一主题快照、逐股说明和指数历史 | 运行 `tdx-tool market theme-library --view catalog --source general`；详情使用 `--view theme --theme-id <ID>`，证据见[统一主题与债券资料库](99-log/2026-08-08-native-theme-library-kline-context-bond-reference.md) |
| 查询全市场债券、评级、利率类型和付息序列 | 运行 `tdx-tool market bond-reference --group category --bucket all --jsn-root output/tdx-jsn`；追加 `--include-projections` 核对沪深来源，证据见[分类投影对账](99-log/2026-08-09-native-bond-projection-reconciliation.md) |
| 查看行业/区域机会及爆炒复盘 | 运行 `tdx-tool market thematic-opportunities --view catalog --refresh`；分组详情使用 `--view group --group-id <ID>`，也可选择 `hype-completed` 或 `hype-active`；证据见[私募、解禁、公式显式上下文与主题机会](99-log/2026-08-07-native-private-fund-unlock-formula-thematic.md) |
| 盘点 XML 清单外的隐藏 JSN 功能 | 运行 `python doc/90-scripts/download_tdx_jsn.py --list --include-cfg`，样本见[CFG 隐藏主表](99-log/2026-07-31-cfg-jsn-hidden-resources.md) |
| 更新龙虎榜事件及营业部明细 | 运行 `python doc/90-scripts/update_tdx_lhb.py --root C:\new_tdx --download-masters`，证据见[机构与龙虎榜隐藏资源](99-log/2026-08-01-institution-lhb-hidden-resources.md) |
| 更新机构明细并反查同股东持股 | 运行 `python doc/90-scripts/update_tdx_institution.py --root C:\new_tdx --download-details --market 0 --code 000063`，证据见[机构持仓动态明细链](99-log/2026-08-01-institution-holder-detail-chain.md) |
| 更新公司行动、大宗交易与股权关联分组 | 使用 `download_tdx_jsn.py` 的 `corporate-actions`、`block-trading`、`equity-groups` 资源族，证据见[公司行动与交易统计主从链](99-log/2026-08-01-corporate-actions-trading-resources.md) |
| 更新融资融券、沪深港通、活跃龙虎榜和财经日历 | 使用 `margin-financing`、`stock-connect`、兼容旧名 `industry-lhb` 的活跃龙虎资源族、`market-calendar`，证据见[两融与沪深港通主从链](99-log/2026-08-01-margin-stock-connect-chains.md) |
| 更新期货、IPO/债券发行人、涨跌停与商品主题 | 使用 `futures-statistics`、`ipo-bond-issuance`、`price-limit-analysis`、`commodity-themes`、`premium-stocks` 资源族，证据见[期货与市场事件主从链](99-log/2026-08-01-futures-issuance-market-events.md) |
| 更新竞价信号、市场异动、ETF 资金和事件研究 | 运行 `python doc/90-scripts/update_tdx_auction.py --download`，并使用 `market-anomalies`、`etf-fund-flow`、`event-research` 等资源族；证据见[竞价与事件研究链](99-log/2026-08-01-auction-anomaly-event-chains.md) |
| Frida 挂载测试 | [Frida 设置与陷阱](04-methods/01-frida-setup-and-gotchas.md) |
| 捕获网络流量 | [攻坚任务 P3](06-next-steps/01-tdx-attack-plan.md#p3-网络拓扑与连接建立) |

## 关键边界

- TDX 主进程 `TdxW.exe` 无壳（VS2010 原生），但插件 DLL 可能存在独立保护（待确认）。
- CEF 内嵌页面使用 `http://www.treeid/...` 伪协议路由，不是标准 HTTP 请求。
- 插件 DLL 可能有独立加壳或混淆，不能单独用主进程的无壳结论外推；
  当前安装没有 `PYPlugins/`。
- `login-frame.bin`、pcap、wire、`session.json` 和认证材料均为私密短期材料，只能保存在仓库外；公开文档仅记录必要的哈希、长度、计数和脱敏分类。
