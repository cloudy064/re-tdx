# 原生 C++ 可维护性治理汇总

首次整理：2026-08-11；最后更新：2026-08-12

## 当前规模

完成因子链路、公式渲染流水线、技术信号链路、公式渲染契约、市场情报、云端公式计算应用层、市场日历、实时行情核心、公式上下文构建器、杠杆资金链路、公式 HTTP 处理层、期货/发行数据链路、TDX 专用指标策略、公式分析流水线、市场统计资源、常用指标计算器、资金流后续表现、公式策略流水线、专业数据链路、限售解禁、评级、机构分析、云计算宿主、机构股东、板块基础、交易所基金、大宗交易、基金分析、业绩预告、国企改革、公式横向聚合上下文、市场查询适配器、公式资源提取、企业/精选数据响应契约、特殊情形、债券参考资料、商品关联、公式扫描/持续监控、披露命令、TQLEX 协议层、港股事件、行业画像、财务洞察、题材机会、对账/诊断编排、市场研究、股份回购、个股/行业估值、DDX 资金强度、战略主题层级、股东信号、涨跌停复盘、相对估值、异常风险、阈值股票、国债逆回购、分时主力资金、公告信号、精选数据、逐笔成交、对账契约集成层、对账研究目录层、公式渲染事件派发、题材库链路、因子查询分派、经济指标链路以及命令注册表的结构治理后，
`native/src` 的生产 `.cpp` 状态为：

| 指标 | 结果 |
|---|---:|
| 文件数 | 706 |
| 总行数 | 133,532 |
| 中位文件行数 | 141 |
| 最大文件行数 | 727 |
| 超过 1,500 行 | 0 |
| 超过 5,000 行 | 0 |

本表已在源码目录重组和后续手工 recon 重构后重新扫描。活跃生产源码现在全部位于
`native/src/{bonds,calendar,cloud,common,corporate,data,derivatives,exchange,formula,funds,
industry,institution,leverage,market,protocol,recon,registry,research,server,trading}/` 二十个领域
目录中；CMake 唯一引用全部 706 个 `.cpp`，缺失引用、孤立源文件和跨目录重名均为 0。
旧审计中的 `native/src/<file>.cpp` 活跃路径和 690 文件统计已经失效，表内历史条目的
`original_file` 仅表示重构前来源，不再表示当前物理路径。

