# 统一主题、K 线批量上下文、远端清单与债券资料库

> 日期：2026-08-08。范围：公开 7709 JSN/K 线、纯 C++ CLI/API、Svelte
> 数据中心、公式解释器边界与正式服务部署；不依赖 Python、账号态或 Level2。

## 统一主题库

客户端 `ZTTZ102—106` 不是一棵可直接串联的主题树，而是区域经济、国企系、
公司系、统一主题、参股持股五张独立快照。声明数量与实际行数全部一致：

| 来源 | 行数 |
| --- | ---: |
| 区域经济 | 109 |
| 国企系 | 75 |
| 公司系 | 29 |
| 统一主题 | 850 |
| 参股持股 | 39 |

五表合计 1,102 条、927 个去重主题 ID、48,145 条主题—股票成员关系，覆盖
5,541 只证券。纯 C++ `market theme-library` 提供目录、单主题和单证券视图；
`zttz/<ID>` 的逐股说明非空时覆盖该主题的静态成员，`zttz1/<ID>` 提供主题
指数历史。`source=all` 使用 `来源:ID` 作为记录键，避免把跨来源同 ID 误合并
成父子关系。3137“实验猴”实测为 4 条逐股说明和 18 个图表点。

新增固定 API `/api/v1/market/theme-library`、Svelte“统一主题库”页面及主题指数
图表。它与原有 26 类战略主题保持独立，不把两套页面的刷新截面混在一起。

## 公式解释器的 K 线批量上下文

`formulas context-template` 与对应 API 现接受 `market/code/period/pages/page-size`
（HTTP 使用 `page_size`），先获取实际 K 线，再为每根 bar 生成精确
`DATE|TIME` 空键。平安银行日线 20 根样本生成 20 个时间点和 8 条
`ZJLX/L2_AMO` 绑定，元数据为 `stamp_source=kline`、`bar_count=20`。

这只是索引模板：所有外部值仍为 `null`。工具不会根据 OHLCV 推导 L2 金额，
不会下载券商私有序列，也不会绕过授权边界。用户只能填入自己合法持有、与
TCalc 兼容的数值，再交给现有纯 C++ 解释器执行。网页公式详情可在单时间模板与
当前证券/周期 K 线批量模板之间切换。

## 远端可用性清单与审计排序

`jsn download --report` 新增原子 JSON 清单，记录探测动作、资源路径、状态、
长度和 MD5。对当时剩余 99 个通用静态候选重新执行 709 元数据探测：64 个非空、
35 个零长度，非空总量 266,718,311 字节。

`recon jsn-variants --availability-report` 会把 99 项结果附到模板记录，优先排序
确认非空项，并从 `high_value_gaps` 排除确认零长度项。远端可用性本身不改变
类型化覆盖。主题动态详情下载后还暴露一个审计矛盾：四个文件已有类型化命令，
但 CFG 的 `file="zttz"/"zttz1"` 尚未展开。补齐两个动态模板和单测后：

- 第一阶段 620 个模板中 536 个类型化、84 个仍为通用候选；
- 第一阶段 543 个已下载文件全部匹配模板、全部归属类型化命令；
- 解析错误 0，已下载 `generic-only` 0；
- 增量发现 `recognized=543`、`unrecognized=0`。

清单与最终审计分别保存在：

- `output/tdx-jsn-remote-manifest.json`；
- `output/tdx-jsn-variants.json`。

## 债券条款资料库

远端确认非空的高收益缺口中，客户端 CFG 已明确给出 `ZQ_XYPJ.sp` 的 9 个
信用评级桶和 `ZQ_LLLX.sp` 的 6 个利率类型桶。纯 C++
`market bond-reference` 首先固定这 15 张表，并只懒加载用户选择的桶：

- 信用评级：AAA、AA+、AA、AA-、A+、A、A-、A-1、BBB+及以下；
- 利率类型：固定、浮动、累进、利随本清、贴现、其他；
- 字段：债券/主体评级、债券类别、计息/到期/下一及上一付息日、剩余期限、
  当前票息、付息频率、面值、发行价、完整和剩余票息序列。

付息序列中的 `0.033` 是比例，归一为 `3.3%`；当前票息 `1.850` 已是百分比点，
保持 `1.85%`。AA+ 实测 941 条，示例可恢复 5 期完整序列和 2 期剩余序列。
AAA 与固定利率资源约 30—37 MB，因此网页默认 AA+，用户选择大桶时才读取。
该资料库不拼接不存在的宿主行情列，也不与可转债公开 L1 定价混为一体。