原先最大的 `recon_contract_formula_render.cpp` 已进一步按 8 个契约族拆分，根分派器缩减到
20 行。`intelligence.cpp` 也已按资源目录、归一化、关系图、查询编排和 CLI 拆为 5 个实现
单元，原根文件已移除。`cloud_calc.cpp` 也已将解释器外围的审计、模板、单行请求、批量
请求和 CLI 分开。原 1,212 行的 `calendar.cpp` 已按支持函数、归一化、服务编排和 CLI 拆为
4 个实现单元，24 个视图与 28 个资源常量集中到类型化内部目录。原 1,212 行的
`market.cpp` 也已按协议、投影、传输、服务、持久会话与命令拆开。证券关系上下文随后拆为
跨证券时序、证券身份、行业上下文、共享判定和组合根。其上层 `formula_context.cpp` 又将
分析计划、宿主数据、本地元数据和财务扩展拆成四个阶段，根文件缩至 239 行。原 1,083 行
的杠杆资金单体也已按两类归一化、两套查询服务、缓存支持和 CLI 拆开。公式 HTTP 处理层
随后按目录/审计、云计算、GET 执行、内联 POST、回测和扫描/策略拆为 7 个单元。原 1,039
行的 `futures_issuance.cpp` 又按公共支持、期货、IPO、再融资、服务编排和 CLI 拆为 6 个
实现单元，并将 16 个固定资源集中到类型化目录。随后原 1,035 行的 TDX 专用指标文件从
“18 个名称映射同一个条件分派器”改为四个真实策略族和 18 项类型化函数目录；当前最大
生产文件降为 1,034 行的 `formula_analysis.cpp`。该文件随后也按语义审计、单公式分析、整库
覆盖率和显式上下文模板拆成四个实现单元；当前已经没有超过 1,000 行的生产 `.cpp`，最大
文件原为 988 行的 `stats.cpp`。该文件又按 0x06B9/ZIP、三类文本解析、JSON/估值投影、
下载服务和 CLI 拆为六个实现单元。随后常用指标计算器也将 26 路条件分派改成统一类型化
策略目录，并按三类算法、公共支持、文档调度和 CLI 拆开。资金流后续表现链路又将 15 个
视图别名与 4 组模型请求集中到编译期目录，并把历史归一化、模型归一化、来源审计、服务和
CLI 拆为 6 个实现单元；原 970 行根文件已移除。公式策略流水线随后按定义归一化、上下文、
求值、扫描、组合回测、命令编排和公共支持拆成 7 个实现单元，原 952 行根文件也已移除。
同为 952 行的专业数据文件又按 manifest/缓存、ZIP、目录、解析、获取、投影、CLI 和公共
支持拆成 8 个实现单元。限售解禁链路随后将四类归一化、三项主资源、四类缓存、查询编排
和 CLI 拆为 5 个实现单元，原 909 行根文件已移除。评级链路又将三项主资源改为类型化
目录，并把名称补全、三类汇总、四类归一化、缓存、查询和 CLI 拆成 7 个实现单元。当前
机构分析随后把 25 项视图提升为编译期目录，并按目录、支持、归一化、汇总、文档组合、缓存
服务和 CLI 拆为 7 个实现单元。云计算解释器宿主又按 L1、封单、板块聚合、财务/转债和
最终绑定编排拆为 5 个策略单元。机构持仓与股东链路又按 TQLEX/缓存、公共支持、两类归一化、
两类查询服务和 CLI 拆为 7 个实现单元。板块基础链路随后将五类板块与三类市场改为编译期
类型目录，并按支持、TNF/行业解析、主营业务/缓存、自定义/组合板块、综合装载和 CLI 拆为
6 个实现单元。交易所基金链路又将 22 个资源与 11 种基金语义改为两张编译期类型目录，
并按目录、公共支持、归一化、行情派生、抓取缓存、查询服务和 CLI 拆为 7 个实现单元。
大宗交易链路随后将三项核心资源、四个营业部周期与四种视图目录化，并按公共支持、两类
归一化、抓取缓存、查询服务和 CLI 拆为 6 个实现单元。当前最大生产文件为 837 行的
基金分析随后将 13 个视图改成行处理函数指针与排序策略目录，并把 18 项风格和三项基准
目录化；支持、三类归一化、策略调度、服务和 CLI 拆为 8 个实现单元。业绩预告链路又将
三项资源、四种视图和四类情绪目录化，并按支持、三类归一化、汇总、抓取缓存、查询和 CLI
拆为 9 个实现单元。国企改革链路随后把四个维度、五种视图和两类排序目录化，并按支持、
排序、两类归一化、抓取缓存、查询、响应和 CLI 拆为 9 个实现单元。公式横向聚合上下文又
抽取共享 universe resolver，并把 `HORCALC` 与 `INSORT/INSUM` 分成独立算法单元；两种成员
顺序策略显式建模，避免破坏成员序号语义。市场 catalog 文件随后确认实际承载 33 个查询
适配器，按基金债券、主题发现、公司研究和市场分析四个领域拆开。公式资源提取随后把 5 类
公式、18 项恢复源码、PE 资源读取、图标转换、5,072 字节记录解析和 CLI 分成 5 个实现单元，
原 812 行根文件已移除。企业与精选数据契约随后把七项响应验证器从条件链提升为编译期
类型化目录，并拆成七个独立实现，根分派器缩至 46 行。特殊情形链路随后将十项资源改为
带重复检查的 `ResourceDefinition` 目录，并把公共解析、归一化、行情、抓取、查询和 CLI
拆成七个实现单元，原 794 行根文件已移除。债券参考资料链路随后将 24 项来源、20 项资源
画像、8 组投影和 2 个客户端对照表集中目录化，并把支持、归一化、抓取、查询和 CLI 拆为
六个实现单元。商品关联链路随后将七个视图、两项主资源、默认排序和排序能力统一到类型化
目录，并按三类归一化、缓存、查询与 CLI 拆为八个实现单元。公式扫描与持续监控随后按
公共支持、证券/K 线 universe、扫描核心、diff、watch state、单次编排和两条命令拆为九个
实现单元。披露命令随后把四类 universe 输入适配器与命令编排分离，并将五项披露资源改为
带归一化函数的类型目录。TQLEX 协议层又将配置文本、响应模型、目录、请求、传输、执行和
CLI 拆为七个实现单元，以类型化分页策略和集中默认值约束控制流。港股事件链路随后将四项
资源、事件类型、标签和视图合并为编译期唯一的类型目录，并把字段支持、事件归一化、历史沽空
对账、抓取缓存、查询和双 CLI 拆为七个实现单元。行业画像链路又将四个持仓周期和股东主资源
集中为类型目录，并把字段投影、两类归一化、拓扑、抓取缓存、查询与 CLI 拆为八个实现单元；
内部 API 进入模块子命名空间。财务洞察链路随后把 15 项资源/视图/标签和 6 种排序改为
编译期类型目录，并将数值投影、15 类归一化、抓取缓存、查询和 CLI 拆为六个实现单元。
题材机会链路又将五项资源及其角色、分组字段、明细前缀统一为类型目录，并把两类归一化、
抓取缓存、查询和 CLI 拆为七个实现单元。公式基础契约随后将五项契约改为类型化分派，并用
39 项成员指针绑定统一解释器能力数组、计数字段和预期集合；图标、覆盖率、服务目录与
OpenAPI 各自进入独立实现。债券市场契约又将十项契约改为编译期目录，并按参考主表、
评级/政府债、待发、申购、可交换债和定价拆为七个单元。可转债服务随后将五类缓存抓取与
四种视图查询分离，四项视图由编译期目录解析后交给私有处理器。强势股链路又把四项视图、
主资源、默认值、九项区间排序和五项详情排序集中到同一目录，并按支持、摘要、归一化、排序、
服务和 CLI 分层。TPool 流程运行时随后将八周期映射改为类型目录，并把无状态投影、有状态
调度和告警差异分离。云计算解释器随后将 36 项内建目录、日期/债券现金流算法和逐 ID 执行器
拆开，并增加目录唯一性检查。对账与诊断编排随后把安装盘点、响应校验、HTTP 巡检、两个
CLI 适配器和 doctor 拆成六个单元；安装命令复用唯一盘点入口，224 项帮助列表直接从契约
目录生成。市场研究链路又将九类主目录和四种动态明细命名空间提升为编译期唯一的类型目录，
并把三类归一化、缓存抓取、查询组合和 CLI 拆为七个实现单元；相关断言从通用测试迁入独立
专项。股份回购链路随后把三项核心资源、三类年度段和四种视图提升为编译期唯一目录，并按
四类归一化、缓存抓取、查询和 CLI 拆为八个单元；原先未使用的全局 `--jsn-root` 已接入
回购服务，本地来源补齐完整韧性元数据。个股/行业估值链路又将五类 view、十个别名、
五项 PBRPC 请求模板、归一化函数和排序规则集中为同一 `ViewSpec` 目录，并按基础转换、
身份投影、两类归一化、统一调度、请求、服务和 CLI 拆成九个单元，最大单元 184 行。
DDX 资金强度链路随后将五个周期、二十个别名、四种 view 和十三项排序集中为三类类型化
目录，资源字段、默认值和排序能力不再散落在服务条件链中；九个实现单元最大为 158 行。
战略主题层级链路又将 26 类资源、五种 view 和三种排序集中到编译期目录，并把主资源合并、
动态逐股明细和两级缓存拆开；九个实现单元最大为 190 行。
股东信号链路随后将五资源、六 view、八排序和四套 CFG 公式绑定为类型化函数目录,并把
牛散目录、逐人持仓、本地/远端获取和合计对账分层；九个实现单元最大为 197 行。涨跌停复盘
链路又将五项固定资源、六种 view 和七种归一化策略集中为类型目录，以 `QueryPlan` 统一决定
分类校验、动态日期资源、缺失容错和单票历史；九个实现单元最大为 201 行。相对估值链路
随后将五类指数、五个基准、三种估值方法及九个方法别名集中为编译期目录，`QueryPlan` 统一
生成日期窗口、替换表和缓存键，TQLEX 主表与 PBRPC 明细复用同一缓存/重试模板；八个实现
单元最大为 165 行。异常风险链路又把两种真实 view、六个 view 别名、四种预警状态和八个
预警别名集中为编译期目录，view 直接绑定请求号、XML、DLL 模块、分页参数和归一化函数；
九个实现单元最大为 151 行。阈值股票链路随后将两类 universe、八个别名、四种 view 和
11 项排序集中为三张类型目录，universe 绑定主资源、标签和趋势字段，view 绑定成员加载与
单票约束；十个实现单元最大为 152 行。国债逆回购链路又把三种 view 与 11 项排序集中为
类型目录，并将 JSN 日历、公开 L1、收益计算、双缓存及行情失败降级分层；九个实现单元最大
为 105 行。分时主力资金链路随后把七个时段 key/字段后缀/时长集中为编译期目录，并将研究
行业拓扑、`200340/200341` 双层请求、四种投影、反向索引和 CLI 分为九个实现单元，最大为
102 行。公告信号链路又把精选/风险两项资源集中为编译期目录，并将公共支持、资源目录、
两类归一化（精选/风险、历史）、排序、抓取缓存、查询服务和 CLI 拆为七个实现单元，最大为
183 行。精选数据链路随后把八项 BigData 资源与其 kind/标签绑定为编译期目录，并将公共
支持、资源目录、证券身份（含港股 `tdxhkag.cfg` 名称解析）、本地/远端资源加载、八类
归一化、抓取缓存、查询服务和 CLI 拆为八个实现单元，最大为 162 行。逐笔成交链路随后把
`0x0FC5`/`0x0FC6`/`0x0004` 三个协议命令与分页上限固定为编译期常量，并将公共支持、
带符号 varint 记录解码、请求组装与载荷解析、三窗口聚合、投影、分页抓取与端点降级、
CLI 拆为七个实现单元，最大为 181 行。对账契约集成层随后把一条 13 分支的 `contract_id`
判断链改为编译期派发表，22 项契约按排名、日历、解禁、个股韧性和平台五个域分为六个实现
单元，最大为 161 行；其中 11 项个股契约的 schema/mode 由两张运行期 `std::map` 改为一张
`constexpr` 表，73 条断言逐条保留。

当前物理最大生产文件是已搁置的 727 行 `market/level2.cpp`；排除 L2 后的最大文件为 713 行共享
解析器 `data/jsn.cpp`；645 行的 `formula/formula_functions_series.cpp` 属共享解释器语义，需要宽回归，
637 行的 `tpool.cpp` 也涉及 XML/公式兼容解析。至此 600 行以上的生产文件已全部落在受控
名单内。对账研究目录层随后用同一手法把 12 项契约的判断链改为编译期派发表，四组共享前置
断言提为 helper，按预测、指标、题材库、战略、题材、炒作分为七个实现单元，最大为 180 行，
56 条断言逐条保留。题材库随后按同一职责链路拆分：17 个文件级 helper 收进内部头，五个
ZTTZ 母表目录、三个 normalize 函数、母表与动态两个缓存域、158 行的 `query` 以及 CLI
适配各自独立成单元，最大为 195 行，三个样本经字段归一后哈希完全一致。

因子链路随后处理了树内单函数集中度最高的一处：`FactorService::query` 占 595 行中的
571 行（96%），与治理前的 `formula_render_events.cpp`（572/589）同一种病灶。该函数实为
一个视图分派器，按四条分支拆成矩阵、广度、个股反查和表格四个单元，根文件收敛到 49 行
只保留校验、缓存探测与分派；四份重复的 cache 片段、七处 sources 追加循环、三处分页截断
和三条重试循环分别归并为共享 helper。八个样本加 23 条校验拒绝路径验证等价，其中三个样本
逐字节一致。

经济指标链路随后按同一套责任分层处理：584 行中 `EconomicIndicatorService::query` 占 199 行
（34%），且历史序列、关联证券、行情附着三段可选细节各自要产出多个字段。拆分把 19 个私有
helper、3 个 normalize 函数、两个缓存域、5 个查询阶段 helper、3 段可选细节和 CLI 适配分成
6 个单元，最大 173 行，`query` 收敛到 80 行只做校验、母表拉取与两视图分派；13 个原本平铺的
出参归入一个 `QueryState`。17 个响应键的插入顺序保持不变，其中 `related_security_count`
仍排在 `history_points` 之前。15 个样本（两视图、五种排序、分页、过滤、三个开关、刷新与
自定义 TTL）加 22 条校验拒绝路径验证等价，8 个样本逐字节一致；其余差异经基线自比对确认
只是时间戳、缓存年龄与网络重试次数。

命令注册表随后按领域分组处理：586 行中 `command_registry()` 的初始化字面量占 453 行（77%），
该字面量不是函数但构成单一不可独立测试的长段。该文件既是中心 CLI 分派表又是 HTTP
`/api/v1/features`、`/api/v1/openapi.json` 的唯一来源；149 个表项分散在 12 个类别，顺序
不可重排。拆分按表项在向量中的原始顺序分为 11 个领域单元（云网关、公式、静态资源、市场参考、
市场企业、市场行情、市场信息披露、市场异动、市场场次、市场估值、系统），每单元只包含自己
表项真实依赖的 command handler 头文件（5 到 23 个），根文件收敛到 41 行只调用 11 个 append
函数；内部头声明 11 个 append 原型，代码生成器保证每单元的表项文本逐字节提取自原文件。
`--help`、`help <command>` 与 `<unknown command>` 的输出逐字节一致；`/api/v1/features`
（149 entries）与 `/api/v1/openapi.json`（151 paths）的序列化 JSON 也逐字节一致。11 个实现
单元最大为 109 行。

旧候选 `data/disclosures_command.cpp` 和 `formula/formula_environment.cpp` 已分别完成职责拆分，
不再是待办。当前没有生产 `.cpp` 越过仓库规定的 5,000 行软阈值，也没有测试 `.cpp` 越过
2,500 行软阈值，因此后续不再按 500—700 行排名机械拆分。受控高耦合名单为
`data/jsn.cpp`、`formula/formula_functions_series.cpp`、`cloud/tpool.cpp`、
`formula/formula_context_dynamic.cpp`、`cloud/pbrpc.cpp` 与 `cloud/tpool_evaluate.cpp`；只有在
出现已证实的新职责、真实缺陷或功能需求时，才先划定语义域并走定向宽回归。

## 测试代码