同轮继续固定 7 个债券类别：国债、地方债、公司债、企业债、私募债、资产支持
证券和次新可转债。前六类与评级/利率表同构，另补发行规模与担保状态；国债
实测 450 条。地方债远端约 28 MB，仍只在用户主动选择时加载。次新可转债表
实测 57 条，字段与普通债券不同，已通过别名映射恢复正股、转股期、转股价、
下修/回售/赎回触发比例和票息序列。真实样本 `26江铜EB` 能关联
`SH600362`、转股价 52.4 元、下修阈值 80% 和赎回阈值 120%；缺失回售阈值保持
`null`。它仍是类别/条款快照，不替代现有实时定价视图。

扩展后共 22 张债券资料表；覆盖提升为 543/620，549 个已下载文件全部匹配，
解析错误和已下载 `generic-only` 均为 0。新增国债契约与 AA+ 契约 2/2，
预部署 full 为 150/150。

新增固定 API `/api/v1/market/bond-reference`、Svelte“债券条款”页面和 AA+ 契约。

## 转融资与转融券历史

继续筛选远端已确认非空且不与现有能力重复的资源后，将
`list/func_rzt101_1.jsn` 类型化为 `market margin --view transfer`：

- 当前 241 个交易日，最新 `20260807`；
- 转融资偿还 1,400,000,000 元、余额 147,580,000,000 元；
- `zrz*` 金额保持原始元，`zrq1—zrq4` 数量保持原始股；
- 当前为空的融出、净增、余量和余额字段保持 `null`，不补零、不倒推；
- 固定 API 和 Svelte 融资融券页均增加独立转融通视图。

覆盖由 543/620 提升到 544/620；550 个下载文件涉及 493 个模板，全部匹配、
解析错误和已下载 `generic-only` 均为 0。剩余通用候选降到 76 个。

## 一致预期价格阶段榜

远端清单确认 `func_yzyq106/107/109_1.jsn` 非空后，由纯 C++ 下载并并入
既有 `market consensus`：

| 榜单 | 资源 | 当前行数 | 关键字段 |
| --- | --- | ---: | --- |
| 年高点至今回撤 | `func_yzyq106_1` | 23 | `zgdf/jynzgj/zxspj` |
| 连续上涨 | `func_yzyq107_1` | 48 | `lzts/zgdf` |
| 年低点至今涨幅 | `func_yzyq109_1` | 101 | `zdzf/jynzdj/zxspj` |

一致预期由六榜扩展到九榜，共 963 行、636 只去重证券。`SZ000100` 的年低点榜
样例为低点 3.95、源表收盘 4.86、客户端涨幅 23.03797%。这些价格可能来自不同
采样时点，因此接口保留客户端百分比，不拿当前 L1 价格重算并覆盖源值。

覆盖继续提升到 547/620；553 个下载文件涉及 496 个模板，匹配 553/553、解析
错误和已下载 `generic-only` 均为 0，剩余通用候选 73 个。

## 验证与部署

- 原生构建通过；CTest 全量 98/98，主题库、债券资料、公式引擎、JSN 变体和
  契约求值均有独立覆盖。
- Svelte `check` 为 0 错误、0 警告；Vite 生产构建通过。
- 第一阶段新增主题、上下文、债券及 JSN 发现正式契约 6/6；预部署 full 149/149。
- 第一次正式 full 为 148/149：阴极铜关联股票的公开 L1 批量行情一次
  `WSA 10060`，但 72 项目录、100 个历史点和 139 只关联证券均完整。单项立即
  复核 1/1，随后正式 full 149/149；失败报告保留，没有放宽断言。
- 债券类别扩展后新增契约 2/2、预部署 full 150/150；该阶段正式选定契约 3/3、
  正式 full 150/150。
- 转融通新增单元语义测试及契约；该阶段预部署及正式 full 均为 151/151。
- 一致预期三榜新增字段语义测试和固定契约；预部署、最终正式 full 均为 152/152。
- 最终正式服务为 `127.0.0.1:8765`，PID `38252`，功能目录 145 项，健康页识别
  553 份 JSN、47,658 个证券键，`native_cpp=true`、`python_runtime=false`。
- 发行 EXE SHA-256：
  `699EEA70218DF28060036F54A7941AC930BE1F1C253CE5B41D83309D49401F6E`。

主要契约证据：