解释器专项测试原先把 4,393 行和 359 个断言调用全部放在一个 `main()`。现在仍保留同一个
`tdx-formula-engine-tests` 可执行文件，但拆成共享 fixture 和五个语义域；入口 51 行，
最大测试单元 1,417 行。测试域通过五项类型化目录顺序调度，失败消息会携带域名，并支持
`--domain` 仅执行指定语义域。

API 契约测试也从 3,883 行单体拆为七个契约族、共享支持和 54 行入口；288 个断言调用
保持原顺序，并支持 `--domain` 仅执行指定契约族。板块基础链路的 230 行专项测试还覆盖
板块报价代码、唯一名称和同名歧义，后续无需借整套解释器测试验证底层解析。大宗交易断言
又从通用 `native_tests.cpp` 迁入独立的 139 行专项目标；TQLEX 的模板、
传输、分页和错误断言随后迁入独立的 118 行专项目标；行业画像三组断言又迁入 114 行专项
目标，使通用文件降至 1,302 行。当前 `native/tests` 有 122 个 `.cpp`、共 22,092 行，
中位数 102 行，最大文件为 1,465 行，
已经没有超过 1,500、3,000 或 5,000 行的测试文件。

## 本轮落地方式

| 原文件 | 原行数 | 根文件现行数 | 主要结构手段 |
|---|---:|---:|---|
| `factors.cpp` | 1,458 | 595 | 视图目录、归一化、广度、快照、服务、CLI 分层 |
| `formula_render.cpp` | 1,419 | 77 | 五阶段渲染流水线、只读上下文端口、类型目录 |
| `technical_signals.cpp` | 1,297 | 243 | 统一 32 视图目录、四类策略目录、服务与 CLI 分层 |
| `recon_contract_formula_render.cpp` | 1,268 | 20 | 19 项类型化契约目录、8 个验证器族 |
| `intelligence.cpp` | 1,225 | 已移除 | 12 视图类型目录、资源策略、归一化、关系图、服务与 CLI 分层 |
| `cloud_calc.cpp` | 1,214 | 已移除 | schema 常量、审计、模板、单行/批量执行、CLI 分层 |
| `calendar.cpp` | 1,212 | 已移除 | 24 视图类型目录、28 资源常量、归一化、服务与 CLI 分层 |
| `market.cpp` | 1,212 | 已移除 | 三类协议目录、13 条价格规则、解码/投影/传输/会话/CLI 分层 |
| `formula_context_relations.cpp` | 1,137 | 140 | 跨证券时序、证券身份、行业估值/序列、关系组合分层 |
| `formula_context.cpp` | 1,108 | 239 | 17 类绑定计划、宿主阶段、本地元数据、财务扩展与组合根分层 |
| `leverage.cpp` | 1,083 | 已移除 | 41 项资源目录、两类归一化、融资/互联查询、缓存与 CLI 分层 |
| `server_formula.cpp` | 1,056 | 已移除 | 共享请求支持、目录/审计、云计算、执行、内联、回测、扫描策略分层 |
| `futures_issuance.cpp` | 1,039 | 已移除 | 16 项资源目录、13 项选项目录、三类归一化、服务与 CLI 分层 |
| `formula_functions_tdx_indicators.cpp` | 1,035 | 已移除 | 18 项类型化策略目录、四类处理器、共享递归算法和 15 行分派入口 |
| `formula_analysis.cpp` | 1,034 | 已移除 | 语义污染审计、单公式依赖分析、整库能力汇总和显式上下文模板分层 |
| `stats.cpp` | 988 | 已移除 | 0x06B9 协议、ZIP 安全解压、三类资源解析、估值投影、服务和 CLI 分层 |
| `formula_calc.cpp` | 972 | 已移除 | 26 项类型化策略目录、三类指标算法、公共滚动工具、文档调度和 CLI 分层 |
| `flow_followup.cpp` | 970 | 已移除 | 15 项视图别名、4 组模型定义、历史/模型归一化、来源审计、服务和 CLI 分层 |
| `formula_strategy.cpp` | 952 | 已移除 | 定义归一化、外部上下文、逐证券求值、扫描、组合回测、并发命令和共享端口分层 |
| `professional_data.cpp` | 952 | 已移除 | 固定资源边界、manifest/缓存、ZIP、101 项字段目录、解析、获取、投影和 CLI 分层 |
| `unlocks.cpp` | 909 | 已移除 | 四类归一化、三项资源常量、主/明细缓存、查询编排和 CLI 分层 |
| `ratings.cpp` | 887 | 已移除 | 三项资源目录、三类身份、名称补全、三类汇总、四类归一化、缓存/查询/CLI 分层 |
| `institution_analysis.cpp` | 879 | 已移除 | 25 项类型化视图、12 种布局、归一化、特殊汇总、组合、缓存和 CLI 分层 |
| `cloud_calc_host.cpp` | 873 | 已移除 | 44 项宿主列、L1、封单、板块聚合、财务/转债和绑定编排策略分层 |
| `institution.cpp` | 870 | 已移除 | TQLEX/缓存、公共支持、证券/股东归一化、两类查询服务和 CLI 分层 |
| `blocks.cpp` | 868 | 已移除 | 五类板块/三类市场目录、解析、缓存、本地板块、装载和 CLI 分层 |
| `exchange_funds.cpp` | 857 | 已移除 | 22 项资源/11 种类型目录、归一化、行情、缓存、查询和 CLI 分层 |
| `block_trades.cpp` | 848 | 已移除 | 核心资源/周期/视图目录、两类归一化、缓存、四模式查询和 CLI 分层 |
| `fund_analytics.cpp` | 837 | 已移除 | 13 项函数指针视图策略、18 项风格、3 项基准、三类归一化、服务和 CLI 分层 |
| `forecasts.cpp` | 828 | 已移除 | 3 项资源、4 项视图、4 类情绪目录、三类归一化、汇总、缓存、查询和 CLI 分层 |
| `state_owned_reform.cpp` | 823 | 已移除 | 4 个维度、5 种视图、两类排序、三类归一化、缓存、响应和 CLI 分层 |
| `formula_context_aggregate.cpp` | 820 | 已移除 | 共享 universe resolver、HORCALC、INSORT/INSUM 与两种成员顺序策略分层 |
| `server_market_catalog.cpp` | 814 | 已移除 | 33 个 HTTP 查询适配器按基金债券、主题发现、公司研究和市场分析四域分层 |
| `formulas.cpp` | 812 | 已移除 | 5 类公式/18 项恢复源码目录、PE 资源、图标、记录提取和 CLI 分层 |
| `recon_contract_market_corporate_catalog.cpp` | 810 | 46 | 7 项响应契约类型目录和逐契约实现单元 |
| `special_situations.cpp` | 794 | 已移除 | 10 项资源类型目录、归一化、行情、抓取、查询和 CLI 分层 |
| `bond_reference.cpp` | 785 | 已移除 | 24 项来源/20 项资源画像、投影对账、归一化、抓取、查询和 CLI 分层 |
| `commodity_links.cpp` | 780 | 已移除 | 7 项视图目录、三类归一化、缓存、反向关联查询和 CLI 分层 |
| `formula_scan.cpp` | 778 | 已移除 | universe、扫描核心、diff、watch state、单次编排与持续监控分层 |
| `disclosures.cpp` | 765 | 已移除 | 5 项资源策略、四类 universe 输入和多模式命令编排分层 |
| `tqlex.cpp` | 757 | 已移除 | 配置文本、响应模型、目录、请求、传输、执行和 CLI 分层 |
| `hk_events.cpp` | 749 | 已移除 | 四项资源目录、公共支持、两类归一化、抓取、查询和双 CLI 分层 |
| `industry_profile.cpp` | 744 | 已移除 | 周期资源目录、字段投影、两类归一化、拓扑、缓存、查询和 CLI 分层 |
| `financial_insights.cpp` | 742 | 已移除 | 15 项资源/6 种排序目录、数值投影、归一化、缓存、查询和 CLI 分层 |
| `thematic_opportunities.cpp` | 731 | 已移除 | 五项资源/六视图类型目录、两类归一化、缓存、查询和 CLI 分层 |
| `recon_contract_formula_foundation.cpp` | 715 | 已移除 | 五项契约目录、39 项能力绑定、图标/覆盖率/服务/OpenAPI 分层 |
| `recon_contract_market_data_bonds.cpp` | 707 | 已移除 | 十项契约目录、参考债/政府债/待发/申购/可交换债/定价分层 |
| `convertible_bonds_service.cpp` | 698 | 已移除 | 五类缓存抓取、四视图目录和独立查询响应分层 |
| `strong_stocks.cpp` | 694 | 已移除 | 四视图/两排序族目录、区间/详情归一化、摘要、服务和 CLI 分层 |
| `tpool_flow.cpp` | 692 | 已移除 | 八周期目录、调度模型、拓扑投影、有状态推进和告警 diff 分层 |
| `cloud_calc_builtins.cpp` | 692 | 已移除 | 36 项目录、日期/现金流算法与逐 ID 执行策略分层 |
| `equity_valuation.cpp` | 671 | 已移除 | 五 view/十别名类型目录、两类归一化、请求模板、缓存服务和 CLI 分层 |
| `capital_strength.cpp` | 669 | 已移除 | 五周期/四 view/十三排序目录、归一化、共振、查询计划、缓存服务和 CLI 分层 |
| `strategic_themes.cpp` | 663 | 已移除 | 26 类资源/五 view/三排序目录、主/明细归一化、两级缓存、服务和 CLI 分层 |
| `shareholder_signals.cpp` | 661 | 已移除 | 五资源/六 view/八排序目录、四类公式策略、牛散持仓、本地回退、服务和 CLI 分层 |
| `limit_review.cpp` | 644 | 已移除 | 五资源/六 view/七类归一化策略、动态资源计划、缺失容错、服务和 CLI 分层 |
| `relative_valuation.cpp` | 639 | 已移除 | 五类指数/五基准/三方法目录、查询计划、两阶段共享缓存重试、服务和 CLI 分层 |
| `anomaly_risk.cpp` | 635 | 已移除 | 两 view/四预警目录、证据投影、两类归一化、查询计划、响应、缓存和 CLI 分层 |
| `threshold_stocks.cpp` | 634 | 已移除 | 两 universe/四 view/十一排序目录、三类归一化、对账、缓存、服务和 CLI 分层 |
| `reverse_repo.cpp` | 630 | 已移除 | 三 view/十一排序目录、日历/L1 联结、收益计算、双缓存、降级和 CLI 分层 |
| `funds.cpp` | 624 | 已移除 | 七时段目录、研究行业拓扑、双层请求、四种投影、双缓存和 CLI 分层 |
| `announcement_signals.cpp` | 612 | 已移除 | 两项资源目录、两类归一化（精选/风险、历史）、七种排序、缓存、查询和 CLI 分层 |
| `curated_data.cpp` | 608 | 已移除 | 八项资源目录、港股名称解析、八类归一化、九种 view、缓存、服务和 CLI 分层 |
| `trades.cpp` | 605 | 已移除 | 协议常量、varint 记录解码、请求/载荷、三窗口聚合、投影、分页抓取降级和 CLI 分层 |
| `recon_contract_market_corporate_integration.cpp` | 610 | 已移除 | 22 项契约的编译期派发表、11 项个股韧性类型表、排名/日历/解禁/个股/平台五域分层 |
| `recon_contract_market_research_catalog.cpp` | 598 | 已移除 | 12 项契约的编译期派发表、四组共享前置断言 helper、预测/指标/题材库/战略/题材/炒作分层 |
| `formula_render_events.cpp` | 589 | 55 | 八图元编译期派发表、分段/填充/序列/区域/通用/标注六层、二进制等价验证 |
| `theme_library.cpp` | 594 | 0 | 17 helper 入内部头、五母表目录、双缓存域、`query` 再分四助手、三样本等价验证 |
| `factors.cpp` | 595 | 49 | 571 行 `query` 按四条分派分支成单元、重试策略与缓存/来源片段去重、八样本加 23 条校验路径等价验证 |
| `economic_indicators.cpp` | 584 | 0 | 19 helper 入内部头、三 normalize 与双缓存域分离、`QueryState` 归并 13 个出参、三段可选细节成私有成员、15 样本加 22 条校验路径等价验证 |
| `registry.cpp` | 586 | 41 | 按表项原始顺序分为 11 个领域 append 单元、每单元只含真实依赖的 handler 头文件、代码生成保证逐字节提取、`--help`/`features`/`openapi` 三项输出全部逐字节一致 |

上述模块均保留公开头文件和 JSON schema。固定映射使用编译期类型化目录；无状态业务策略使用
函数端口组合，没有为了“设计模式”引入不必要的继承层级。命令入口继续与领域实现分离，
常量目录不再散落在请求控制流中。

## 验证范围

- 本次市场情报拆分的 `tdx-intelligence-tests` 与 `tdx-tool` 编译链接通过；
- 市场情报专项测试与 CLI 帮助契约通过；
- `attention` 真实样例返回 `live`，schema 保持 `tdx-market-intelligence-native-v1`；
- 云端公式计算专项测试通过，`func_kzz_kzzsy101.cfg` 模板样例仍返回 19 个输入、3 个
  主机字段和 15 个计算字段；
- 拆分后的公式解释器测试重新编译并完整执行通过，仍包含 359 个断言调用；
- 拆分后的 API 契约测试重新编译并完整执行通过，仍包含 288 个断言调用；
- 市场日历四个实现单元与 `tdx-tool` 编译链接通过，`tdx-calendar-tests` 通过；
- `futures` 代表样例返回 3 条记录，schema 保持 `tdx-market-calendar-native-v1`，资源目录仍为
  28 项；
- 实时行情拆分后的 `tdx-native-tests` 与 `tdx-market-stream-tests` 通过；
- 平安银行单证券快照返回 1/1，schema 保持 `tdx-market-snapshot-native-v1`，命令号保持
  `0x054C`；