- `output/api-contracts-theme-context-bond-selected.json`；
- `output/api-contracts-theme-context-bond-predeploy-full-final.json`；
- `output/api-contracts-theme-context-bond-formal-selected.json`；
- `output/api-contracts-theme-context-bond-formal-full.json`（保留瞬时失败）；
- `output/api-contracts-economic-indicator-formal-retry.json`；
- `output/api-contracts-theme-context-bond-formal-full-final.json`。
- `output/api-contracts-bond-categories-selected.json`；
- `output/api-contracts-bond-categories-predeploy-full.json`；
- `output/api-contracts-bond-categories-formal-selected.json`；
- `output/api-contracts-bond-categories-formal-full.json`；
- `output/api-contracts-margin-transfer-selected.json`；
- `output/api-contracts-margin-transfer-predeploy-full.json`；
- `output/api-contracts-margin-transfer-formal-selected.json`；
- `output/api-contracts-margin-transfer-formal-full.json`；
- `output/api-contracts-consensus-stage-selected.json`；
- `output/api-contracts-consensus-stage-predeploy-full.json`；
- `output/api-contracts-consensus-stage-formal-selected.json`；
- `output/api-contracts-consensus-stage-formal-full.json`。

## 后续（上一阶段）

剩余 73 个通用模板中，沪深交易所拆分债券表与本轮类别表高度重叠，继续迁移的
边际收益较低。下一轮应重新查看远端非空榜中字段闭合、与现有能力不重复的页面；
仍坚持先闭合客户端页面分组和单位，不凭文件名注册功能。

## 后续推进：优先股、失信风险与小盘双筛选

这一阶段继续从远端非空候选中选择与现有功能互补、字段可由客户端配置证明的
六张资源，没有把名称相似但集合口径不同的表直接拼接：

| 能力 | 资源 | 当前结果 | 关键口径 |
| --- | --- | ---: | --- |
| 优先股发行 | `func_yxg101_3` | 55 条、33 只基础 A 股 | 优先股代码与基础股票分别保留；万股/亿元换算为股/元 |
| 失信被执行 | `func_sxbzx101_1` | 9 条 | 事实清单，不生成不存在的安全分 |
| 小盘成长 | `func_xpcz101_1` | 33 只 | T/T-1/T-2/T-3 同比与源表三年 CAGR |
| 创业板投影 | `func_xpcz103_1` | 6 只 | 与 33 只母表对应板块子集精确一致 |
| 科创板投影 | `func_xpcz104_1` | 13 只 | 与 33 只母表对应板块子集精确一致 |
| 小市值机构增持 | `func_xszgp101_1` | 46 条 | 与既有 `JGXC101` 仅重叠 4 只 |

`XPCZ103/104` 是 `XPCZ101` 的精确投影，不是两个可追加的数据批次，因此对外
只返回 33 只去重母集，并在 `projection_reconciliation` 中报告两个子集的
`exact_match=true`。`XSZGP101` 的 `jgsl/jgbhl` 当前出现非整数值，不能仅凭列名
解释为机构家数；类型化接口使用中性源值名称，同时给出持股数量、持股价值和
两个可复核变化率公式。优先股当前累计发行 9,065,509,300 股、
921,551,000,000 元；11 条累积股息、45 条可调整股息。失信表只保留公告日、
涉及对象、对象类型和近一年次数，`safety_score=null` 是有意的语义边界。

四项能力已分别进入：

- `market futures-issuance --section preferred-shares`；
- `market intelligence --view risks --category discredited`；
- `market financial-screen --dataset small-cap-growth`；
- `market shareholder-signals --view small-cap-institution`。

相应固定 API、Svelte 数据中心和个股关联页均已接入。JSN 审计最终为 553/620
类型化、67 个通用候选；559 个下载文件涉及 502 个模板，匹配 559/559，解析错误
和已下载 `generic-only` 均为 0。

## 最终验证与正式部署

- 原生构建通过，CTest 全量 98/98；Svelte 检查为 0 错误、0 警告，Vite 生产
  构建通过。
- 本阶段四项选定契约预部署及正式环境均为 4/4；两次 full 分别为 155/155。
- 正式服务继续监听 `127.0.0.1:8765`，当前 PID `20096`，功能目录 145 项；
  健康页报告 JSN 559、证券键 47,658、公式 379，且
  `native_cpp=true`、`python_runtime=false`。
- 正式 EXE SHA-256 为
  `E425911B05B47A17D58EB192D67E858C71832F797D599460F40C5D67885292C7`；
  正式网页脚本为 `index-Cs9mcTyU.js`。

新增证据：

- `output/tdx-preferred-shares-native.json`；
- `output/tdx-discredited-risk-native.json`；
- `output/tdx-small-cap-growth-native.json`；
- `output/tdx-small-cap-institution-native.json`；
- `output/api-contracts-preferred-risk-smallcap-selected.json`；
- `output/api-contracts-preferred-risk-smallcap-predeploy-full.json`；
- `output/api-contracts-preferred-risk-smallcap-formal-selected.json`；
- `output/api-contracts-preferred-risk-smallcap-formal-full.json`。

## 下一步

剩余 67 个通用模板中，下一轮继续优先选择已确认非空、与现有页面不重叠且单位
和集合关系可证明的资源；高度重叠的交易所拆分债券表继续保持低优先级。