- 公式证券关系上下文拆分后 `tdx-formula-engine-tests` 通过；
- 可转债关系探针返回 120 根与 8 个输出，主指数仍为 `999999`，标的仍为 `600029`；
- 公式上下文薄根拆分后 `tdx-formula-engine-tests` 再次通过；
- 同一可转债探针继续返回 120 根、8 个输出，说明计划对象与四阶段编排保持关系语义；
- 杠杆资金拆分后的 `tdx-leverage-tests` 通过；
- 融资融券市场与北向资金流样例各返回 3 条 `live` 数据，两个原有 schema 均保持不变；
- 公式 HTTP 拆分后的 `tdx-formulas-tests` 与 `tdx-formula-strategy-tests` 通过；
- 新二进制在临时 8877 端口通过公式目录、technical 覆盖率和 MACD 解释执行三个 HTTP 契约；
- 期货/发行数据拆分后的 `tdx-futures-issuance-tests` 与 `tdx-tool` 构建通过；
- `futures` 真实样本继续使用 3 个资源，返回 74 个商品合约、90 条月度记录和 4 个股指合约，
  schema 保持 `tdx-futures-issuance-native-v1`；
- TDX 指标二次拆分后的 `tdx-formula-engine-tests` 与 `tdx-tool` 构建、测试通过；
- 平安银行 40 根日线 `TDXKDJ` 样本的末值仍为 K≈41.2522、D≈57.1403、J≈9.4759；
- 公式分析拆分后的 `tdx-formula-engine-tests` 与 `tdx-formulas-tests` 通过；
- 379 条公式重新分析后仍为 379 条语法支持、339 条上下文可执行、390 项静态注册证据，
  analysis schema 保持 v7 且注册边界继续完全分类；
- 市场统计拆分后的 `tdx-native-tests` 与 `tdx-tool` 通过；
- 0x06B9 真实样本解析 7,975 条 tdxstat、7,975 条 tdxstat2 和 5,618 条 tipinfo，
  `SZ000001` 精确返回 1 条且 schema 保持 `tdx-stats-native-v1`；
- 常用指标计算器拆分后的 `tdx-formula-calc-tests` 与 `tdx-tool` 通过；
- 平安银行 40 根日线 KDJ 继续按 `K,D,J` 输出，末值约为 41.2521、57.1394、9.4775；
- 资金流后续表现拆分后的 `tdx-flow-followup-tests` 与 `tdx-tool` 通过；
- 融资模型真实样本返回 3 行和两路 TQLEX 来源，schema 保持 `tdx-flow-followup-native-v2`；
- 公式策略拆分后的 `tdx-formula-strategy-tests` 与 `tdx-tool` 通过；
- 平安银行 MA5 策略扫描 1/1 成功、无抓取错误，schema 保持 `tdx-formula-strategy-scan-v1`；
- 专业数据拆分后的 `tdx-professional-data-tests` 与 `tdx-tool` 通过；
- 官方专业数据目录返回 147 个财务包与 8,262 个交易文件，schema 保持 `tdx-professional-catalog-v1`；
- 限售解禁拆分后的 `tdx-unlock-monthly-tests` 与 `tdx-tool` 通过；
- 月度解禁压力样本返回 3 个月 `live` 数据，三项金额公式校验全部一致，schema 保持
  `tdx-market-unlocks-native-v1`；
- 评级拆分后的 `tdx-ratings-tests` 与 `tdx-tool` 通过；
- 港股评级目录样本返回 619 只主目录证券、3 条结果、三路来源且无明细错误，schema 保持
  `tdx-market-ratings-native-v1`；
- 机构分析拆分后的 `tdx-institution-analysis-tests` 与 `tdx-tool` 通过；
- 汇金证金样本的 191 条组合比例公式全部一致、0 个不匹配，schema 保持
  `tdx-institution-analysis-native-v1`；
- 云计算宿主拆分后的 `tdx-cloud-calc-tests` 与 `tdx-tool` 通过；
- 可转债模板继续返回 19 个输入、3 个宿主字段和 15 个计算字段；
- 机构持仓与股东链路拆分后的 `tdx-institution-lhb-tests`、`tdx-institution-resilience-tests`
  与 `tdx-tool` 通过；
- 平安银行样本继续返回机构历史、十大流通股东和可查询股东引用，两个来源且无错误，schema
  保持 `tdx-security-profile-native-v2`；
- 新增的 `tdx-blocks-tests` 与 `tdx-tool` 通过，专项覆盖六类底层文件/解析语义；
- 平安银行真实板块查询返回 34 个归属、覆盖五个板块族，34 个证券名称全部解析成功；
- 交易所基金拆分后的 `tdx-exchange-funds-tests` 与 `tdx-tool` 通过；
- ETF 份额榜真实样本返回 458 条匹配、22 个来源，抽取 3 条类型正确且无行情错误，schema
  保持 `tdx-market-exchange-funds-native-v1`；
- 新增的 `tdx-block-trades-tests` 与 `tdx-tool` 通过，通用 `tdx-native-tests` 重新编译通过；
- 大宗交易真实目录样本返回 13 个月度点、280 条近期成交、280 只证券、三项核心来源且无
  错误，schema 保持 `tdx-market-block-trades-native-v1`；
- 平安银行证券模式另返回一条历史成交；上游意向明细零长度被保留为 `partial` 容错证据；
- 基金分析拆分后的 `tdx-fund-analytics-tests` 与 `tdx-tool` 通过；
- `risk` 真实首页继续以请求 `500030` 返回 20 条，13 个视图和默认普通股票型映射正确，
  schema 保持 `tdx-fund-analytics-native-v2`；
- 业绩预告拆分后的 `tdx-forecasts-tests` 与 `tdx-tool` 通过；
- `latest` 真实样本汇总 1,775 条预告、30 个行业和三项来源，抽取 3 条且无明细错误，schema
  保持 `tdx-market-forecasts-native-v1`；
- 国企改革拆分后的 `tdx-state-owned-reform-tests` 与 `tdx-tool` 通过；
- “整合预期”真实样本汇总 111 个分组、1,968 条关系、899 只证券、49 条重组和五项来源，
  计数不一致为 0，schema 保持 `tdx-market-state-owned-reform-native-v1`；
- 新增 `tdx-formula-context-aggregate-tests` 14 行聚焦入口，只复用并执行 context-builder 域；
- `HORCALC` 求和/排名/加权与 `INSORT/INSUM` 六种模式聚焦测试通过，`tdx-tool` 增量链接通过；
- 市场查询适配器的 33 项声明与定义静态核对一致，`tdx-tool` 增量链接通过；
- `serve --self-test` 不监听端口并返回 `ok=true`、149 项功能、JSN 可用、5 项公式匹配和
  1 个银行板块/42 条成员关系；
- 公式资源提取拆分后的 `tdx-formulas-tests` 与 `tdx-tool` 通过；
- `TCalc.dll` 真实样本继续返回 379 项公式，其中嵌入 361、恢复 18、缺失 0，公开 schema
  保持 v4，图标 schema 保持 `tdx-formula-icon-sprite-v1`；
- 企业与精选数据契约拆分后的 `tdx-recon-contract-tests` 增量构建通过；
- `realtime-corporate` 与 `curated-issuance` 两个定向域在约 0.7 秒内分别通过，合计直接覆盖
  七个迁移契约及其关键反例；
- 特殊情形拆分后的 `tdx-special-situations-tests` 与 `tdx-tool` 通过；
- 本地十资源样本返回 4,036 条匹配和 10 个来源，`realtime-corporate` 定向契约通过，schema
  保持 `tdx-market-special-situations-native-v1`；
- 债券参考资料拆分后的 `tdx-bond-reference-tests` 与 `tdx-tool` 通过；
- 政策性金融债样本的 10 条主表与 10 条投影精确一致，40 亿元正确换算为 40 亿人民币，
  `calendar-resilience-web` 定向契约通过，schema 保持 `tdx-market-bond-reference-native-v1`；
- 商品关联拆分后的 `tdx-commodity-links-tests` 与 `tdx-tool` 通过；
- 实时样本继续返回 221 个报价行、219 个唯一商品和六个目录项，`research-signals` 定向契约
  通过，schema 保持 `tdx-market-commodity-links-native-v1`；
- 公式扫描拆分后的 `tdx-formula-engine-tests` 与 `tdx-tool` 通过；
- `workflow` 域单独执行通过，“BIAS卖出”样本输入/求值为 1/1 且两类错误均为 0，
  `formula-render-workflow` 契约通过，解释器仍为 `tdx-source-interpreter-v1`；
- 披露命令拆分后的 `tdx-disclosures-tests` 与 `tdx-tool` 通过；实时 schedule 样本汇总
  6,865 行和 5 个来源，schema 保持 `tdx-market-disclosures-native-v1`；
- TQLEX 拆分后的 `tdx-tqlex-tests` 与 `tdx-tool` 通过；真实配置清单返回 73 项配置、
  11 个 Entry 和 34 个 ReqId，`formula-evaluation` 定向契约通过；
- 港股事件拆分后的 `tdx-hk-events-tests` 与 `tdx-tool` 通过；真实本地样本汇总 5,674 行，
  四项来源齐全，`realtime-corporate` 定向契约通过；
- 行业画像拆分后的 `tdx-industry-profile-tests` 与 `tdx-tool` 通过；平安银行真实样本返回
  467 个行业、10 个持仓历史点、1 条逐股画像、7 个来源和 0 个详情错误，
  `calendar-resilience-web` 定向契约通过；
- 财务洞察拆分后的 `tdx-financial-insights-tests` 与 `tdx-tool` 通过；真实本地样本汇总
  2,480 条、15 项视图和 15 个来源，`calendar-resilience-web` 定向契约通过；
- 题材机会拆分后的 `tdx-thematic-opportunities-tests` 与 `tdx-tool` 通过；真实上游样本返回
  15 个分组、五项来源且 availability 为 `live`，`research-signals` 定向契约通过；
- 公式基础契约拆分后的 `tdx-recon-contract-tests` 与 `tdx-tool` 通过；两个相关测试域通过，
  临时服务上的 health/features/openapi/formula-icons 四项真实 HTTP 契约为 4/4；
- 债券市场契约拆分后的 `tdx-recon-contract-tests` 与 `tdx-tool` 通过；`market-core` 和
  `calendar-resilience-web` 两个定向域直接覆盖全部十项契约并通过；
- 可转债服务拆分后的 `tdx-convertible-bonds-tests` 与 `tdx-tool` 通过；真实申购样本返回
  323 条匹配、两项来源和 20 条新债投影，`market-core` 定向契约通过；
- 强势股链路拆分后的 `tdx-strong-stocks-tests` 与 `tdx-tool` 通过；真实样本返回 614 个
  区间和 562 只证券，`research-signals` 定向契约通过；
- TPool 流程拆分后的 `tdx-tpool-tests` 与 `tdx-tool` 通过；双单元一 flow fixture 的引用、
  无环拓扑和只读投影均验证通过；
- 云计算内建拆分后的 `tdx-cloud-calc-tests` 与 `tdx-tool` 通过；36 项目录全部实现，31 项
  非当前日期内建执行零错误，可转债真实模板仍为 19/3/15 字段；
- 对账与诊断编排拆分后的 `tdx-recon-contract-tests` 与 `tdx-tool` 构建通过；`market-core`
  定向域通过，`api-contracts --help` 自动生成的 224 项与契约目录逐项数量一致；
- 新增 `tdx-research-tests` 并从通用测试迁移三组研究断言；专项与 `stock-research-live` 单项
  HTTP 契约通过，真实主目录样本返回 9 类、5,598 行、9 个来源且无明细错误；
- 回购拆分后的 `tdx-repurchases-tests` 与 `tdx-tool` 通过；本地六资源样本返回 1,220 条计划、
  13 个月度点、5 个年度点和 167 只港股，`stock-repurchases-live` 的 13 项断言全部通过；
- 个股/行业估值拆分后的 `tdx-equity-valuation-tests` 与 `tdx-tool` 通过；真实 PE 行业样本由
  请求 `200302` 返回 30 行、抽取 3 行，schema 保持 `tdx-equity-valuation-native-v1`；
- DDX 资金强度拆分后的 `tdx-capital-strength-tests` 与 `tdx-tool` 通过；真实 5 日榜返回
  100 行、抽取 3 行，来源 DDX 保持降序，schema 保持 `tdx-market-capital-strength-native-v1`；
- 战略主题拆分后的 `tdx-strategic-themes-tests` 与 `tdx-tool` 通过；真实目录返回 26 类、
  598 个主题、655 条分类归属和 26 路来源，schema 保持 `tdx-market-strategic-themes-native-v1`；
- 股东信号拆分后的 `tdx-shareholder-signals-tests` 与 `tdx-tool` 通过；本地真实样本返回
  1,085 条信号和 1,755 位牛散目录成员，schema 保持 `tdx-market-shareholder-signals-native-v1`；
- 涨跌停复盘拆分后的 `tdx-limit-review-tests` 与 `tdx-tool` 通过；真实当日样本从三路资源
  返回 198 行、抽取 3 行且 availability 为 `live`，schema 保持 `tdx-market-limit-review-native-v1`；
- 相对估值拆分后的 `tdx-relative-valuation-tests` 与 `tdx-tool` 通过；真实完整路径返回
  180 个指数和 483 个历史点，选中 `SH000019` 后保留最近 3 点，schema 保持
  `tdx-relative-valuation-native-v1`；
- 异常风险拆分后的 `tdx-anomaly-risk-tests` 与 `tdx-tool` 通过；真实 `2044` 全分页请求返回
  837 行、5 条预警，结果集声明与解码一致，schema 保持 `tdx-anomaly-risk-native-v1`；
- 阈值股票拆分后的 `tdx-threshold-stocks-tests` 与 `tdx-tool` 通过；真实百元股成员样本返回
  233 个成员和 119 个趋势点，主表/成员及主表/趋势计数均一致，schema 保持
  `tdx-market-threshold-stocks-native-v1`；
- 国债逆回购拆分后的 `tdx-reverse-repo-tests` 与 `tdx-tool` 通过；真实样本 18 个品种全部
  取得公开 L1 行情和名称，沪深各 9 个且无 warning，schema 保持
  `tdx-market-reverse-repo-native-v1`；
- 分时主力资金拆分后的 `tdx-funds-resilience-tests` 与 `tdx-tool` 通过；平安银行真实路径
  解析到 `881385 银行`，两阶段请求返回 42 个成分并命中单票，schema 保持
  `tdx-intraday-funds-native-v1`；
- 经济指标拆分后的 `tdx-economic-indicators-tests` 与 `tdx-tool` 通过；15 个真实样本覆盖
  两视图、五种排序、分页、过滤、三个开关、`--refresh` 与自定义 TTL，全部与拆分前二进制
  等价（8 个逐字节一致，其余仅差时间戳、缓存年龄与网络重试次数，已由基线自比对确认为
  运行间漂移），22 条校验拒绝路径逐字一致，schema 保持
  `tdx-market-economic-indicators-native-v1`；
- 命令注册表拆分后的 `tdx-tool` 重新构建通过；`--help`、`help market economic-indicators` 与
  未知命令输出逐字节一致；在临时端口启动 `serve` 后，`/api/v1/features` 返回 149 entries
  与 `/api/v1/openapi.json` 返回 151 paths 的 JSON 序列化均与拆分前逐字节一致，表项顺序、
  handler 集合与公开 endpoint 映射全部保持；
- 本轮未改变公开解析语义、传输或 API schema，并补充了直接专项测试，因此按增量验证规则
  未运行完整 CTest。
- 随后的多页 K 线修复触及共享输出顺序：`tdx-native-tests` 与
  `tdx-recon-contract-tests` 专项通过，银行板块 1,600 根真实分钟线和两个 HTTP 样本均无
  倒序边界，`market-kline-30m-live` 的 `chronological_bars` 定向合约通过；按规则执行一次
  完整 CTest，108/108 通过。当前累计专项测试 72 项、代表性样本 74 项、定向 HTTP 合约
  34 项。
- JSN 候选器随后按 CFG 族和客户端 `.sp` 页面共同出现关系限定局部 `unit id`：剔除
  2,491 条跨页面假候选，同时保留 HSGT、SJQD、BYGTJ 等真实跨配置引用；两个专项测试、
  三项 QHTJ2 非空探测与三个定向 HTTP 合约通过。当前累计专项测试 74 项、代表性样本
  77 项、定向 HTTP 合约 37 项；本阶段未触及解码、下载协议、缓存或 schema，未重复运行
  完整 CTest。
- TPool 随后在独立 `tpool_rule.cpp` 中闭合 `nbeginday/nendday/nperiodnum` 历史窗口，
  并新增 42 项类型化 `nset=3/4` 财务/实时行情字段解释器；公式和内置字段使用各自的
  操作码表，L1/财务批量请求按证券复用。两轮 `tdx-tpool-tests`、两个真实样本和七项聚焦
  HTTP 契约通过。当前累计专项测试 76 项、代表性样本 79 项、定向 HTTP 合约 44 项；
  未修改共享解析器、传输、缓存或既有 schema，未重复运行完整 CTest。

详细证据见本日各模块化归档，汇总机器可读证据位于
`output/native-cpp-maintainability-audit.json`。
