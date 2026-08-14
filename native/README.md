# tdx-tool：纯 C++ 统一工具

`tdx-tool` 是本项目后续唯一的整合型命令行入口。运行时不嵌入 Python，
不启动 Python 解释器，也不把 `doc/90-scripts` 中的旧脚本作为后端。协议、
文件解析、索引构建、JSON/CSV 输出、压缩和哈希校验均由 C++ 直接完成。

## 当前已实现的原生子命令

| 子命令 | 能力 |
| --- | --- |
| `doctor` | 检查原生运行环境与通达信目录 |
| `recon install` | 只读盘点安装目录中的 EXE/DLL |
| `recon runtime-topology` | 只读枚举 TdxW 进程树、本地模块和 TCP 端点；默认递归子进程，支持前后快照、定时变化观察及公开 `connect.cfg` 服务器分组注释，不注入或读取进程内存 |
| `recon session-config` | 只读盘点公开服务器组和 tpbus/TaApi 会话默认值；本地私密会话字段仅输出存在性与编码长度，不输出值或哈希 |
| `recon ttplugin-redirect` | 输出 TTPlugin 六种 `RedirectData` 请求/响应翻译契约，并可把调用方 JSON 离线编码为固定旧协议体；不加载 DLL、不联网 |
| `recon api-contracts` | 巡检健康/OpenAPI、Svelte 深链接、参数拒绝、正常空关系、自定义公式计算/扫描/回测、多公式组合扫描/组合回测 POST、TCalc 390 条静态注册及解释器能力、行情/财务/板块/JSN/PBRPC/TPool、L1 SSE、扩展市场和发行包边界；POST 正文由随发行物安装的 `recon-assets` 严格目录提供，不再内嵌在 C++ 控制流中。当前目录共 224 项，可重试 HTTP 状态最多尝试三次，输出结构化报告并以退出码标记最终失败 |
| `recon cloud-variants` | 按解析后的规范请求 JSON 归并 TQLEX/PBRPC 参数变体，核对每个 ReqId 与无 ReqId 路由是否已有类型化 C++ 命令；存在通用入口独占分支时返回非零 |
| `recon jsn-variants` | 合并 XML/CFG/动态键盘点 622 个 JSN 模板，对账下载文件并审计类型化 C++ 覆盖；可导入 `jsn download --report` 生成的远端长度/MD5 清单，把已确认非空资源优先排序并排除零长度缺口 |
| `recon jsn-discovery` | 为下载目录建立 SHA-256 基线，比较资源增删改、字段结构，提取动态市场/代码键并生成字段类型画像与优先级 |
| `jsn candidates` | 沿 CFG `refunit` 主从关系和同一类型化业务域生成具体动态路径；默认只列本地缺失项，显式指定资源族后最多探测 25 项且绝不下载 |
| `formulas extract` | 从已校验的 `formula-assets` 发行快照导出四类内置公式；不读取或加载 DLL |
| `formulas icons` | 从已校验的 `formula-assets` 导出 DRAWICON PNG、BMP 与资源清单；不读取或加载 DLL |
| `tdx-formula-extractor` | 默认关闭的独立证据更新工具，才负责把已指纹识别的 `TCalc.dll` 当作惰性 PE 字节解析；仅 `TDX_BUILD_TCALC_EXTRACTOR=ON` 时构建，始终不影响 `tdx-tool` |
| `formula-assets` | 随发行物提供 379/379 条系统公式正文与 DRAWICON PNG/BMP；带 SHA-256 清单，正常服务和公式命令不读取 `TCalc.dll`，可用 `TDX_FORMULA_ASSETS` 显式指定目录 |
| `formulas calculate` | 从多周期 K 线以纯 C++ 计算 26 类 TCalc 兼容指标 |
| `formulas analyze` | 对 379 条公式做语法、依赖、未来函数和数值/展示语义审计，并统计可生成绘图 IR 的公式；同时发布 277 个支持函数、87 个自动符号、13 个调用方授权序列驱动的 L2 订单流函数、可复现 `RAND`、本地 `SAFESCORE/SHINESCORE`、有界 `CALCSTOCKINDEX`，以及 TCalc 390 条静态注册项的完整分类（319 条已识别、71 条运行时/权限边界、0 条公开非 L2 待补候选） |
| `formulas context-template` | 为授权 L2 与券商私有序列生成精确键和 K 线时间空模板；网页可编辑后直接交给内置公式 POST 执行，不下载、推导或伪造外部值 |
| `formulas audit` | 在同一组 A 股或 TDX 扩展市场真实 K 线上运行全部兼容公式，逐条报告全部 379 条公式，按市场过滤无关上下文；港股 `31/48` 会从本地 `hkcwdata.dat` 绑定 19 个已证实的 TCalc FINANCE selector（含 type-103 的 `1/7`），当前目录无类别样本的兼容市场 `71` 保留 17 个 type-105 selector，其余 selector 发布精确的 `unsupported_expansion_bindings`；同能力开放 `/api/v1/formulas/audit` |
| `formulas evaluate` | 解释执行全部 379 条内置公式正文/恢复体或 `--source-file` 自定义源码；支持参数、序列、多周期 K 线、港股 type-103/type-105 本地财务、期权 `IVOLAT`、六类筹码函数、披露日归档财务序列及 18 个无源码原生指标；支持同证券 `"指标.输出"` 及带常量算术/父公式参数的位置参数引用，未传子参数沿用默认值并拒绝逐 K 线变化参数；另含 `RAND` 的可审计重放、本地 `SAFESCORE/SHINESCORE`，以及 `CALCSTOCKINDEX(证券,指标,输出序号)` 的同周期跨证券嵌套计算；嵌套最多四层并检测直接/间接循环。GET/POST、全库审计及 CLI 均复用相同解释器和上下文边界，未来型函数仍只允许显式只读执行 |
| `formulas scan` | 并发扫描内置条件公式或 `--source-file` 自定义源码，支持本地/指定证券/全 A 股、按历史请求身份隔离的原始缓存、最近 N 根触发和逐证券复权；HTTP/Svelte POST 支持 `workers=1..16` 首轮有界抓取与 900 秒共享复权输入缓存 |
| `formulas watch` | 周期重算条件公式，按证券差分进入、退出和信号更新，使用独立 JSON 原子恢复活跃成员并输出 JSONL；显式指定时可同步导出通达信 `.blk`，不完整扫描不会制造虚假退出 |
| `formulas strategy` | 用 JSON 清单组合 1—16 条内置或自定义条件公式，严格在同一证券、日期、时间 K 线上执行 `all/any/at-least`；支持扫描，以及收盘确认、下一共同 K 线开盘调仓的固定股票池等权回测和逐股归因；CLI/HTTP/Svelte 支持逐证券 none/qfq/hfq/fixed 复权、共享输入缓存和有界首轮抓取 |
| `formulas backtest` | 回测内置专家公式或 `--source-file` 自定义 `ENTERLONG/EXITLONG` 源码，计入佣金、滑点与最大回撤；HTTP/Svelte 可用确认 POST 回测内存源码，并可显式选择 none/qfq/hfq |
| `pool inspect` | 只读解析 TPool XML、规则、证券、`psatt` 动作策略、历史保留/过期清理策略、三个 TdxW 宿主回调契约和兼容边界 |
| `pool history` | 只读解析 `tpool/<池>/<节点>/<YYYYMMDD>.dat/.log` 及原版 `_in_pool_his.txt/_status_his.txt`，支持目录筛选、跨文件证券汇总、GBK/UTF-8 自动识别和原版管道文本字节级回转；GET `/api/v1/pools/history` 仅扫描服务 root 并只返回 root-relative 路径，TPoolLab 提供同一每日历史视图 |
| `pool evaluate` | 用恢复公式源码评估 TPool 比较/交叉/拐点、三类过滤、跨证券排名、只读 flow 投影及未执行动作计划 |
| `pool watch` | 周期重算股票池，推进多节点/带环 flow；输出 JSONL 告警并可用独立 JSON 原子恢复状态 |
| `image-data decode` | 离线解压 `zst_cache` 的 24 字节 zlib 包装，恢复增量控制流、1032 字节盘口快照、十档、买一/卖一委托队列及买卖均价/总量；不加载原 DLL |
| `blocks export` | 导出多级行业树、研究行业、概念、风格、指数及成分关系 |
| `blocks query` | 查询板块下的股票，或反查股票所属板块 |
| `hyzt extract` | 生成云端行业、股票、主题双向关系模型 |
| `jsn download` | 通过 7709 `709/1721` 探测、下载并校验 JSN；`--report` 原子保存远端可用性、长度和 MD5 清单 |
| `jsn catalog` | 离线编目全部已下载 JSN 表 |
| `jsn query` | 按证券横向反查全部 JSN 的直接、引用和成员关系 |
| `minute extract` | 解析本地 `.lc1` 一分钟线 |
| `minute download` | 通过 7709 `0x052D` 分页下载并生成 LC1/JSON/CSV；按配置主站优先并在已完成页边界重连/换站 |
| `market snapshot` | 通过 `0x054C` 批量取得最新价和行情快照；默认按 `connect.cfg/[HQHOST]/PrimaryHost` 选择当前主站与两个备用站，TCP 瞬时失败时每站用新连接最多尝试三次并报告传输元数据 |
| `market depth` | 通过 `0x0547` 取得买卖五档并计算档位金额 |
| `market etf-flows` | 聚合股票与研究行业的 ETF 持股、申赎净流入和周交易资金 |
| `market fund-reference` | 纯本地读取 `specjjdata/specetfdata/speclofdata`，查询基金份额、交易参考值、已发布单位净值、ETF/LOF 跟踪标的和原生日期窗口状态 |
| `market investment` | 解密 `invest.dll` 的 `pinfo.dat/.da0`：`transactions` 分页读取 12 类流水，`holdings` 重放成本/盈亏/现金流，`valuation` 用离线快照或公开 L1 计算市值、浮盈、预计卖出费和含费保本价；CLI-only，不输出密码或保留字节 |
| `market shape-match` | 纯本地解析 `TDXDeep.dll` 的 `shapematch.dat` 模板，并按原生加权 Pearson 算法评分单票或扫描离线包、证券清单、本地板块/市场；扫描默认本地，联网须显式设置请求上限，不加载原 DLL |
| `market hot-history` | 纯本地读取 `speczshot`，查询多年 K 线热点区间、区间/峰值收益、驱动主题和完整分析文本；支持日期重叠筛选 |
| `market index-events` | 纯本地读取 `speczsevent` 双表，查询上证、恒生、纳指图的事件发生日、目标交易日、正文 recid 和原生跳转目标 |
| `market securities` | 通过 `0x044E/0x044D` 分页导出服务端股票、指数、ETF、债券和可转债目录；失败时整市场重建，避免缓存半成品 |
| `market watch` | 持久复用一条公开 7709 会话，以 `0x0547` 同时读取快照和五档，输出变化 JSONL，并在失败后切换端点、退避重连；网页可复用同一共享会话的 SSE |
| `market kline` | 按证券或本地板块 ID/代码/唯一名称读取多周期 K 线；默认下载普通 7709 或扩展市场 7727，也可用 `--source local` 从 DAY/LC1 离线生成 1/5/15/30/60 分钟及日周月；880/881 板块使用市场 1 的指数解码，普通股票可应用 `0x000F` 前后复权因子 |
| `market instruments` | 通过 7727 `0x23F0/0x23F5` 分页导出期货、港股等扩展市场合约目录，并解析记录偏移 56 的合约乘数及精确来源 |
| `market options` | 从 7727 全目录识别商品/股指期权，输出 wire code、标的、行权价、认购认沽、行权风格和定价族 |
| `market option-expiry` | 从 `code2name_qq.ini` 产品规则和 `neednote.dat` 节假日复现期权精确到期日 |
| `market option-chain` | 批量复用 7727 会话生成期权链、IV 曲面、Greeks、Put/Call 比率和最大痛点 |
| `market option-volatility` | 按 `TQQCalc.dll`/TdxW selector 35 复现历史/隐含波动率、模型价格和 Greeks，并自动解析到期日 |
| `market expansion-quote` | 通过 7727 `0x23FA` 获取扩展品种现价、五档、成交和持仓量 |
| `market expansion-timeline` | 通过 7727 `0x240B/0x240C` 获取扩展品种当日/历史分时、均价和持仓量 |
| `market expansion-trades` | 通过 7727 `0x23FC/0x2406` 获取扩展品种当日/历史逐笔、增仓和开平性质 |
| `market finance` | 通过 `0x0010` 批量取得财务与股本基础信息；瞬时 TCP 失败用新连接有界重试 |
| `market capital` | 默认通过 `0x000F` 取得除权除息、股本变化等事件；`--source local` 可直接解密 `hq_cache/gbbq`，并供本地 K 线完成全离线复权 |

`formulas evaluate` 还会从精确的 7727 合约记录自动绑定裸符号 `MULTIPLIER`；
期货/期权返回记录乘数，标准 A 股按 TCalc 宿主语义返回 0。裸符号
`ZSTJJ/QHJSJ` 则读取同一个 K 线辅助字段：分时图为累计均价，衍生品日线为结算价。
| `market limits` | 通过 `0x0452` 扫描特殊证券涨跌停价覆盖表；瞬时 TCP 失败用新连接有界重试 |
| `market funds` | 通过 PBRPC `200340→200341` 聚合市场、一级研究行业和个股七段主力资金，支持瞬时错误重试和陈旧缓存回退 |
| `market panorama` | 将十张通达信个性数据主表归一为质量、资金、风险、财务、预告、预期、分布、龙虎、两融和增减持视图，并支持单票聚合 |
| `market futures-issuance` | 聚合期货统计/关联股、IPO/债券发行人、定向增发六阶段、配股三阶段和优先股发行/股息条款 |
| `market active-funds` | 聚合基金季报股票持仓变化，并按股票展开持有基金、净值占比和重仓排名 |
| `market forecasts` | 聚合 A 股全市场最新预告、行业统计/动态详情和港股预告，并显式对账静态主表与行业动态截面差异 |
| `market foreign-alerts` | 读取当前外资持股预警清单，并按股票展开持股比例与状态历史 |
| `market institution` | 聚合单票机构/十大流通股东，按股东展开跨股票和单票报告期历史，并可原子持久化或从连续分页 API 快照迁移完整缓存 |
| `market institution-analysis` | 聚合十七张 CGFX 与基金独门、汇金证金、社保汇总、国开/梧桐树/中科汇通持股、举牌等八张专表，支持全市场或单票二十五视图聚合 |
| `market institution-lhb` | 聚合四个滚动周期机构席位买卖排行，并按股票展开异动日榜单总额 |
| `market ratings` | 聚合港股、美股评级/目标价与一级研究行业看多看空，并展开动态历史 |
| `market lhb` | 聚合 8 张龙虎榜分析视图，并按股票或事件展开营业部买卖席位 |
| `market valuation` | 聚合市场指数 PE/PB、历史百分位和关联指数基金 |
| `market relative-valuation` | 比较目标指数与基准指数的 PE/PB/PS 估值比、历史分位和每日走势 |
| `market equity-valuation` | 获取个股/行业 PE 历史、预期估值及 PB-ROE 回归带五类模型 |
| `market flow-followup` | 获取两融/北向逐日历史及四组分档模型，并隔离北向占位段 |
| `market consensus` | 聚合九类一致预期、评级、增长和价格阶段榜单，并按股票展开目标价、三年预测和研报正文 |
| `market research` | 聚合九类机构调研/互动/监管视图，并展开单票正文、行业股票和知名机构覆盖 |
| `market roadshows` | 通过无 ReqId 的 TQLEX KV 路由获取全市场或单票路演、业绩说明会与上市仪式，支持筛选、分页、重试和陈旧缓存 |
| `market industry-profile` | 合并三级研究行业、四类报告期机构持仓、年度股东结构和行业内逐股画像 |
| `market unlocks` | 聚合近期解禁日历、同事件锁定批次，并展开具体解禁股东 |
| `market block-trades` | 聚合大宗成交、意向申报、四周期营业部排行和月度行业成交，并展开动态明细 |
| `market block-backtest` | 按区间比较五类板块，并展开服务端选定成分股的收益、回撤、成交与资金表现 |
| `market repurchases` | 聚合 A 股回购方案、月度/年度统计，并展开港股逐交易日回购历史 |
| `market tender-offers` | 获取全市场要约收购进度、价格、拟定与实际股数/比例/资金、期限、过户、退市标记、目的及单票历史 |
| `market ownership` | 聚合股东/董监高变动、六类全市场增减持榜、五市场股东人数、承诺不减持、质押风险、统计和质押机构，并展开单票/机构历史 |
| `market ranking` | 通过 `0x054B` 获取涨速、涨幅、成交额、封单额和开盘抢筹榜；失败时从首排行页重建 |
| `market limit-quality` | 聚合封板榜、五档、统计与竞价，复核涨停和封单质量 |
| `market limit-review` | 聚合通达信非实时涨跌停原因、涨停基因、年度行为、市场历史、指定日成员和单票原因历史 |
| `market session-turnover` | 获取 A 股或 ETF 统计日总成交、开盘成交、盘后成交及占比，支持七种排序和单票过滤 |
| `market block-rotation` | 获取行业、概念、地区、风格板块上次异动、平均周期以及周/月/季/年涨跌异动数与区间涨幅 |
| `market limit-ladder` | 获取研究行业与概念板块封板、炸板、昨板、连板、最高高度、总高度与晋级率，并关联本地成分 |
| `market threshold-stocks` | 获取百元股/千亿市值历史家数、指定日名单、入围跌出与家数趋势，并支持单票反查 |
| `market capital-strength` | 获取 5日/10日/20日/30日/近3月 DDX 前100榜、跨周期共振和单票命中；保留流通股本、周期涨幅及总/主力净流入元口径 |
| `market strong-stocks` | 获取历史强势股连板生命周期、单票多次入选及指定区间逐日涨停原因、成交额和市场涨跌停温度 |
| `market commodity-links` | 获取商品报价与多周期涨跌、关联股票/行业/ETF，以及涨价题材、历史驱动、事件股票和单票反查 |
| `market announcement-signals` | 获取公告精选、风险提示、公告 PDF、近期涨幅与单票公告前后 3 日表现历史 |
| `market reverse-repo` | 合并国债逆回购当日交收日历与公开 L1 年化利率，计算指定本金的毛/净收益、手续费和净年化 |
| `market exchange-supervision` | 获取当前/历史交易所监管观察期、起止价表现、公开 L1 当前表现及异动公告 PDF |
| `market active-lhb` | 获取近 5 日/一月/半年活跃龙虎榜聚合排行及单票逐次异动原因、换手率和买卖额 |
| `market state-owned-reform` | 获取国企改革行业/地区/整合预期/公司系分组、成员控制人/控股比例/阶段表现及重组预期 |
| `market economic-indicators` | 获取 72 项经济/商品指标当前值、环比同比、历史序列和关联股票，可选合并公开 L1 行情 |
| `market strategic-themes` | 获取主题投资 26 个大类、598 个主题、成分股和逐股入选逻辑，并支持按股票反查主题 |
| `market theme-library` | 获取五类主题快照、927 个去重主题、逐股纳入原因和主题指数历史；同 ID 跨来源快照不错误合并为层级 |
| `market margin` | 聚合市场及个券融资融券历史和排行，并提供转融资/转融券总量历史；金额保持元、数量保持股、上游空值保持 `null` |
| `market stock-connect` | 聚合陆港通资金/持仓/单票历史、七类增减仓、北向分类、南向港股行业及指定日期活跃股，并区分历史快照和配置零字节正常空表 |
| `market intelligence` | 聚合关注度、八类价值关注及动态股票明细、300 只安全亮点排行、失信被执行事实、风险、事件、部委要闻和热点股票关系，并支持单票统一反查 |
| `market technical-signals` | 获取三十二类技术/模型/竞价/盘中因子选股、按单票反查命中视图，并维护新增/消失/变化快照 |
| `market factors` | 获取普通/形态因子目录、完整关系矩阵、单票双类反查、广度/共现、双视图对账和原子快照差异 |
| `market abnormal-moves` | 获取十九类交易异常/停牌核查证券，支持六个板块分支和最新交易日回退 |
| `market abnormal-details` | 获取异常列表刷新摘要，并结构化单证券当前上榜原因、区间和累计量额 |
| `market anomaly-risk` | 获取个股相对分类指数的异常波动统计及异动停牌触发风险 |
| `market profit-gaps` | 获取业绩披露后的利润断层、披露类型和安全分，并支持单票过滤 |
| `market financial-screen` | 获取五板财务快照或 33 只小盘成长母集；后者保留四期同比、三年复合增速及创业板/科创板精确子集对账 |
| `market shareholder-signals` | 聚合牛散、机构增持/股东收缩、小市值专业机构增持、成长调研和知名自然人持仓，并支持单票反查 |
| `market financial-insights` | 获取十五类客户端财务筛选，包含稳健成长、连续质量增长、利润突破和当前分红方案；按 CFG 输出基础单位、派生公式、筛选条件和单票反查 |
| `market index-volatility` | 获取指数滚动已实现波动率目录、历史序列、均值和分位 |
| `market total-return-gap` | 比较价格指数与对应全收益指数的区间回报差 |
| `market fund-analytics` | 获取基金风险/月度/择时选股、公开报告期行业/股票持仓、稳定性与仓位估算十三类视图 |
| `market auction` | 通过 `0x056A` 获取个股开盘/收盘集合竞价逐点序列；仅在单证券完整解析后推进游标 |
| `market trades` | 通过 `0x0FC5/0x0FC6` 获取当日/历史 L1 成交明细与分钟聚合；仅在单证券全部页完成后推进游标 |
| `market stats` | 通过 `0x06B9` 下载并解析估值、流通股本、封单、竞价和涨停统计；断连后从 ZIP 首块重建，缓存不接收半包 |
| `market professional` | 下载、MD5 校验并解析官方 `gpcw/gp` 单期/多季度专业财务、个股交易、板块交易和市场交易序列 |
| `market convertible-bonds` | 以 `pricing|listed|subscriptions|pending` 视图区分公开 L1 + 披露现金流复算、已上市条款/触发历史、发行申购与待发方案；同时对账待发双投影、“新可转债”筛选及可交换债第二投影，保留上游冲突 |
| `market bond-reference` | 查询 42,479 条全市场债券总表，或按九个评级桶、六个利率桶及八个细分类别懒加载；`--include-projections` 核对分类主表、沪深投影和客户端合并表，保留私募债/资产支持证券小表边界及未声明尺度 |
| `market calendar` | 聚合 28 张宏观、热点会议、公司事件、配股四阶段、新股/美股 IPO 与期货日历；`rights-issues` 可单独查询股权登记、缴款开始/结束和除权基准日 |
| `market employees` | 查询全市场员工、研发、人均指标、单票高管职务薪酬及员工持股计划 |
| `market hk-events` | 查询港股分红、权益披露、每日沽空统计和上市申请 |
| `market hk-short-history` | 读取 7727 港股日 K 历史沽空股数、MA5/MA20 和成交股数占比，并与 GGRL104 逐日对账 |
| `level2 build` | 离线构造内置初始 `1363/1373` 与默认 `1364/1374` 的 26 字节请求、TdxW 内部 IPC `1369/1371` 的 40/48 字节请求和 SDK redirect `4653/4655/4680` 的 40/46/37 字节请求体，把 SDK `1807` 的七字节证券项转换为明确标注 `needs-resolution` 的类型化订阅计划，并可用 `fasthq-subscribe-plan` 复现 `FastHQ.Subscribe` 的八个逻辑任务字段及其 envelope/body 来源；`sdk-fnreqdata-plan` 覆盖 `1801/1802/1803/1804/18071` 的 12 槽 ABI 与 25 字节本地关联布局，独立的 `sdk-fnreqdata-18031-plan` 恢复 side→selector、f32→double ABI、固定 cursor/count/mode 及 host-global 关联；`sdk-fnsubscribe-batch-plan` 对 1..100 个已解析大写 A 股身份恢复六槽调用及 mode-9 retained 25B 模板，并已接入确认 POST 与 Level2Lab；`sdk-callback-route-plan` 为 `1801/1802/1803/1804/1807/18031/18071` 发布消息 `0x54E/0x54F/0x551/0x54D/0x8B9/0x91E` 的同步/异步候选及 25B 或 host-global correlation，1807 可保留自动/false/true 三态；所有逻辑计划都不构造 wire、不调用 SDK、不投递消息、不发送请求或订阅 |
| `level2 decode` | 离线解析合法捕获的逐笔、千档、队列、行情更新、tpbus、TCalc、SDK 回调关联、SDK JSON 与 PB wire 数据；`sdk-json-4653/4655/4671/4680` 分别归一化证据命名的分时记录、逐笔成交、买一/卖一委托队列和五/十档快照；`sdk-1801/sdk-1802` 解析计数式 52/40 字节逐笔记录，其余固定 SDK 回调严格只接受单个完整 body；`sdk-callback-invocation` 贯通 domain/CLI/确认 POST 与 Level2Lab，精确校验 `1801=arg5×52`、`1802=arg5×40`、`1803/18031/1804/1807/18071=32016/20012/432/380/380 B`；1801/1802 的 arg5 分别限 `1..7561/1..9830`，其余保留完整 i32，arg6/mode 限 u32，且 18031 不使用 mode；结果复用 decoder 与 route plan，不回显原始输入，也不调用 SDK、投递消息、联网或发送 wire；`sdk-correlation` 解析 25 字节进程内关联表而非网络/会话/Token；`tcalc-order-flow` 输出 53 个公式绑定并可物化为公式上下文，`tcalc-order-side` 输出 `ISBUYORDER` 标量 |
| `level2 project` | 纯离线投影显式输入的宿主状态；`sdk-quote-transition` 处理 1807/18071 行情状态，CLI-only `sdk-correlation-transition` 严格解析 JSON/UTF-8/u32，并按 TdxW 的 25 B callback correlation 首匹配、消费/保留与 upsert 规则产生下一 registry；`sdk-1801-host-projection`/`sdk-1802-host-projection` 把非空 `count*52`/`count*40` 回调记录投影为每条 20 B host record，`sdk-1804-host-projection` 把精确 432 B 回调体投影为 105 个 f32 槽/420 B，三者均共用 CLI、确认 POST 与 Level2Lab；不加载 SDK、不执行 callback、不查宿主证券、不投递消息、不联网或绕过权限 |
| `level2 session` | 只读核对 TdxW/tpbus 进程、模块与版本，不附加、不订阅、不取票据 |
| `cloud workflow` | 自动执行 TQLEX/PBRPC 主表、字段映射及明细请求 |
| `cloud pbrpc` | 发现并执行 `reqformat=22` protobuf RPC，完成 RpcID 握手与分段拼接；瞬时失败时从 RpcID=0 重开完整会话 |
| `cloud tqlex` | 发现并执行 `reqformat=2` TQLEX JSON 模板，支持参数替换与多结果集分页；瞬时失败时从第一页重建完整查询 |
| `cloud routes` | 盘点 `reqformat=1` 旧服务名、跨格式同名入口和 TPData 宿主位数边界，不发请求 |
| `serve` | 启动 Svelte 工作台和纯 C++ 本地 JSON API |

命令表只注册已经真正实现的能力；未迁移模块不会以空壳或 Python 转发的
形式出现在帮助中。

## 常用示例

```powershell
build\native-mingw\tdx-tool.exe formulas extract `
  --kind all --format json `
  --output output\tdx-formulas-native.json

build\native-mingw\tdx-tool.exe formulas icons `
  --output output\tcalc-drawicon.png `
  --bmp-output output\tcalc-drawicon.bmp `
  --manifest output\tcalc-drawicon.json

# 仅在更新证据快照时构建并显式运行；主程序不需要它
build\native-mingw\tdx-formula-extractor.exe extract `
  --root C:\new_tdx --kind all --format json `
  --output output\tdx-formulas-from-dll.json

build\native-mingw\tdx-tool.exe formulas calculate `
  --market sz --code 000001 --formula MACD --period day --pages 2

build\native-mingw\tdx-tool.exe formulas analyze `
  --input output\tdx-formulas-native.json `
  --output output\tdx-formula-coverage.json

build\native-mingw\tdx-tool.exe formulas audit `
  --library output\tdx-formulas-native.json `
  --input output\tdx-market-kline.json --with-context `
  --output output\tdx-formula-runtime-audit.json

# 按真实 K 线的全部 DATE|TIME 生成授权外部序列空模板
build\native-mingw\tdx-tool.exe formulas context-template `
  --library output\tdx-formulas-native.json --formula ZJLX `
  --market sz --code 000001 --period day --pages 1 --page-size 20 `
  --output output\zjlx-context-template.json

build\native-mingw\tdx-tool.exe formulas evaluate `
  --market sz --code 000001 --formula UDL --period day --pages 2

# 先增量保存正式报告的实际披露日，再启用无当前值回退的严格财务时点
build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --view all --archive

build\native-mingw\tdx-tool.exe formulas evaluate `
  --root C:\new_tdx --market sz --code 300503 --period day `
  --source-file output\probes\point-in-time-finance.tdx `
  --formula PITFIN --point-in-time-finance

build\native-mingw\tdx-tool.exe formulas scan `
  --formula "BIAS卖出" --all --period day --workers 4 `
  --cache-dir output\formula-kline-cache --output output\bias-scan.json

build\native-mingw\tdx-tool.exe formulas scan `
  --source-file output\probes\custom-selection.tdx --formula CUSTOM_SELECT `
  --param N=5 --securities sz000001,sh600000 --lookback 10 --period day

build\native-mingw\tdx-tool.exe formulas watch `
  --source-file output\probes\custom-selection.tdx --formula CUSTOM_SELECT `
  --param N=5 --securities sz000001,sh600000 --lookback 10 --period day `
  --interval-seconds 60 --state-file output\formula-watch-state.json `
  --block-output output\formula-watch.blk --output output\formula-watch.jsonl

build\native-mingw\tdx-tool.exe formulas strategy `
  --strategy output\probes\custom-strategy.json --mode scan `
  --securities sz000001,sh600000 --period day --lookback 10

build\native-mingw\tdx-tool.exe formulas strategy `
  --strategy output\probes\custom-strategy.json --mode backtest `
  --securities sz000001,sh600000 --period day --pages 2 `
  --initial-capital 100000 --commission-bps 2.5 --slippage-bps 1 `
  --output output\probes\formula-strategy-backtest.json

build\native-mingw\tdx-tool.exe formulas backtest `
  --formula MA --formula-kind expert --market sz --code 000001 `
  --period day --pages 5 --output output\ma-expert-backtest.json

build\native-mingw\tdx-tool.exe formulas backtest `
  --source-file output\probes\custom-expert.tdx --formula CUSTOM_EXPERT `
  --param N=10 --market sz --code 000001 --period day --pages 1

build\native-mingw\tdx-tool.exe pool inspect --root C:\new_tdx

build\native-mingw\tdx-tool.exe pool history `
  --root C:\new_tdx --kind all --from 20260101 --limit 1000 `
  --output output\tpool-history.json

build\native-mingw\tdx-tool.exe pool history `
  --input C:\path\20260812.dat --name-root C:\new_tdx `
  --native-text-output output\pool_status_his.txt `
  --output output\pool-status-export.json

build\native-mingw\tdx-tool.exe pool evaluate `
  --input output\tpool-native-fixture.xml --pages 2 --limit 20

build\native-mingw\tdx-tool.exe pool watch `
  --input output\tpool-native-fixture.xml --interval-seconds 60 `
  --state-file output\tpool-state.json --output output\tpool-alerts.jsonl

build\native-mingw\tdx-tool.exe image-data decode `
  --input C:\new_tdx\T0002\zst_cache\<缓存文件>.img `
  --input-format auto --record-output output\image-data-records.bin `
  --output output\image-data.json

build\native-mingw\tdx-tool.exe market watch `
  --root C:\new_tdx --security sz:000001 --security sh:600521 `
  --interval-ms 1000 --output output\market-watch.jsonl

build\native-mingw\tdx-tool.exe cloud routes --root C:\new_tdx

build\native-mingw\tdx-tool.exe market futures-issuance `
  --root C:\new_tdx --contract-key 29PPL8 `
  --year 2026 --industry-key 2026881015

build\native-mingw\tdx-tool.exe market futures-issuance `
  --root C:\new_tdx --section placements `
  --placement-status registered --q 工业机械

build\native-mingw\tdx-tool.exe level2 build `
  --kind transaction --market 1 --code 600000 --cursor 0 --count 1500

build\native-mingw\tdx-tool.exe level2 session --pid 4408

build\native-mingw\tdx-tool.exe blocks query `
  --root C:\new_tdx --block 银行 --family industry

build\native-mingw\tdx-tool.exe blocks query `
  --root C:\new_tdx --code 000001 --market sz

build\native-mingw\tdx-tool.exe jsn download `
  --resource func_gx_hyzt101_1.jsn --download `
  --output-dir output\tdx-jsn-native

build\native-mingw\tdx-tool.exe minute download `
  --code 000001 --market sz --kind stock --pages 1 `
  --output output\tdx-000001-1m.json

build\native-mingw\tdx-tool.exe market kline `
  --security 47:IFL9 --period 30m --pages 2 --page-size 800 `
  --output output\IFL9-30m.json

# 同名板块应使用 blocks query 返回的明确 block_id
build\native-mingw\tdx-tool.exe market kline `
  --root C:\new_tdx --block industry:T1001 `
  --period 1m --pages 2 --page-size 800 --date all `
  --output output\bank-industry-1m.json

# 不联网：从本机 DAY/LC1 缓存生成多周期 K 线
build\native-mingw\tdx-tool.exe market kline `
  --root C:\new_tdx --security sz:000001 --source local `
  --period 30m --pages 2 --page-size 80 `
  --output output\local-000001-30m.json

build\native-mingw\tdx-tool.exe market instruments `
  --count 1000 --market 47 --query IF `
  --output output\tdx-instruments-47-if.json

build\native-mingw\tdx-tool.exe market options `
  --market DCE --contract A2609 --type call --limit 100 `
  --output output\tdx-options-A2609.json

# 单独核对本地产品规则推导的精确到期日
build\native-mingw\tdx-tool.exe market option-expiry `
  --root C:\new_tdx --security '5:A 8X06SH' --name A2609-C-4400

build\native-mingw\tdx-tool.exe market option-chain `
  --root C:\new_tdx --market DCE --contract A2609 --lookback 60 `
  --output output\option-chain-A2609.json

# --name 可跳过首次全目录查找；默认从本地产品规则自动解析到期日
build\native-mingw\tdx-tool.exe market option-volatility `
  --root C:\new_tdx --security '5:A 8X06SH' `
  --name A2609-C-4400 --lookback 60 `
  --output output\A2609-C-4400-volatility.json

build\native-mingw\tdx-tool.exe formulas evaluate `
  --root C:\new_tdx --formula VOLATILITY --market 5 --code 'A 8X06SH' `
  --option-name A2609-C-4400 --period day --param N=60

build\native-mingw\tdx-tool.exe market expansion-quote `
  --security 29:A2609 --output output\A2609-quote.json

build\native-mingw\tdx-tool.exe market expansion-timeline `
  --security 29:A2609 --date 20260805 `
  --output output\A2609-timeline.json

build\native-mingw\tdx-tool.exe market expansion-trades `
  --security 29:A2609 --date 20260805 --page-size 1800 --pages 2 `
  --output output\A2609-trades.json

build\native-mingw\tdx-tool.exe formulas evaluate `
  --root C:\new_tdx --formula CCL --market 47 --code IFL9 --period day

build\native-mingw\tdx-tool.exe market depth `
  --root C:\new_tdx --security sz:000001 --security sh:600000

build\native-mingw\tdx-tool.exe market funds `
  --root C:\new_tdx --market sz --code 000001

build\native-mingw\tdx-tool.exe market funds `
  --root C:\new_tdx --industry 881385

build\native-mingw\tdx-tool.exe market funds `
  --root C:\new_tdx --all-industries

build\native-mingw\tdx-tool.exe market institution `
  --root C:\new_tdx --market sh --code 603221

build\native-mingw\tdx-tool.exe market institution `
  --holder-id QF000034 --holder-name 高盛公司有限责任公司 `
  --reference-code 603221 --stock-code 603221 --limit 50 `
  --cache-root output\tdx-runtime-cache\institution

build\native-mingw\tdx-tool.exe market lhb `
  --root C:\new_tdx --market sh --code 688066

build\native-mingw\tdx-tool.exe market lhb `
  --root C:\new_tdx --event 3726245

build\native-mingw\tdx-tool.exe market valuation `
  --root C:\new_tdx --market sh --code 000001

build\native-mingw\tdx-tool.exe market relative-valuation `
  --root C:\new_tdx --code 000026 `
  --index-type broad --benchmark 000001 --method pe-ttm

build\native-mingw\tdx-tool.exe market relative-valuation `
  --root C:\new_tdx --index-type industry --method pb-mrq

build\native-mingw\tdx-tool.exe market equity-valuation `
  --root C:\new_tdx --view pe-security-history `
  --market sz --code 000001 --start 20230806 --end 20260805

build\native-mingw\tdx-tool.exe market equity-valuation `
  --root C:\new_tdx --view pe-industry-members `
  --industry 881001 --required-return-rate 3

build\native-mingw\tdx-tool.exe market equity-valuation `
  --root C:\new_tdx --view pb-roe-members --industry 881001

build\native-mingw\tdx-tool.exe market flow-followup `
  --root C:\new_tdx --view margin

build\native-mingw\tdx-tool.exe market flow-followup `
  --root C:\new_tdx --view northbound --available-only

build\native-mingw\tdx-tool.exe market flow-followup `
  --root C:\new_tdx --view financing-model

build\native-mingw\tdx-tool.exe market flow-followup `
  --root C:\new_tdx --view northbound-purchase-model

build\native-mingw\tdx-tool.exe market consensus `
  --root C:\new_tdx --market sz --code 000001

build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --view schedule --market sz --code 000001

build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --view all --report-period 20260630 --status disclosed

# 公共窗口只有当前季/近一月；定期运行会把实际披露日幂等累积到 TDX 侧车目录
build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --view all --archive

# 预览并批量回补自选股公告；任务限速且可从默认状态文件续传
build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --backfill-announcements --backfill-watchlist --dry-run

build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --backfill-announcements --backfill-watchlist --archive

# 精确父级板块会递归收集子板块成员，也可重复传 --security/--backfill-input
build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --backfill-announcements `
  --backfill-block research-industry:X50 --archive --max-securities 500

# 离线审计最新四个报告期；顶层 items 是可直接回灌的到期缺口队列
build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --audit-coverage --backfill-watchlist

# 显式日期可重放预约日尚未来临或已经过期的状态
build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --audit-coverage --backfill-block research-industry:X50 `
  --audit-as-of 20260901

# 自动读取 base.dbf 上市日，并完成刷新、审计、到期回补和再次审计
build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --maintain --backfill-watchlist

# 仍缺失的已完成项默认 24 小时后才重查，避免重复请求
build\native-mingw\tdx-tool.exe market disclosures `
  --root C:\new_tdx --maintain --backfill-block research-industry:X50 `
  --maintenance-refresh-hours 24

build\native-mingw\tdx-tool.exe market research `
  --root C:\new_tdx --market sz --code 002303

build\native-mingw\tdx-tool.exe market research `
  --root C:\new_tdx --category industry --detail-id 1880400

build\native-mingw\tdx-tool.exe market industry-profile `
  --root C:\new_tdx --industry 881385

build\native-mingw\tdx-tool.exe market industry-profile `
  --root C:\new_tdx --market sz --code 000001

build\native-mingw\tdx-tool.exe market unlocks `
  --root C:\new_tdx --market bj --code 920593

build\native-mingw\tdx-tool.exe market unlocks `
  --root C:\new_tdx --detail-id 20260805920593

build\native-mingw\tdx-tool.exe market block-trades `
  --root C:\new_tdx --view trades --market bj --code 920078 --details

build\native-mingw\tdx-tool.exe market block-trades `
  --root C:\new_tdx --view brokers --period 1y --broker-id 3722844b

build\native-mingw\tdx-tool.exe market block-trades `
  --root C:\new_tdx --view industries --month 2026-08 --industry 880489

build\native-mingw\tdx-tool.exe market ranking `
  --root C:\new_tdx --sort rise-speed --count 50

build\native-mingw\tdx-tool.exe market technical-signals `
  --root C:\new_tdx --view nine-turn --direction up

build\native-mingw\tdx-tool.exe market technical-signals `
  --root C:\new_tdx --view rps-stock

build\native-mingw\tdx-tool.exe market technical-signals `
  --root C:\new_tdx --view high-liquidity-enhance

build\native-mingw\tdx-tool.exe market technical-signals `
  --root C:\new_tdx --view trend-up --board main

build\native-mingw\tdx-tool.exe market factors `
  --root C:\new_tdx --view catalog

# 默认自动翻页，返回该因子下的完整股票列表；--first-page 可复现客户端首屏
build\native-mingw\tdx-tool.exe market factors `
  --root C:\new_tdx --view members --factor-id 24 --quotes

build\native-mingw\tdx-tool.exe market factors `
  --root C:\new_tdx --view intraday-radar

build\native-mingw\tdx-tool.exe market factors `
  --root C:\new_tdx --view security --market sz --code 000001 --quotes

# 显式构建 57 个形态因子的完整正向关系，再为单票反向查找
build\native-mingw\tdx-tool.exe market factors `
  --root C:\new_tdx --view security --market sz --code 000001 `
  --include-patterns --quotes

build\native-mingw\tdx-tool.exe market factors `
  --root C:\new_tdx --view standard-matrix

build\native-mingw\tdx-tool.exe market factors `
  --root C:\new_tdx --view pattern-matrix

# 同时统计 200646 广度/共现，并与每个因子的完整 200636 成员对账
build\native-mingw\tdx-tool.exe market factors `
  --root C:\new_tdx --view overview

build\native-mingw\tdx-tool.exe market factors `
  --root C:\new_tdx --view intraday-radar `
  --snapshot output\state\factor-radar.json

# 默认复现客户端安全分不低于 60；设为 0 可审计未过滤上游集合
build\native-mingw\tdx-tool.exe market profit-gaps `
  --root C:\new_tdx --minimum-safety 60

build\native-mingw\tdx-tool.exe market profit-gaps `
  --root C:\new_tdx --market sh --code 600521

build\native-mingw\tdx-tool.exe market index-volatility `
  --root C:\new_tdx --view catalog --window 5

build\native-mingw\tdx-tool.exe market index-volatility `
  --root C:\new_tdx --view security --market sh --code 999999 `
  --start 2024-08-06 --end 2026-08-06 --window 5

build\native-mingw\tdx-tool.exe market total-return-gap `
  --root C:\new_tdx --year 2026 --month 0

build\native-mingw\tdx-tool.exe market fund-analytics `
  --root C:\new_tdx --view risk

build\native-mingw\tdx-tool.exe market fund-analytics `
  --root C:\new_tdx --view risk-history --fund-code 000711

build\native-mingw\tdx-tool.exe market fund-analytics `
  --root C:\new_tdx --view holding-industries --fund-code 000711

build\native-mingw\tdx-tool.exe market fund-analytics `
  --root C:\new_tdx --view reported-holding-securities --fund-code 000326

build\native-mingw\tdx-tool.exe market fund-analytics `
  --root C:\new_tdx --view reported-holding-industries --fund-code 000326 `
  --report-date 20250630

build\native-mingw\tdx-tool.exe market fund-analytics `
  --root C:\new_tdx --view position-estimates

build\native-mingw\tdx-tool.exe market block-backtest `
  --root C:\new_tdx --category industry `
  --begin 2026-07-01 --end 2026-08-05

build\native-mingw\tdx-tool.exe market block-backtest `
  --root C:\new_tdx --block-code 880471 `
  --begin 2026-07-01 --end 2026-08-05

build\native-mingw\tdx-tool.exe market technical-signals `
  --root C:\new_tdx --market sz --code 000001

build\native-mingw\tdx-tool.exe market technical-signals `
  --root C:\new_tdx --view nine-turn `
  --snapshot output\state\technical-signals-nine-turn.json

build\native-mingw\tdx-tool.exe market auction `
  --root C:\new_tdx --security sz:000001 --limit 500

build\native-mingw\tdx-tool.exe market trades `
  --root C:\new_tdx --security sz:000001

build\native-mingw\tdx-tool.exe market trades `
  --root C:\new_tdx --security sz:000001 --date 20260803

build\native-mingw\tdx-tool.exe market stats `
  --root C:\new_tdx --security sz:000001

build\native-mingw\tdx-tool.exe market stats `
  --root C:\new_tdx --local --security sz:000001

build\native-mingw\tdx-tool.exe market professional `
  --kind stock --security sz:000001 --field 3 --field 6 `
  --history --from 2026-07-01

build\native-mingw\tdx-tool.exe market professional `
  --kind board --security 880201 --field 5 --field 6 --history

build\native-mingw\tdx-tool.exe market professional `
  --kind finance --security sz:000001 --period 20260331 `
  --field 0 --field 271 --field 299 --field 307 --field 308 --field 319

build\native-mingw\tdx-tool.exe market professional `
  --kind finance-series --security sz:000001 --field 271 --field 308 `
  --from 20250101 --limit 8

build\native-mingw\tdx-tool.exe cloud tqlex `
  --root C:\new_tdx --list

build\native-mingw\tdx-tool.exe cloud tqlex `
  --root C:\new_tdx --req-id 200626 --attempts 3

build\native-mingw\tdx-tool.exe cloud tqlex `
  --root C:\new_tdx --req-id 500050 --all-pages --page-size 100 `
  --set style_details=005001 --set fund_size=0 `
  --set fund_setup_time=0 --set report_date=20250630

build\native-mingw\tdx-tool.exe cloud pbrpc `
  --root C:\new_tdx --list

build\native-mingw\tdx-tool.exe cloud pbrpc `
  --root C:\new_tdx --req-id 200340 --source-file sszjtj.xml --attempts 3

build\native-mingw\tdx-tool.exe cloud pbrpc `
  --root C:\new_tdx --req-id 200341 `
  --set code=881001 --set market=1

build\native-mingw\tdx-tool.exe cloud workflow `
  --root C:\new_tdx --list

build\native-mingw\tdx-tool.exe cloud workflow `
  --root C:\new_tdx --workflow index-valuation

build\native-mingw\tdx-tool.exe cloud workflow `
  --root C:\new_tdx --workflow fund-holdings --select 000326 `
  --page-size 100 --set report_date=20250630

build\native-mingw\tdx-tool.exe recon cloud-variants `
  --root C:\new_tdx --output output\tdx-cloud-variants.json

build\native-mingw\tdx-tool.exe market roadshows `
  --root C:\new_tdx --market sz --code 000001 --limit 20

build\native-mingw\tdx-tool.exe recon api-contracts `
  --profile full --output output\api-contracts.json

build\native-mingw\tdx-tool.exe serve --root C:\new_tdx
```

Windows 入口使用 Unicode `wmain`，因此板块中文名可以直接作为参数。

`serve` 默认只监听 `127.0.0.1:8765`。浏览器打开
`http://127.0.0.1:8765/` 后，可以使用个股工作台统一查看实时行情、五档、
基于 TradingView Lightweight Charts 的分时、1/5/15/30/60 分及日/周/月
K 线、本地板块和 JSN/F10 关系；网页还提供板块双向
查询、全市场实时榜单、TCalc 指标、JSN 探测/下载/编目和安装组件盘点。
个股工作台通过
`GET /api/v1/market/stream?market=sz&code=000001` 建立本地 SSE；服务端把多个
浏览器的证券集合去重后，用一条持久 7709 会话批量读取 `0x0547`。状态可从
`GET /api/v1/market/stream/status` 检查。交易时段默认 1 秒、盘后 15 秒；
上游仍是公开 L1 轮询，并非需要登录会话的 `FastHQ.Subscribe`。
“云查询”页会从 `T0002/cloud_cfg` 盘点当前安装中的 TQLEX 模板，显示
ReqId、Entry、占位参数和原始请求体；用户可填入 JSON 参数、覆盖请求字段、
自动拉取全部分页，并把任意 `ColDes/Content` 结果集转换为横向表格。
“策略云查询”页对应 `reqformat=22`：显示模块 DLL 与模板参数，由 C++ 编码
protobuf 外层、完成 RpcID/StartPos 分段传输，再将业务 JSON 展开为表格。
固定 API 共用的 7709 JSN 读取器会以新连接有界重试三次，成功的完整批次还会
写入最多 512 项、按紧凑序列化载荷计 256 MiB 的进程内共享缓存。来源中的 `attempts/stale/age_seconds`
可判断透明恢复或陈旧降级；机构调研、一致预期、行业画像、股权变动、股份回购、
主动基金、机构龙虎、评级、外资预警、限售解禁、大宗交易和龙虎榜均返回聚合的
`cache.upstream`，冷缓存传输故障固定为可重试 HTTP 503。
限售解禁的同一 `YYYYMMDD+证券代码` 详情键若包含多个状态或原因，响应会在
`lots[]` 保留逐批次值，并以 `progresses/reasons`、`mixed_progress` 和
`mixed_reason` 暴露事件级混合状态，不再因合法批次差异中止整个主表查询。
公开 7709 行情/财务的一次性读取也会只对 DNS、建连、发送、接收和对端提前断开
执行最多三次的新连接重试；解码、命令和业务错误不会重试。`0x054C/0x053E`、
`0x0010/0x000F/0x0452`，以及竞价、逐笔、排行、统计、证券目录和普通 K 线
响应的传输元数据会报告连接次数、瞬时重试次数和
是否恢复；个股四项估值在 `valuation.upstream_transport` 分别保留行情与财务来源。
默认服务器来自安装目录公开的 `connect.cfg/[HQHOST]`：`PrimaryHost` 首选，随后
按序循环取两个备用节点；当前安装 43 个节点中首选为 `123.60.84.66:7709`。
显式 `--host` 覆盖该选择，文件缺失/损坏才使用编译期公共回退节点。
未显式传入 `--root` 的内部公式、期权和事件组合读取会先安全自动发现本机安装目录，
因此同样继承当前 HQHOST；无法发现安装时才使用编译期回退。
“关联工作流”页进一步把基金持仓/风险/月度波动/区间持仓、指数估值和
龙虎榜六条主表—明细链固化为一次调用，支持三种 JSON/PBRPC 协议组合。
独立“市场估值”页直接复现客户端的 JSN 估值页，可切换 11 个市场指数、
PE/PB 和历史百分位，并列出关联指数基金；完整历史一次载入后可缩放平移。
“估值与指数研究”页继续接入 TQLEX/PBRPC 的相对估值、个股/行业 PE 与 PB-ROE、
指数已实现波动率和价格指数—全收益指数区间差；目录可下钻历史或成分股，时间序列同样
支持缩放平移。
“一致预期”页对应客户端 `YZYQ` 九类榜单，支持按股票/行业筛选并点选展开
近六月机构研报；个股“更多”侧栏也提供同一份三年预测、目标价区间和正文。
个股“更多”抽屉还会按需加载开盘/收盘集合竞价的虚拟价格、匹配量与买卖
未匹配量；“成交明细”页签支持当日自动更新和指定历史交易日，并展示分钟
成交图、正式开/收盘竞价和最近记录。个股概览还会按需加载 `0x06B9`
在线统计，显示 Beta、PE、自由流通股本、封单/竞价/涨停口径，并计算
封流比、封昨比和封单衰减；服务端缓存五分钟。“分时资金”页签会从本地
研究行业树把股票直接映射到一级 `881xxx`，仅请求相应行业的七段资金明细，
停留时每 15 秒检查更新。“龙虎榜”页签把 8 张分析视图按事件 ID 聚合，
并继续读取动态营业部买卖席位；同股同日的多个上榜原因不会错误合并。
“股东与机构”页签还能从十大流通股东的 `gdjc` 入口提取股东 ID，按需查询
该股东的跨股票历史，并在点击某只股票后展开逐报告期变化。全量跨股票记录
在 C++ 内缓存，网页分页返回，不会一次渲染上千行；服务端默认再原子持久化到
`output/tdx-runtime-cache/institution`，重启后仍能在瞬时断线时返回完整陈旧缓存。
“行业画像”页把 467 个
研究行业组织为三级树；个股“行业与板块”页签会按需关联一级行业持仓历史、
二级行业股东结构和自身逐股画像。“大宗交易”独立页按成交、意向申报、
营业部、行业四个视图组织；个股抽屉按需读取其成交与申报历史。“股份回购”页
按 A 股方案、月度进度、年度统计和港股逐笔四个视图组织，并能从个股抽屉读取
同票全部方案历史。“股权变动”页把实际/计划增减持、董监高变动、承诺不减持、
质押风险、市场统计和质押机构组织为七个视图；个股抽屉可直接关联上述单票
历史与当前承诺。
“业绩预告”页按 A 股全市场主表、行业统计和港股预告组织；行业可继续下钻报告期内的具体
A 股，个股抽屉则自动反查其一级研究行业和当前预告。页面右上角可切换并
持久化深色/亮色主题，图表同步换色。“主动基金持仓”页按基金季报汇总
股票增减持，点击股票再按动态键读取持有基金；个股抽屉只在打开对应页签时
加载。“机构龙虎”页在新版数据中心展示四周期机构净买排行，个股抽屉可
切换周期查看异动日；它与逐营业部“龙虎榜”页签保持独立。“评级雷达”页
后端评级接口还支持 `view=us`，读取美股目标价、机构覆盖和单票评级调整历史；
页面当前分别展示港股目标价和行业看多/看空，个股“行业评级”页签会自动映射一级
研究行业并按需加载研报正文。“外资预警”页读取当前上游清单，点击股票
展开持股数量、占总股本比例和状态历史；个股侧栏也可直接核对是否在最新
清单中。“期货与发行”页可从商品期货展开关联 A 股、从股指期货展开净持仓
历史，也可按年份/行业下钻 IPO 股票、查询债券发行人，并查看定向增发、配股和
优先股发行/股息条款。“基金风险分析”页覆盖收益风险、月度能力、择时选股、
公开报告期持仓、持仓稳定性和仓位估算十三类视图，基金主表可直接下钻行业、股票与
历史明细，四类时间序列使用 TradingView 缩放查看。“指标计算验证台”可
直接画出 26 类原生兼容指标；“Level2 协议实验室”提供请求体离线构造、合法
捕获离线解码和会话只读前置检查。当前解码还包含只处理内存 JSON 的
`sdk-json-4653/4655/4671/4680`；CLI 的 `fasthq-subscribe-plan` 只复现
`Name/CODE/SC/LX/PkgType/OperType/PushType/BatchPush`，不序列化、不入队也不发送。
其中 `field_origins` 把 `Name` 标为 IXReq request/envelope 来源，把其余七项标为
IXReq body item；`request_field_count=7`、投影字段为 8。CLI 的
`sdk-fnreqdata-plan` 另把 `1801/1802/1803/1804/18071` 调用恢复为 12 个 32 位
ABI 槽和 25 字节本地 callback correlation 记录布局；它因运行时宿主窗口、导出及
callback key 未解析而保持 `ready/invoked/request_sent=false`，不生成 ABI stack 或
wire/network bytes。
`sdk-fnreqdata-18031-plan` 不复用上述 25 字节关联：它保留 `side_mode_raw`，仅把
原值 1 映射为 SDK selector 0、其余 u8 映射为 selector 1；selected f32 按原调用精确
提升为 double ABI 两槽，cursor/count 固定 `0/1`，registry mode 固定 2 且不是 ABI
槽。关联状态为 `host-global-correlation/needs-live-host-context`，不创建 25 字节记录，
不构造 wire，也不调用 SDK。
`sdk-fnsubscribe-batch-plan` 只从 `--input` 读取 1..16384 字节 strict UTF-8，顶层
对象必须且只能含 `data_type` 与 `symbols`；类型仅为 `1801/1802/1803`，证券数组
为 1..100 个严格大写 `SZ/SH/BJ` 加六位 ASCII 数字。结果发布逗号连接列表、六个
32 位 ABI 槽，以及每票 25B、mode 9、匹配后保留的 correlation template；两个 key
均为 null、template/records/call 均未就绪，host view、security resolver、ABI stack、
wire/network bytes、request/subscription 全部保持 false。传入 1807 会明确引导到
既有专用 `sdk-1807-plan`。
`1369` 的 40 字节内部 IPC 体在 `+38`
保存 `1..1000` 的 `u16 count`（主程序常用 `11/1000`）；`1371` 的 48 字节
内部 IPC 体在 `+37/+38/+42/+46` 保存 `side_mode_raw`、`f32
selected_price`、允许 `-1` 的 `i32 cursor` 和 `1..5000` 的 `u16 count`。
页面不会把未标定的方向/模式原值解释成买卖枚举。个股工作台另有只读“披露日历”
和“数据全景”页签：前者不开放归档写入，后者用一次 `view=security` 请求固定呈现
十类类型化摘要，局部空分类不影响全页。披露归档 HTTP 只能通过带精确
`X-TDX-Action: disclosure-archive` 的 POST `/api/v1/market/disclosures/archive`
写入 `--root` 下固定路径；GET 会拒绝 `archive/archive_path`。当前功能目录共 145 项，
按真实 GET/POST 与 CLI-only surface 映射。
除显式 JSN 下载和有界运行时缓存外接口不写文件；服务端不暴露任意命令
执行。非本机监听仍必须显式增加 `--allow-remote`。

网页周期切换只刷新 K 线接口，不重复请求快照、五档、板块和 JSN。除当日
分时外，初次加载 400 根；向左缩放或拖到数据边界时按 `next_start` 自动
补取更早的 800 根，并保持当前视窗不跳动。服务启动
时会把全部 JSN 解析为常驻证券索引，并复用板块中的证券名称表；当前 326 个
JSN、9,108 个证券键的本机样本中，`000001` 整页五项并发请求约 0.25 秒，
不再按股票逐次扫描全部 JSN 文件。

个股首屏只保留放大的 K 线和买卖五档。右上角“更多”抽屉按概览、股东与
机构、行业与板块、数据关系四类组织扩展资料；其中
`/api/v1/security/profile` 会按市场号和代码批量读取 `cgfxmx1/2`，返回
机构持仓历史和十大流通股东；`/api/v1/market/holder` 再按股东 ID 展开
跨股票和单票报告期历史，两者均不依赖预先下载的单票样本。

已下载 JSN 不在发布包内时，可以指定其目录：

```powershell
tdx-tool serve --root C:\new_tdx --jsn-root D:\tdx-data\tdx-jsn
```

运行时缓存可显式改到其他目录：

```powershell
tdx-tool serve --root C:\new_tdx --cache-root D:\tdx-data\runtime-cache
```

用户公式属于本地私人数据。服务默认加载随程序发布的系统公式快照，不读取
`TCalc.dll`；需要在公式网页中
查看和执行 `T0002/PriGS.dat` 时，必须显式开启只读合并库：

```powershell
tdx-tool serve --root C:\new_tdx --include-user-formulas
```

该开关只把本机 `PriGS.dat` 合并进发行快照，不加载 32 位 DLL、不写回用户目录，
也不会改变 Level2 权限边界。开发构建会把 `native/resources/formula` 和
`native/resources/recon` 分别复制到 EXE 旁的 `formula-assets`、`recon-assets`；
安装包则放在 `share/tdx-tool/` 下的同名目录。主程序不需要 `TCalc.dll`，默认构建
也不包含证据提取器；只有更新证据快照时才显式配置
`-DTDX_BUILD_TCALC_EXTRACTOR=ON`。

## 2026-08-12 当前批次增量

基础公式运算新增一组 TCalc 精确语义：`MA` 从首个有效值开始、窗口内 missing 跳过
求和但固定除以 `N`，逆序累计与结果均经 float32；`CROSS` 使用前导有效门禁和
`abs(left)*1e-7+1e-5` 滞回；`EMA/EXPMA` 共用 handler，动态周期不重新播种，后续
源 missing 继承上一输出。二元减法、乘法均先收窄两个操作数并输出 float32，任一
missing 则结果 missing。`EXTERNAL#KDJ.J` 的 RSV 除法、比例缩放和 `3*K-2*D`
快捷计算已复用同一公共运算路径。

Level2 新增纯领域 `sdk-quote-transition` 投影，输入精确 380 B 的 `1807/18071`
body、显式上一状态和 context，输出下一版 host quote state；它通过 `level2 project`
CLI、确认 POST `/api/v1/level2/project` 和 Level2Lab 暴露。另新增 CLI-only
`sdk-compatibility` preflight：只读取显式 root 下候选 DLL，通过独立、有界的 PE
export parser 检查 host edition、7 个必需导出和 2 个可选导出，不加载 DLL、不读取
配置、不联网。当前 `C:\new_tdx` 的 `TdxDataSDK.dll` 缺失，故结果为 `absent`。

Svelte 数据中心同时新增板块轮动页面，展示行业、概念、地区、风格四类汇总与 515
条明细，并支持周期、方向、排序、检索、刷新和成员跳转；不可用的地区成员及缺失的
当前行情列保持明确缺失。

## 2026-08-13 当前批次增量

公式 `NOT` 现在复现独立的 started 状态：只跳过前导 missing，启动后 missing/sentinel
输出 `0`；有限输入先收窄为 float32，再按精确零输出 `1`、其他值输出 `0`。这不会
改写全局 truth 或一元负号；379 条内建公式中有 10 条使用 `NOT`。

`level2 project --format sdk-correlation-transition` 新增 25 B registry 的 CLI-only
离线状态投影。callback 以 `(type,key1,key2)` 首匹配，mode 9 保留，其他 mode 在
host lookup 前消费；upsert 以 `(type,mode,market,code)` 首匹配并换 key，否则追加。
入口严格校验 JSON、UTF-8、u32，registry 保守限制为不超过 10,000 条；输出明确保持
SDK/callback/host lookup/message/network/entitlement-bypass 等副作用标志为 false。

网页 `/data/hot-history` 和 StockWorkbench 面板复用本地热点历史：目录共有 210 条、
203 只证券且上游网络请求为 0。只有 `native_host_eligible=true` 才生成安全链接，
`analysis` 以纯文本呈现；数字 market 别名不再被转为字符串。因服务端负责分页排序，
面板不再对单页数据提供误导性的本地表头排序。

## 构建

当前验证构建使用 MSYS2 UCRT64 GCC、静态 zlib 和 CMake/Ninja。GCC、
libstdc++、winpthread 与 zlib 均静态链接到 EXE；运行时不需要 MSYS2 DLL。

```powershell
$originalLocation = Get-Location
Set-Location web
npm install
npm run check
npm run build
Set-Location $originalLocation

$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH

cmake -S native -B build/native-mingw -G Ninja `
  -DCMAKE_MAKE_PROGRAM="C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" `
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
  -DCMAKE_BUILD_TYPE=Release `
  -DZLIB_LIBRARY=C:/msys64/ucrt64/lib/libz.a `
  -DZLIB_INCLUDE_DIR=C:/msys64/ucrt64/include

cmake --build build/native-mingw --parallel
ctest --test-dir build/native-mingw --output-on-failure
cmake --install build/native-mingw --prefix dist/tdx-tool
```

Node.js 只参与 Svelte 的开发和编译。`cmake --install` 会把 `web/dist`
安装到 `share/tdx-tool/web`；最终运行仍只需要发布目录中的
`dist/tdx-tool/bin/tdx-tool.exe`，不需要 Node.js 或 Python。

## 迁移边界

旧 Python 文件保留为历史协议证据和交叉验证样本，但不属于新工具的运行
路径。公开 L1、云查询、高价值市场聚合、26 类原生指标、TPool 只读检查与
逐规则求值、Level2 内置/TdxW 内部 IPC/SDK 请求离线构造、SDK 1807 类型化
订阅计划、`FastHQ.Subscribe` 逻辑任务计划和十八类捕获/SDK JSON 格式解码均已
迁入 C++。TdxW `1369/1371` 构造结果仅为
内部 IPC 字节，不会发送，也不冒充 SDK 网络请求体。
云端公式计算应用层现按配置审计、模板、单行请求、批量请求和 CLI 分为独立翻译单元；
底层 config、builtins、interpreter、host 模块保持独立，生产路径不加载 Python 或
`TBigData.dll`。
TCalc
的 18 个无源码原生指标入口已经全部按入口语义恢复到解释器：KDJ-TDX、
BOLL-M、BB、WIDTH、ASI、NVI、PVI、SAR、VTY、MSI、MCST、SSRP、PAV、
PAVE、NDB、SC、XLPL 和 ZXNH。公式正文/恢复体覆盖因此达到 379/379；
XLPL 的 BACKSET 状态回标和 ZXNH 的前向分段只允许显式只读绘图，不进入扫描或回测。
分析器另将展示占位与数值语义分开；schema 7 当前确认 379 条公式全部通过
数值输出传播审计，90/90 条含绘图公式的展示语义已物化为 IR，379/379 条展示
语义可信且未知展示指令为 0。56 条 `presentation-return-only` 只是绘图调用返回值
的兼容占位，不是缺少 renderer；它们没有传播到数值输出。
扫描和回测会拒绝任何占位值污染输出。`CODE/STRCMP` 使用精确字符串，
`DRAWGBK_DIV` 明确为纯展示调用；L2 字段、券商私有 `SIGNALS_QS` 与 34 个
只读账户/策略状态也已成为
严格的调用方显式上下文，而不再是解释器缺口。没有真实外部序列时仍明确拒绝，
不会用零值替代；四个交易动作不会被模拟。`USEDDATANUM` 广播完整求值柱数，
`SETCODE` 保留深沪京别名/ID 及扩展市场原始 ID，`MINDIFF` 优先使用 `min_tick`，
否则从 `price_precision` 推导并保留 `1e-5` 下限。调用方可在 K 线文档中显式提供
最多四段、有序不重叠的 `trading_sessions`（允许一个跨午夜周期），共同驱动
`FROMOPEN/TOTALFZNUM`；未提供时只对 A 股使用默认会话，非 A 股返回 missing。
`TDXZXNH` 使用同一市场 ID 归一化：`qz/qd/qs/cz/qg` 分别与
`28/29/30/47/66` 完全同义，不再因别名走错价格来源分支。
360 字节 TNF 证券记录的 `+76` 现作为缓存内 `price_precision`（仅接受 `0..8`）
补入 K 线文档，`price_precision_source=local-tnf-security-master`，并由既有
`MINDIFF` 逻辑换算最小价差；314 字节旧格式没有同等证据，绝不读取该偏移。
调用方显式 `min_tick/price_precision` 优先，目录缺失时保持既有回退，全程不联网。
解释器还 additive 输出 `tdx-formula-trade-event-ir-v1`，仅识别顶层且严格两参数的
`BUY/SELL/SELLSHORT/BUYSHORT/BUYSHORT_BUY/SELL_SELLSHORT`。原 `points` 数值序列
保持兼容；condition 或 price missing 都会清 condition，price 本身仍保留 null。每柱
历史候选明确为 offline/non-native，只有最后一柱 condition 约等于 1（epsilon 1e-5）
才形成只读 latest host action。基础 wrapper/host 位为
`0x1/0x10/0x100/0x1000`；两种组合分别为 wrapper `0x10000`、host `0x1001`，以及
wrapper `0x100000`、host `0x110`。IR 全局标明无副作用、下单、账户或网络访问。
`IF/IFF` 现在跳过前导 missing，进入有效区后只把精确零判为假；`AND` 让精确零
优先于 missing，否则传播 missing；`OR` 把 missing 当假，并按约 `1e-5` 的绝对值
阈值判真。`REF` 使用 started gate：前导 source/offset 均有效后才启动，启动后当前
source missing 不阻止历史引用，missing、非正或左越界 offset 继承该节点上一输出。
除法收窄为 float，先传播任一 missing，再在分母约 `1e-5` 近零时继承上一输出；
`< <= > >=` 使用 float 与左值锚定的 `abs(left)*1e-7+1e-5` 容差，内建
`KDJ.J` 的 RSV 也复用同一除法。精确顶层 `AUTOFILTER` 标记会在 raw candidates 之外按文档配对规则生成
单一 `flat/long/short` 投影，遍历次序固定为 bar 外层、源码 statement 内层；它只覆盖
上述六类信号，v1 不含 `CLOSEALLD/CLOSEALLK`，也不宣称已验证原生宿主同柱去重。
FormulaLibrary 可查看 raw/filtered candidates、filtered latest action、接受/过滤数和最终
position；primitive 明细仅在展开后序列化，每类默认预览最近 100 条并显示总数/截断，
该展示仍只是无副作用的离线 IR，不会调用宿主交易动作。
公式详情页生成的 JSON 模板现在可以直接编辑；填满合法数值后，
网页以无 `source` 的确认 POST 提交公式代码和显式上下文，服务端从内置库选择源码、
执行后标记 `formula_source_mode=library-post`，且不保留请求体或公式源码。90 条含绘图公式现会随 `evaluate` 返回
`tdx-formula-render-ir-v1`：普通线引用 `points`，绘图事件仅记录命中 bar 的
索引、求值参数、动态字符串、源码 `statement_index`、连续 `render_order` 和逐语句有序样式；16 个命名色、`COLORrrggbb` 与 `RGBXrrggbb` 会发布从
`TCalc.dll` 证据恢复的精确 Windows `COLORREF`；它不宣称像素级等价。
 其中 `DRAWNUMBER_DIF` 使用单活动序列状态机：条件按 `abs(value-1)<0.0001`
判断，活动期间忽略新触发，START/NUM 锁定在源 bar，STYLE 逐 bar 精确求值。
STYLE 0/1/2 分别为贴近锚点、10px 点状引线、引线加同色 `0x50` Alpha 直角框；
数字/字母盒为 `8×14/14×14`，会在 HIGH/LOW 与窗格边缘间翻转。IR 还发布逐
bar STYLE，Svelte 在缩放/平移后按首个可见 bar 为整层重选字体 index 1/13。
`PARTLINE` 明确携带
`DIRECT` 对应的线段起止索引，`DRAWBAND` 逐根携带按上下关系选中的填充色；
原生绘制器已证明该带状区域通过实心画刷和 `StrokeAndFillPath` 不透明填充，IR
发布 `band_fill_opacity=1`，网页不再套用统一半透明样式；
`DRAWTEXT/DRAWNUMBER` 逐事件携带真实价格和文字；无框路径按
`abs(COND-1)<0.0001` 判断，跳过缺失价格，使用 renderer 的原生像素偏移和
`DRAWABOVE` 行高规则，不做边缘夹取、自动翻转或碰撞避让。`DRAWTEXT` 按 `&`
拆分至多十段并保留空行高度；`DRAWNUMBER` 按 IndexInfo/图表精度规则格式化。
`DRAWTEXT_FIX/DRAWNUMBER_FIX` 携带窗格比例坐标和左右对齐；`DRAWCFRAME` 只对
`DRAWTEXT` 生效，固定文字明确标为 renderer ignored，数字明确标为 not-forwarded。
有效 frame 发布 bar HIGH/LOW 锚点、上下空间规则、20px 点状 leader、4px 圆角、
`5/4`px 外扩、`(3,3)` 文字 inset、`0x50` 填充 Alpha 和不透明 1px 边框；
 `PLOYLINE` 只连接相邻条件顶点，`DRAWLINE` 物化起止锚点、斜率和向右延长端点，
 `DRAWKLINE` 固定发布 H/O/L/C 参数次序；`DRAWGBK_DIV` 按原生类型 21 发布连续
 区间、全窗格/区间高低极值/首 OPEN 至末 CLOSE 范围，官方模式不透明，私有
 `10..20` 模式的 Alpha 为 `255*(mode-10)/10`，系统模式 17 即 `178/255`；
 Svelte 会随图表缩放/平移重算这些 SVG 图元、带状
 填充和价格文字坐标；`DRAWICON` 则使用 `TCalc.dll` 的原生 18×18 精灵图，
 `STICK/LINESTICK` 按零轴细 stem 绘制，后者再连接连续有限值；普通线和
 `DOTLINE` 按 `DRAWNULL` 断段并保留孤立点短线，`CIRCLEDOT/CROSSDOT/POINTDOT`
 会按缩放后的 bar spacing 切换原生像素/圆/叉几何，避免退化成宽 histogram、
 通用虚线、方形 marker 或跨缺失值折线。混合图元公式会统一按 `render_order` 分组叠放，
 TradingView 只继续提供可缩放的时间/价格坐标。
全库运行审计使用 `schema_version=2` 结构逐条保留全部 379 条记录；平安银行
日线、完整市场上下文及显式未来只读模式的当前结果为 354 条通过、20 条依赖不可用、
4 条市场不适用、1 条周期不适用、0 条执行错误，报告数与公式库总数严格相等。
TPool 的跨证券排名、一秒调度、多节点/带环 flow 状态转移、独立 JSON 原子恢复和
`psatt` 动作规划已实现；`baimpool` 开启时，按日入池记录、声音、提示和板块操作会
按证券进入 cell 输出 `planned_actions`，但固定为 `executed=false`。`bsavehis` 是
状态序列化阶段的每日 `.dat` 快照，`bclearblock` 仅是板块保存前的清空选项。
`btip` 已恢复为 TPool 自有置顶定时窗口及 187 字节批量载荷，不再误标为宿主回调。
板块保存则明确走第二注册回调的七参数签名，以操作号 88 传递每只
`1 字节市场+6 字节代码`。`pool inspect` 还公开三个注册槽：第一槽 11 参数数据查询、
第二槽 7 参数 UI/动作路由、第三槽逻辑 4 参数辅助操作；只描述 TPool 直接使用的
类型/操作号，不调用宿主函数。
`bdel/ndelnum/ndeltype` 已按 DLL
运行路径恢复为历史记录过期清理：`0/1/2/3` 分别为天/小时/分钟/秒，它属于周期
维护策略，不再错误附着到 cell-entry 的证券列表。原 worker、宿主 UI/声音调用及
池 XML/文件写回没有实现。`pool history` 已能只读发现并解析上述每日文件：`.log`
发布 5 个入池字段及 `market+code` 重复诊断，`.dat` 发布 14 个完整状态字段；输入
会先归一为 UTF-8，字段缺失和非法数值分别报告，目录限额优先保留最新日期。
`--native-text-output` 进一步复现 `sub_1003A720/sub_1003AC40` 的原版中文表头、
列次序、两位小数、CRLF 和默认 GBK 编码；证券名称只从本地目录解析，未命中保持空列。
同一 `--input` 也能反向读取原客户端生成的两种 `*_his.txt`。状态文本只携带最高日期的
月/日，因此 JSON 明确发布 `maximum_date=null` 和 `maximum_date_precision=month-day`，
不会利用文件名猜测年份。GET `/api/v1/pools/history` 只允许
`pool/cell/from/to/limit`，且 `kind` 只能是 `all|snapshot|entry`；端点只查询服务配置的 root，拒绝
`input/path/url/file`，文件和目录路径均稳定投影为 root-relative 且响应不含服务器
绝对根目录。TPoolLab 的“每日历史文件”可筛选、查看文件和成员；真实没有文件时显示
空态，不伪造样本。

第二批增量验证通过 `tdx-level2-tests`、两项 `sdk-fnreqdata-plan` CLI smoke、
`tdx-formula-engine-tests --domain language`、`tdx-server-tpool-history-tests`、
`tdx-server-catalog-tests`；`npm run check` 为 0/0，`npm run build` 仅有既有 chunk
warning；该批没有运行完整 CTest 或 full API。第三批 blocks、formula language、
Level2 focused 均通过，完整构建与 CTest 123/123 通过，但未运行 full API。
该阶段 build/install/restart 后正式服务为 PID `17540`，EXE SHA-256
`007f61021734ea56ed9b7106bafbe46bd74e1a01467c539734faa8072f457645`；网页没有变化，
`index.html` SHA-256 仍为
`f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`。运行时
平安银行本地 day/5 bars 返回 precision 2 与 local TNF source；18031 的 side 255
返回 selector 1、double bits `0x4024800000000000`、host-global/no-25B，且
ready/invoked/sent 均为 false。PID `11468` 与对应 EXE hash 仅是第二批历史部署。

第四批 Level2、formula language、formula strategy focused 通过；完整构建用时
94.9 秒，CTest 123/123 用时 50.30 秒。正式运行时只跑 health、batch-plan CLI、
普通公式、BUY IR、arity HTTP 400 五项 focused 合约；未跑 full API。网页本批没有
变化，未重复 `npm run check/build`。第四批正式服务 PID `44960`，监听
`127.0.0.1:8765`，root `C:\new_tdx`，显式 include-user；EXE SHA-256
`ef22956254d57079c44f8d106979fb773e74a9612db39413fc7249f4d910d808`，网页 SHA-256
仍为 `f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`。
PID `17540` 和 PID `44960` 及对应哈希都只保留为历史部署。

第五批的 Level2、formula language、context-and-library、strategy、server-level2 与
server-catalog focused 均通过；`svelte-check` 为 0/0，生产构建成功且仅有既有 chunk
warning；完整 C++ 构建成功，CTest 123/123。临时与正式 HTTP 样本均通过：平安银行
40 bars 返回 2 个 primitive，AUTOFILTER 接受 16、过滤 22，副作用为 false；含 1 条
显式用户公式的 380 条全库审计为 0 errors。第五批阶段正式服务 PID `17908`，EXE SHA-256
`03bb4a68b87cbcda4d1a27534da3ba5914d9c49a1594533e3db496c7ba17af31`，网页
`index.html` SHA-256
`8ffc02ac014017478efabf576619732b58af123c123697dd03f48fa0773ff37b`；health 为
`native_cpp=true`、`python_runtime=false`。PID `17908` 与对应哈希现仅为历史部署。

第六批 formula language、context-and-library、strategy、Level2、server-level2 与
server-catalog focused 均通过；`npm run check` 为 0/0，生产构建通过且仅有既有 chunk
warning；完整 C++ 构建通过，CTest 123/123（49.99 秒）。临时与正式 HTTP 合约均
通过：callback invocation 的 1804 body 为精确 432 B，平安银行 40 bars 使用原生运算，
全库审计为 `total=380, passed=228, errors=0, context=128, period=1, market=3`。
当前正式服务 PID `21416`，EXE SHA-256
`07b972da59da1cb959678c701ffb9be59eba023a2550bc533e4ee76557e7da6a`，部署网页
`index.html` SHA-256
`1f202c046bc8f3ff2c36de731027b72112b7c8c206a62b8d13df326b58107c9f`；health 为
`native_cpp=true`、`python_runtime=false`。这些验证不代表取得 L2 授权、绕过权限或
真实投递宿主消息。

第七批 formula、Level2、server、catalog 和板块轮动 focused 全部通过；
`svelte-check` 为 0 errors / 0 warnings，生产网页构建成功且只有既有 chunk warning。
完整 C++ 构建成功，CTest 126/126，用时 99.91 秒。正式 runtime 的 18071 状态投影
没有 SDK/network/message 动作；平安银行 40 bars、板块轮动 515 条/4 类汇总且
`errors=0`、OpenAPI `/api/v1/level2/project` POST-only 均通过。平安银行 day 80 bars
全库公式审计得到 `formula_count/reported=380/380`、`eligible/passed=335/335`、
`errors=0`、`context_unavailable=20`、`future_disabled=20`、
`market_inapplicable=4`、`period_inapplicable=1`、`with_numeric=329`，无运行错误。

第七批正式服务 PID 曾为 `40020`，监听 `127.0.0.1:8765`；EXE SHA-256 为
`86fb3a4b674cbb60c817383e3b4f154b0b4482d133a1c03333510ba67c93ed04`，部署网页
`index.html` SHA-256 为
`b1dc8125928d3d6d0e1359b3ce532d4c0beb90211d82577f0cfc1ab8b2cce6ed`。health 确认
`native_cpp=true`、`python_runtime=false`。PID `21416` 与对应哈希现仅保留为第六批
历史；本批不声称 Level2 授权绕过、SDK 加载、SDK callback 执行或宿主消息发送。
授权登录、会话票据、主动 Level2 网络请求和绕过
权限均未实现，也不会伪装成可用命令；继续标定需要用户自己的合法授权会话
与真实业务样本。

第八批只验证受影响目标：formula `native-operators`、`language`、
`context-and-library` 通过，独立 correlation target/test 通过，`tdx-tool` 目标构建成功；
`svelte-check` 为 0 errors / 0 warnings，`npm run build` 成功且只有既有 chunk warning。
本批未运行完整 CTest（当前注册 127 项）或 full API 契约。

当前正式服务 PID `36624`；EXE SHA-256
`6b8b507a32b355a173bf95daac92afb77d73ffeaaf95275afa1d4180179d4499`，部署网页
`index.html` SHA-256
`ca685162fc620086b6caeb9653bceb909dfe8543a293cf00287dea55242ab42f`。health 为
`native_cpp=true`、`python_runtime=false`、`formulas=380`、`blocks=1159`。正式 smoke
确认 NOT 5 点结果 `[null,1,0,0,0]`；correlation 命中索引 0、消费后投影 1 条且所有
副作用标志为 false；热点历史为 `catalog_count=210`、`returned=2`、网络请求 0，
数字 market 回显 `0`，`sh603137` 返回 1 条可安全跳转记录，部署 UI asset 存在。
PID `40020` 及其 EXE/web 哈希现仅为第七批历史。以上仍不代表取得或绕过 Level2
授权、加载 SDK、执行真实 callback 或投递宿主消息。

## 第九批：TCalc 滚动核心、1804 host slots 与板块回测网页

`SUM/HHV/LLV` 已由通用窗口实现切换为独立 TCalc handler，fresh IDA 证据为
`output/ida-tcalc-rolling-core-20260813.log`。`SUM` 从首个有限源值生成 float32 前缀，
源 missing 继承前缀；只有完整、有效且位于首个有效值之后的正周期窗口才以逆向
float32 累加结果覆盖前缀，窗口内 missing 跳过。`HHV/LLV` 对有限的非正周期和超长
周期使用截至当前柱的全部可用历史，周期 missing 保持 missing；比较使用
`abs(candidate)*1e-7+1e-5` 量级的原生容差并由相等带内的后出现值替换。`HHV` 忽略
missing，`LLV` 则保持 TCalc 的不对称 sentinel 行为：内部 missing 清空选择，后续
有限值可恢复，尾部 missing 令本柱输出 missing。所有候选、累加与结果均经 float32。

首阶段新增 CLI 入口 `level2 project --format sdk-1804-host-projection`。输入必须是 raw 或
hex 编码的精确 432 B body；输出复现全零初始化的 105 个 f32 槽（420 B），将
`+8/+216` 的 f64 价格窄化到槽 1/2，将 `+424/+428` 的 u32 原始计数位保存到槽
3/4，并把 `+16/+224` 的两组 f32 数量最多各复制 50 项到槽 5/55。原始 count 不因
clamp 改写，方向只保留 `first/second`。命令不调用 SDK/callback/host storage，
不构造 wire，不联网、不发送请求且不绕过授权。

Svelte 数据中心新增 `/data/block-backtest`。主表按 category、begin/end、adjustment、
sort/order、limit 请求既有 C++ 端点，成员页仅允许对唯一解析的本地 block identity
下钻；安全证券身份可进入个股工作台。页面完整处理 loading/error/empty/cache，切换
板块失败不会把旧成员挂到新 identity，并明确声明服务端当前选择的成员不是历史时点
成分重建；表格不做会破坏服务端排行口径的本地排序。

本批 focused 验证通过 formula `native-operators`、`language`、
`context-and-library` 和独立 1804 target/test；`svelte-check` 为 0 errors / 0 warnings，
`npm run build` 成功且只有既有 chunk warning。该 1804 CLI 首阶段未运行完整 CTest 或
full API 契约。
正式 PID `4716` 的 focused 样本为：`sz/000001` 5 日 `NATIVEROLLING` 输出
`S=[5,5,4,7,6]`、`H=[5,5,5,4,4]`、`L=[5,null,4,3,3]`；432 B 1804 样本投影
105 槽/420 B，first/second raw/copied 为 `2/2` 与 `3/3`，SDK/callback/host
storage/wire/request 均 false、network 0；industry 板块主表 normalized 56，
`880310` 成员 normalized 40，首项为 `bj/920088`。

当前正式 EXE SHA-256 为
`d44ddaa5c95398cbac5a860963401c02eb682a574e3823b62bf5a38291ef01d8`，部署网页
`index.html` SHA-256 为
`838bb4e58df85fd2ae4d46915142fe1a9a5fe0add2f3e2f8ba557344786f1420`；health 为
`native_cpp=true`、`python_runtime=false`、`formulas=380`、`blocks=1159`。
PID `36624` 与相应 EXE/web 哈希现仅为上一批历史部署。

## 本批：TCalc 布尔窗口、SDK 1801/1802 host records 与 LimitReview UI

TCalc 静态证据现由
`output/ida-tcalc-rolling-core-20260813.log` 覆盖 `SMA/COUNT/EVERY/EXIST/MAX/MIN`，
由 `output/ida-tcalc-window-positions-20260813.log` 覆盖
`BARSLAST/BARSLASTCOUNT`。解释器据此改为独立原生 handler：

- `SMA` 将末柱 `N/M` 以原生 float-to-int 截断后用于整条序列；只有 `N>M` 且
  `N>=1` 才执行。从首个有效 float32 源值播种，后续使用
  `(X*M+previous*(N-M))/N`，每柱重新收窄；启动后的 missing sentinel 进入递推，
  精确回到 sentinel 或非有限时再映射为 missing。
- `COUNT` 以约 `1e-5` 判断周期是否相对末柱保持常量，并区分常量近零、动态近零和
  实质负周期；窗口最多夹到已有历史，只统计 float32 后近似等于 1 的值。
  `EVERY` 从首个有效源启动，周期 missing/小于 1 时输出 0 但不清空连续真值长度；
  启动后的源 missing sentinel 作为非零值。`EXIST` 只读取末柱周期，内部源 missing
  保持 missing，并显式复现 `-N`/`i-N` 的 signed-i32 wrap。
- `MAX/MIN` 先寻找首个两侧共同有效柱，之后把内部 missing 还原为原生有限 sentinel
  做 float32 比较；相等选择右侧。多参数调用只作为逐个二元左折叠扩展。
  `BARSLAST` 跳过前导 missing/零，从首个非零有限值起计数，内部 sentinel 会重置为
  0；`BARSLASTCOUNT` 从当柱向前跳过 missing，只累计近似 1，遇近似 0 终止。

首阶段新增 CLI 入口：

```powershell
build\native-mingw\tdx-tool.exe level2 project `
  --format sdk-1801-host-projection --input callback-1801.hex `
  --encoding hex --market 0 --code 110000
```

1801/1802 分别严格接受解码后非空且为 52/40 B 整数倍的 body，统一限制 384 KiB；
market 只允许 `0..2`，code 必须六位数字。领域层直接解析 signed i64 epoch-ms/volume，
复现 `sub_689E20` 的本地时间偏移、完整证券分类与 10/100 手除数、x87
`price*10000+0.503000020980835` chop/整数不定值边界，以及 signed quotient/remainder。
每条输出恰为 20 B；每份 callback 文档声明清空并替换前一逻辑状态。1801 的
`first_raw/second_raw/qualifier_raw` 不猜业务语义；1802 保留已有证据确认的
`order_id_raw/side_or_cancel_qualifier_raw/action_raw`。空 body 所代表的 clear-only
路径不在 CLI 合同内。SDK、callback、host storage、host message、wire、network、
send 与 entitlement bypass 标志全部为 false/0。

Svelte 新增 `/data/limit-review` 四视图页和 StockWorkbench 单票复盘面板。页面只显示
归一化字段，保持服务端分页/顺序及安全证券身份；`raw` 审计字段默认不渲染，不自动
轮询。文案明确数据为盘后逐步更新、非实时，与实时 `market limit-quality`/盘口分层；
指定日或单票动态源尚未生成时显示正常空态。

增量验证通过 formula `native-operators`、独立 quote-transition 与 180x host
projection targets/tests、`npm run check`、`npm run build` 及 `tdx-tool` 主程序构建；
未运行完整 CTest 或 full API。PID `24992` 的平安银行 40 bars 末值为
`S=11.2484588623047/C=0/E=1/X=0/H=11.2299995422363/L=11.1999998092651/B=3/BC=0`，
`errors=0`。LimitReview current/history/daily/annual 的
`returned/source_rows` 分别为 `3/165、3/484、3/111、3/129`，security 为空且
`missing_sources=1`。1801/1802 fixture 分别产生
`843039300000f6ffffffff004433221188776655`、
`4038204e0000feffffffff4243000000d4c3b2a1` 两条 20 B host record，副作用计数均为零。

当前 EXE SHA-256 为
`17815ec422ff519eb69c69ecc3fe7186119b7ad00f2d3f41e6130754319f7c03`，部署网页
`index.html` SHA-256 为
`68b198ebed1345089b9a8df0b9255ca27f6e4c04e1d04fc6391ff0f4b77b428c`；health 为
`native_cpp=true`、`python_runtime=false`。PID `28660` 及其旧 EXE/web 哈希现仅代表
前一批历史部署，PID `4716` 与相应哈希是更早的历史阶段。

## 后续收口：HHVBARS/LLVBARS/FILTER/FILTERX 与 180x POST

独立 `formula_functions_window_positions.cpp` 现按
`output/ida-tcalc-window-positions-20260813.log` 实现四个剩余 handler：

- `HHVBARS/LLVBARS` 的 N 每柱先收窄为 float32 再截断；N missing 时该柱 missing，
  N 非正或超过已有历史时改用全部可用柱。leading sentinel 被跳过，启动后的 internal
  sentinel 仍是普通候选；比较使用候选相关的相对 `1e-7` 加绝对 `1e-5` 容差，
  容差带内选择后出现值，结果为 float32 bars distance。
- `FILTER/FILTERX` 先排除 source sentinel，再以包含端点的 `±1e-5` 判断信号；N 也先
  转 float32 再截断。`FILTER` 正向保留命中并跳过后续 N 柱；`FILTERX` 反向时只有
  `N<=cursor` 才跳过前置 N 柱，N 超出已有前缀不会把该前缀误清空。

1801/1802 的同一 domain projection 已从首阶段 CLI 扩展到确认 POST
`/api/v1/level2/project` 和 Level2Lab。POST 的唯一字段集合为
`format/market/code/payload_hex`；只收内联 hex，继续校验 52/40 B 整数倍、非空、
market `0..2`、六位 code 与 384 KiB 请求/输入边界，路径、URL、文件和所有未知字段
均拒绝。响应提供 `input_size` 等传输元数据但不回显原始 payload，明确
`input_retained=false`。Level2Lab 提供 data type、market、code 和内联 hex 输入并
复用 POST；SDK/callback/storage/message/wire/network/send/bypass 仍全部为 false/0。

后续验证中，formula `native-operators`、`tdx-server-level2-project-tests` 与
`tdx-server-catalog-tests` focused 均通过；`svelte-check` 为 0 errors / 0 warnings，
网页 production build 成功。完整主程序构建耗时 104.7 秒，CTest 129/129 用时
59.5 秒。正式 `sz/000001` day 40 bars 返回
`H=3/L=0/F=0/R=0/errors=0`，分别对应 HHVBARS/LLVBARS/FILTER/FILTERX；FILTERX
仅在显式 `allow_future` 的只读求值中启用。1801 HTTP 样本为
`schema=tdx-level2-sdk-1801-host-projection-v1`、`input_size=52`、source/host
`1/1`、host hex `7440649001007b00000000005100000052000000`，并保持
`sdk_called=false`、`network_requests=0`、`input_retained=false`。

该中间阶段正式 PID `22116`；EXE SHA-256
`580ed60e50492f7a5e52d1b1093bf3e9a7726fb15d4f0862712419e1828289c3`，部署网页
`index.html` SHA-256
`5e246f8299a167526971062ee4f252614503b3749c51e63c35d507074c4bd0f6`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `24992` 及其 EXE
`17815ec422ff519eb69c69ecc3fe7186119b7ad00f2d3f41e6130754319f7c03`、网页
`68b198ebed1345089b9a8df0b9255ca27f6e4c04e1d04fc6391ff0f4b77b428c` 现仅为
前阶段历史部署。

### 1804 POST/UI 与最终产物

随后 `sdk-1804-host-projection` 也进入 `/api/v1/level2/project` 和同一 Level2Lab
面板。1804 POST 只允许 `format/payload_hex`，严格要求内联 hex 解码后恰为 432 B；
market/code、文件/路径及所有额外字段均拒绝，并继续执行 384 KiB 与输入不回显合同。
网页选择 1804 时不显示证券身份，提交前校验精确长度，结果显示 105 槽/420 B；面板
已打入最终 web bundle。

正式 HTTP 样本为 `schema=tdx-level2-sdk-1804-host-projection-v1`、
`input_size=432`、`slot_count=105`、`byte_size=420`、first/second copied `2/3`；
`sdk_called=false`、`host_storage_call_attempted=false`、`network_requests=0`、
`input_retained=false`。1804 HTTP/UI 追加后只重跑 project/catalog focused，均通过；
`svelte-check` 为 0 errors / 0 warnings，production web build 成功。104.7 秒完整构建及
当时 CTest 129/129（59.5 秒）是追加前 production 阶段的结果；最终 surface 完成后
再次运行完整 CTest，129/129、0 failed，约 73 秒（10:38:34—10:39:47）。本轮未运行
full API。

最终正式 PID `4336`；EXE SHA-256
`0c945050aab8db2134cc5e23b5e62a4dc7431e77630e2d53cbfaca0349198c28`，部署网页
`index.html` SHA-256
`3eba9d193d3b04840b57a709be00907926c20f07cfca6438eadc5c7b1b152339`。health 与磁盘
哈希匹配，`native_cpp=true`、`python_runtime=false`。PID `22116` 及中间 EXE
`580ed60e50492f7a5e52d1b1093bf3e9a7726fb15d4f0862712419e1828289c3`、网页
`5e246f8299a167526971062ee4f252614503b3749c51e63c35d507074c4bd0f6` 现仅为历史；
PID `24992` 及更早哈希也不代表当前部署。

## SUMBARS/STDDEV 原生语义收口

`output/ida-tcalc-sumbarsx-beta-final.log` 的 `sub_1001ABE0` 证明 `SUMBARS` 只跳过
前导 source missing；启动后的 source/target missing 以有限原生 sentinel 参与比较与
累加。窗口向后以 float32 累加，阈值判断使用绝对 `1e-5` 加相对 `1e-7` 容差；当前
柱即可达到阈值时，首个有效柱因左边界钳制返回 `0`，其余柱返回 `1`。例如 source
`[10,10,10]`、threshold `10` 得 `[0,1,1]`。独立的 `SUMBARSX` handler 未改动。

`output/ida-tcalc-formula-rolling-variance.log` 的 `sub_1001EA70` 和
`sub_1001E800` 证明 `STDDEV` 只取末柱周期，先收窄为 float32 再截断并要求
`N>=2`；从首个有效 source 后第 N 柱开始，以此前 N 个价格的 N-1 个滞后对数收益
计算。价格非正或不高于 epsilon 时对应收益为零；source 读取、比值、log、和、均值、
平方偏差和、方差以及 sqrt/result 均按 handler 保留逐步 float32 落点。

本批只运行并通过 formula `native-operators` focused，没有重跑完整 CTest 或 full API；
129/129 是上一轮最终 1804 surface 阶段结果。正式平安银行 day 40 bars 样本末柱为
`S=1`、`V=0.0008861038950271904`、`engine=native`。当前正式 PID `23140`；EXE
SHA-256 为 `25882a4e62eda0d5c1702e2e4da4d8f60739c47ab0927616a8ee13cf0e3b9183`，
部署网页 `index.html` SHA-256 仍为
`3eba9d193d3b04840b57a709be00907926c20f07cfca6438eadc5c7b1b152339`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `4336` 及旧 EXE 哈希仅为
上一阶段历史部署。

## SDK JSON 4653 domain、CLI、POST 与 Level2Lab

新增 `normalize_level2_sdk_json_4653`，并通过通用 SDK JSON adapter 暴露
`level2 decode --format sdk-json-4653`。领域合同只接受顶层精确 `{Data}`，数组最多
10,000 条；记录只允许 `datetime/closePrice/averagePrice/tradeVolume/reference_price`，
`datetime` 必须是非空 ASCII 数字串，首记录必须带 `reference_price`。三个价格字段沿
原生路径先转换为 float32，再以 float32 `/1000`；`tradeVolume` 严格保持 u32。

输出的 `native_header_size=35`、`native_record_size=18` 与 `35+18*N` 只表示恢复的布局
语义；实现不生成 native body。`datetime` 仅投影 raw/right4 和证据命名的
`time_u16_raw = u16(61 * (atoi(CString::Right(datetime, 4)) % 100))`，不猜其日期、
分钟或秒语义。compact document 上限 384 KiB，Data 与 `limit` 上限均为 10,000。

确认 POST `/api/v1/level2/decode` 对 4653 只接受 `format/document/limit`，复用相同
384 KiB 与 `0..10000` 校验；路径、文件、SDK 参数和其他字段全部拒绝，响应不回显
document。Level2Lab 新增对应 JSON 编辑器、limit 控件和 opaque-time/float32/安全边界
说明。POST/UI 不读取服务器文件；领域、CLI、POST/UI 都不调用 SDK/callback，不构造
native body/wire/request，不联网或发送订阅，处理后不保留输入，也不绕过授权。

独立 domain、server Level2 与 catalog focused tests 全部通过；`svelte-check` 为
0 errors / 0 warnings，production web build 成功并仅报告既有 chunk warning；完整
CTest 为 130/130、0 failed、50.51 秒。正式 runtime fixture 为 `count=2`、
`returned=1`、`reference_price=10`、`close_price=10.25`、
`average_price=10.100000381469727`、`trade_volume_u32_raw=123`、
`time_u16_raw=61`，并保持
`sdk_called=false`、`native_body_built=false`、`network_requests=0`、
`input_retained=false`。

当前正式 PID `12148`；EXE SHA-256
`b8e66b48c283383520f010148554a0cb80c58997601868c514954369854d8594`，部署网页
`index.html` SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `23140` 及其 EXE/web 哈希只表示
上一阶段历史部署。

## BACKSET 原生 handler 收口

`formula_functions_reference.cpp` 中的 `BACKSET` 现精确遵循
`TCalc!sub_10017550`。handler 只跳过 leading source missing，并把首个有效 source 作为
回填下界；启动后遇到 missing 时，以有限原生 sentinel 参与信号判断并触发回填。source
先窄化为 float32，仅当 `abs(value) > 0.000009999999747378752F` 时命中，等于阈值不
命中。动态 N 每柱先转 float32，再按原生外部整数规则转 i32，并至少钳为 1；回填当前
到此前 `N-1` 柱，同时裁剪在首个有效 source 以内。BACKSET 仍仅允许显式只读 future
求值，不进入普通扫描或回测。

`native-operators` focused 已通过；正式 `sz/000001` day 5 样本为
`B=[0,1,1,0,0]`、`engine=native`。本批没有运行完整 CTest；130/130 是上一轮
sdk-json-4653 surface 阶段的结果。当前正式 PID `17676`；EXE SHA-256
`dc6bbe1ff93295db75a77518a8970f8d3260fd0ad0b6e7ee30af6a01ee060a1d`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `12148` 现仅代表上一阶段历史
部署。

## BARSCOUNT/DMA targeted handler 收口

依据 `output/ida-tcalc-barscount-dma-targeted-20260813.json`，`BARSCOUNT`
`sub_10006860` 现跳过且保留 leading sentinel，从首个有效 source 输出 float32 `0`；
启动后无需再次检查 source，每柱递增，内部 sentinel 不会暂停或重置计数。`DMA`
`sub_10018410` 则等待 X/A 首次共同非 sentinel，以 float32 X 播种状态。此后 X、A 和
previous state 均走 float32 落点；A 不做 `[0,1]` clamp，负值可外推；严格满足
`A+abs(A)*1e-7+1e-5>1` 时直接返回 X，否则执行
`previous*(1-A)+X*A`。内部 sentinel 保持原生有限 float 参与分支/递推，只有最终精确
sentinel 或非有限输出映射为 missing。

379 库审计中 BARSCOUNT 覆盖 13 条公式/18 次调用，DMA 覆盖 6 条/6 次调用。
`native-operators` 与 `language` focused 已通过；REF boundary fixture 用
`BARSCOUNT(CLOSE)+1` 明确维持旧偏移断言，BACKSET leading fixture 改为直接构造前导
missing 后验证 first-valid crop。正式 `sz/000001` day 5 返回
`C=[null,0,1,2,3]`、
`D=[11.1899995803833,11.239999771118164,11.25,11.25,11.229999542236328]`，
`engine=native`。

本批未运行完整 CTest；130/130 是上一 sdk-json-4653 阶段结果。当前正式 PID `9272`；
EXE SHA-256 为 `0388ce56c670078406e45def79a87f300e71b0621c9191638af954f140400063`，
部署网页 `index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `17676` 现为上一阶段历史部署。

## SLOPE targeted handler 收口

`output/ida-tcalc-slope-targeted-20260813.json` 直接记录 `sub_1000CDD0` 与 first-valid
helper `sub_10001280`。SLOPE 只跳过 leading source sentinel；动态 N 逐柱按
`int(float32(N)+0.503000020980835)` 取整，只有 `1<=N<=index-first+1` 才执行。窗口按
时间从旧到新对应回归位置 `1..N`；位置和/平方和、source/加权 source 和及两个均值
均在 handler 对应位置落为 float32，最终协方差分子与位置方差分母不另存 float32，
而是在 x87 宽精度中直接计算并相除，仅商写回结果时落为 float32。internal sentinel
仍为有限 float 数据；N=1 保留原生 singular 除法，经非有限输出边界成为 missing，
而非硬编码为 0。

库内只有 `ACCER`、`BSQJ` 使用 SLOPE，共 2 条公式/2 次调用；`WMA/FORCAST` 未改。
新增 source `[5941892,49958348,-9642243,1061.6112060546875]`、N=4 判别夹具，正确
结果为 `-7742307.5`，而错误的分子 float32 落点会得到 `-7742307.0`。
`native-operators`、`language` focused 通过。正式 `sz/000001` day 10、N=5 末柱为
`S=9.5367431640625E-07`、`normalized=8.507353044251431E-08`、`engine=native`。
本批未运行完整 CTest；130/130 属于上一 4653 阶段。

该 SLOPE 精度修正的中间部署 PID 为 `11332`；EXE SHA-256
`b79d085fe4ea93c5f56c825a3f202c32f831e2a9941c79136c18f2127494ce87`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `4380` 现为历史部署。

## ABS/MOD targeted handler 收口

`output/ida-tcalc-formula-gap-audit-20260813.json` 直接记录 `ABS sub_1001D9F0` 与
`MOD sub_1000C990`。ABS 以 leading canonical sentinel 建立启动边界；启动后精确
sentinel 仍原样保留，其他输入通过原生 float `fabs` 写回 float32。调用影响为 35 条
公式、61 次 ABS 调用。

MOD 逐柱拒绝任一精确 sentinel；随后对 dividend/divisor 分别执行
`int(float32(value)+0.503000020980835)`，再做 signed i32 remainder 并写回 float32，
所以余数保留 dividend 符号，而转换后的 divisor 为 0 时输出 missing。
`MOD(5.4,2)=1` 是基础判别例。调用影响为 3 条公式、6 次 MOD 调用。
`native-operators`、`language` focused 通过；正式 `sz/000001` day 5 的 2026-08-13
末柱 `CLOSE=11.210000038146973`，ABS 为同值，`MOD(CLOSE*10,7)=0`，
`engine=native`。本批未运行完整 CTest；130/130 属于上一 4653 阶段。

该 ABS/MOD 阶段正式 PID 为 `26364`；EXE SHA-256
`5284c5916b00cf158356c765154380280215675784069e6b416c4f56befc3258`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `11332` 是 SLOPE 精度修正的
中间部署。

## SQRT/INTPART targeted handler 收口

`output/ida-tcalc-sqrt-intpart-gap-audit-20260813.json` 直接记录
`SQRT sub_1001D820` 与 `INTPART sub_1001DD50`。SQRT 跳过 leading canonical
sentinel，并以原生 float 判定 `abs_f32(x)*1e-7+x+1e-5<=0`；命中时负值/internal
sentinel carry previous，未命中时执行 `sqrt(float32(x))` 并写回 float32。首个可用值
若命中 carry，则因没有 previous 而保持 missing。调用影响为 `BOLL-RB/HISV` 2 条
公式/2 次调用。

INTPART 只跳过 leading canonical sentinel，started internal sentinel 仍参与计算。
handler 以 `float32(x)-abs_f32(x)*1e-7-1e-5<0` 选取
`-0.000099999997/+0.000099999997` 调整量，随后安全截断为 i32 并写回 float32；非有限
或越界输入得到原生 `INT_MIN`。调用影响为 `WSBVOL` 1 条公式/2 次调用。
`native-operators`、`language` focused 通过；正式 `sz/000001` day 5 的 2026-08-13
末柱 `CLOSE=11.210000038146973`、`SQRT=3.3481338024139404`、`INTPART=11`、
`engine=native`。本批未运行完整 CTest；130/130 属于上一 4653 阶段。

上一 SQRT/INTPART 阶段正式 PID `24720`；EXE SHA-256
`9f45aa0a6754496873b7b29a754dd94bbed930c3014dda26b7bb5b5ee6b58e8c`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；PID `26364`
现为上一 ABS/MOD 阶段部署。

## MEMA/EXPMEMA targeted handler 收口

`output/ida-tcalc-mema-expmema-gap-audit-20260813.json` 直接记录
`MEMA sub_1000AEF0` 与 `EXPMEMA sub_1000B1A0`。两者仅使用末柱 N，经 float32 安全
截断为 i32；seed 区的 internal sentinel 前向填充上一 source，seed 累加、状态、递推和
输出均保留 float32 落点。MEMA 的 seed 门槛为严格 `first+N<count`，递推
`((N-1)*previous+X)/N`；EXPMEMA 为 `first+N<=count`，递推
`((N-1)*previous+2*X)/(N+1)`。seed 后 source sentinel carry previous，previous sentinel
则继续传播。调用影响为 MEMA 1 条公式/4 次、EXPMEMA 4 条公式/4 次。typed helper
接线完成后已删除无调用的旧 `exponential` helper，未改变 EMA/EXPMA 路径。

`native-operators`、`language` focused 通过。正式 `sz/000001` day 5 live 为
`E=11.229166030883789`、`M=11.235184669494629`、`engine=native`。本批未运行完整
CTest；130/130 仍只属于 sdk-json-4653 阶段。

上一 MEMA/EXPMEMA 阶段正式 PID `32380`；EXE SHA-256
`81b2845e4104e6fd22d79f3be288c99b92653189416b31f9f32b7c83ef054ca1`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `24720` 现为上一 SQRT/INTPART
阶段历史部署。

## AVEDEV/POW 与 SDK 1803/18031 fixed snapshot 收口

`output/ida-tcalc-next-scalar-series-targeted-20260813.json` 包含 AVEDEV、POW handler
的完整伪码与逐指令证据。AVEDEV 使用末柱 N、leading canonical sentinel gate，并复现
float32/wide 混合计算，影响 CCI 1 条公式/1 次调用；POW 使用 float32 operands、
mixed-wide gate 与 invalid carry，影响 BOLL-RB 1 条公式/1 次调用。`native-operators`、
`language` focused 通过；正式 `sz/000001` day 5 live 为
`D=0.015555699355900288`、`P=1.4884006977081299`、`engine=native`。

Level2 CLI-only formats `sdk-1803-host-projection`、`sdk-18031-host-projection` 分别要求
exact `32016`、`20012` 字节。projection 按 byte-for-byte 完整替换逻辑状态，只输出
prior/new replacement metadata、size 和 SHA-256；不读 previous state、不回显 body、
不字段化或变换。raw/hex 有界预读、decoded body `384 KiB` 上限与 exact-size 校验同时
生效；callback/SDK/storage/message/wire/network/request/subscription 全为 false/0。
`tdx-level2-sdk-fixed-snapshot-projection-tests` focused 已通过。SHA-256 仅在该 projection
TU 内实现，最终未扩展 common 公共接口。

上一 AVEDEV/POW + Level2 固定快照阶段正式 PID `30060`；EXE SHA-256
`cca7fdb7cace9a712b177a64322b0b1b9462025f12f7ef551b56074e9df7b91c`，部署网页
`index.html` SHA-256 为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `32380` 为上一 MEMA/EXPMEMA
阶段历史。本批未运行完整 CTest；130/130 仍归 sdk-json-4653 阶段。

## VALUEWHEN/BETWEEN targeted handler 收口

`output/ida-tcalc-next-scalar-series-targeted-20260813.json` 同时提供
`VALUEWHEN sub_1000B830`、`BETWEEN sub_1001DE70` 的完整证据。VALUEWHEN 仅以
condition 通过 leading gate；started 后 exact float32 zero carry，任意 nonzero（含
sentinel）选择 raw float32 selected value，selected sentinel 可覆盖 held。调用影响为
HANS123 1 条公式/2 次。BETWEEN 的 leading gate 只检查 bounds 是否同时为 sentinel，
value 不参与；started 后读取 raw float32，排序 bounds，并以 float32 `abs(value)` 执行
relative/absolute open-tolerance 判定。调用影响为 W106 1 条公式/1 次。

`native-operators`、`language` focused 通过。正式 `sz/000001` day 5 live source
`Q:=CLOSE>11.2;V:VALUEWHEN(Q,CLOSE);B:BETWEEN(CLOSE,11,11.3);` 末柱为
`V=11.229999542236328`、`B=1`、`engine=native`。当前正式 PID `32284`；EXE SHA-256
`76787fa34f2b53c563e8ebfa70a651bbcce05271ac9873d70b81377073fafacc`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `30060` 为上一 AVEDEV/POW +
Level2 固定快照阶段历史。本批仍未运行完整 CTest，130/130 仍归 sdk-json-4653 阶段。

## REFX/REFXV targeted handler 收口

`output/ida-tcalc-refx-limit-price-targeted-20260813.json` 完整记录
`REFX sub_10019F90` 与 `REFXV sub_1001A300` 的伪码、逐指令和直接 helper。
两者均以 source/offset 同时非 canonical sentinel 结束 dual-leading gate；started
后使用 raw float32 operands/source，保留 offset `fabs`、target float32 landing 及
wide tolerance 比较顺序，再安全 trunc 为 i32 取值。无效 REFX 返回 missing；
无效 REFXV 在 index 0 返回当柱 raw source，否则 carry previous raw output。

379 库共影响 `NXTS/WAVEKX/SQJZ/ICHIMOKU` 4 条公式/15 次调用，分别为
2/4/8/1 次。`native-operators`、`language` focused 均通过，language 旧反向/
夹取预期已纠正。正式 `sz/000001` day 5 live 为
`R=[11.260000228881836,11.25,11.229999542236328,null,null]`、
`V=[11.260000228881836,11.25,11.229999542236328,11.229999542236328,11.229999542236328]`、
`engine=native`。两者仍只能通过 `allow_future=true` 的
`explicit-read-only-lookahead` 执行。

最终正式 PID `26104`；EXE SHA-256
`8c8c893ed3d652756361199861e751db906b0a95348e30913f72992adf7ffd4e`，部署网页
`index.html` SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `32284` 为上一 VALUEWHEN/BETWEEN
阶段历史。本批未运行完整 CTest；130/130 仍归 sdk-json-4653 阶段。

## LAST/NDAY 与 limit-price host-context fidelity gate

`output/ida-tcalc-last-nday-targeted-20260813.json` 给出
`LAST sub_10005970` 的 210 条指令和 `NDAY sub_100165F0` 的 177 条指令及完整直接
helper。两者均只从末柱取得整数参数并先落为 float32。LAST 只做 source leading
sentinel gate，`A=0` 采用全历史起点，窗口零值使用严格 float32 epsilon，started
sentinel 按 raw 非零值参与。NDAY 采用双 operand leading gate、float32 magnitude 的
relative/absolute tolerance 与连续计数状态机，started 后 sentinel 也进入原生比较。
实际影响为 `XRDS/QTDS` 2 formulas / 2 LAST calls，以及 `K300/K310` 2 formulas /
2 NDAY calls。

`output/ida-tdxw-type120-security-class-targeted-20260813.json` 和
`output/ida-tdxw-security-record-precision-displacements-20260813.json` 明确区分了两类
字段：TNF `+76 price_precision` 不能替代 ZTPRICE/DTPRICE 所需的 `runtime+283`、
`sub_5A3810`、`type120+31` 证券分类上下文。`B007/C128/C129/C130` 仍为 executable
approximation，但其 numeric fidelity 为 degraded 且 `pure_ohlcv=false`。379 库统计为
375 numeric-safe / 4 degraded；scan/backtest 拒绝 degraded 输出，嵌套 surrogate cause
递归保留，例如 RGB 与 ZTPRICE 原因会同时到达最终输出。

formula target build 通过，focused `native-operators`、`context-and-library`、`language`
均通过；临时与正式 focused API `health/evaluate/nested/scan/backtest` 均通过。正式
`sz/000001` day 40 末柱为 `L=1`、`D=0`，nested causes 为 `RGB + ZTPRICE`。未运行完整
CTest 或 full API。当前正式 PID `24556`；EXE SHA-256
`39483c732fa9a317cfc4b4674193c93a034f3703dc857508c5b61a63516bb36a`，网页 SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `26104` 及 EXE
`8c8c893ed3d652756361199861e751db906b0a95348e30913f72992adf7ffd4e` 为历史，PID
`32284` 为更早历史。回滚副本：
`output/tdx-tool-refx-pre-last-nday-zd-gate-predeploy-rollback-20260813.exe`。

## UPNDAY/DOWNNDAY targeted handler 收口

`output/ida-tcalc-upnday-downnday-targeted-20260813.json` 给出 UPNDAY
`0x10016330`（135 instructions）与 DOWNNDAY `0x10016490`（134 instructions）的完整
证据。两个 handler 均采用 final N float32 trunc、source-only leading gate，从
`first+N-1` 初始化零输出并从 `first+1` 开始比较；严格 tolerance 驱动连续 run，
`run==N` 后回退 `N-1`，started sentinel 按 raw float32 参与。系统库调用为 UPNDAY
`UPN/K300` 2 formulas / 2 calls、DOWNNDAY `DOWNN/K310` 2 formulas / 2 calls。

formula target 与 focused `native-operators/language` 通过；正式 `sz/000001` day 800
末柱 `U=0`、`D=1`、`K=0`。未运行 full CTest/full API。当前正式 PID `17880`，EXE
SHA-256 `7f70d91d456598bfc6d3ecef3318e7fbebd162dac612a9d31efef06f73b5f05e`，web SHA-256
仍为 `71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `24556` 与 EXE
`39483c732fa9a317cfc4b4674193c93a034f3703dc857508c5b61a63516bb36a` 为历史。回滚副本：
`output/tdx-tool-last-nday-zd-gate-pre-upnday-downnday-predeploy-rollback-20260813.exe`。

## STD/LOWRANGE targeted 复核与 1803/18031 typed-domain projection

`output/ida_probe_tcalc_std_lowrange_20260813.py` 与
`output/ida-tcalc-std-lowrange-targeted-20260813.json` 已完整复核
`STD 0x1001E850`/110 instructions、`VAR 0x1000D5E0` 与
`sqrt 0x1001E800`。STD 现有主路径已匹配，未改代码。`LOWRANGE 0x100072D0`/
144 instructions 以 source-only leading gate 启动，固定当柱 raw float32 值从
最新 prior 向 0 回扫，对 prior 同样取 raw float32，严格以
`current-prior < -1e-5` 继续计数。started internal sentinel 与回扫的
leading sentinel 都作 raw 值，count 以 float32 写回。影响为 `YYD` 1 call、
`W107` 3 calls，合计 2 formulas / 4 calls。

Level2 新增两个未接 CLI/HTTP/UI 的 typed domain。`1803` 的 exact
32016 B body 按两侧独立 raw count clamp 1000，生成 13 B
`f32 price/u32 volume/zero/u16 auxiliary/rank/zero` 记录；price 由 f64 缩窄，
unsigned volume 在每侧独立做 stable top-3，保留原生 `-1/0/0` fallback，
输出只称 `first/second` raw。`18031` 要求 exact 20012 B，count clamp
5000，复用 security classifier；`sub_594680` predicate 为真时 u32 quantity
`/10`，否则 `/1`，包装为 6 B `zero u16 + quantity`，保留
`first_raw` 与 `second_output_raw=0`。两者均不回显 body，SDK/callback/
message/storage/request/subscription/network 均不发生。

formula target build 及 focused `native-operators/language` 通过；`sz/000001`
day 800 末七点 `R=[5,0,8,0,1,2,0]`，正式 API 40 点页面样本一致。
两个新 Level2 test target 及既有 1801/1802 classifier regression 通过。未运行
full CTest/full API。当前正式 PID `24140`，EXE SHA-256
`8571ce09ac27110defa68ed1727a3ed14ce4b834b3c1c944f26eb7ea022cdd7b`，web SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `17880` / EXE
`7f70d91d456598bfc6d3ecef3318e7fbebd162dac612a9d31efef06f73b5f05e` 为历史。
回滚副本：
`output/tdx-tool-directional-nday-pre-lowrange-level2-projections-predeploy-rollback-20260813.exe`。

## SAR/INCLUDED/BARSNEXT/REFDATE 与 Level2 双快照最终收口

本批解释器证据为
`output/ida-tcalc-sar-sarturn-targeted-20260813.json`、
`output/ida-tcalc-included-includedv-targeted-20260813.json`、
`output/ida-tcalc-barsnext-zig-targeted-20260813.json` 和
`output/ida-tcalc-if-datetoday-refdate-targeted-20260813.json`。`SAR/SARTURN` 现使用
末柱参数、float32 状态与原生容差/反转；`INCLUDED/INCLUDEDV` 使用末柱
selector/limit、排除 self、跳过已命中候选和 backward/forward 扫描；`BARSNEXT`
以 raw-f32 inclusive `±9.999999747e-6` 判真并保留末个真值右侧 missing；`REFDATE`
以末柱 raw-f32 target 截整并广播 raw-f32 source。影响分别为 SAR 3/4、SARTURN
2/2、INCLUDED/INCLUDEDV 的 WAVEKX、BARSNEXT 1/3、REFDATE 1/2。所有 future
能力仍只是 `allow_future=true` 的只读 lookahead。

1803 depth-record 与 18031 queue-record 已通过 POST `/api/v1/level2/project` 和
Level2Lab 暴露 exact-size、inline-hex 的离线投影；不回显 body、不调用 SDK、
不发送消息或网络请求。新 CLI-only `sdk-4654-dual-snapshot-transition` 与
`sdk-4651-dual-snapshot-transition` 则依据
`output/ida-tpbus-sdk-4654-4651-targeted-20260813.json` 接受显式 decoded/raw 文件。
4654 的 decoded 必须 exact 48 B；4651 先校验 decoded `+2 == 0xffffffff` 的 caller
gate，再按 previous raw logical size 替换或保留。它们只输出大小、SHA-256、ready
和决策，不执行 raw-to-decoded 转换，没有 HTTP/UI，也不调用 SDK/宿主存储/网络。
retained state 的严格九字段连续 CLI replay 已加入聚焦测试并通过。

单线程完整构建通过；CTest `135/135`、0 failed、`80.89 s`；Svelte check
0 errors / 0 warnings。本批未运行 full API suite。正式 PID `36100`，EXE SHA-256
`a5fb392bf25fe6f2d224bc681212ca4049ad6364391b40b8f5730f311e07da0a`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health
匹配，`native_cpp=true`、`python_runtime=false`。PID `33268` / EXE
`69ac767a2d9cea3bc53899a92d6deed8df89f8c125582bc766b7ff56658e0db0` 为历史。
回滚副本为
`output/tdx-tool-formal-69ac767a-pre-barsnext-refdate-465x-rollback-20260813.exe`。

## ZIG 负阈值与 SDK 4655 companion/raw transition

ZIG 仅在现有主算法前补齐原生阈值 gate：读取末柱 raw 值、收窄到 float32，并按
`t-abs(t)*1e-7-1e-5 < 0` 对负值/无效值早退全零。正阈值路径、selector 与主状态机
没有变化。focused 及正式 day/7 判别结果为负阈值全 0、正阈值末柱 `11.25`，仍明确
标记 `explicit-read-only-lookahead`。

`output/ida-tpbus-sdk-4655-downstream-targeted-20260813.json` 固定了 4655 的输入边界：
caller 从 prior local state 克隆 exact 46 B companion，raw 必须已由 dispatcher
qualified，exact shape 为 `39 + 18*signed_i16_count + 120*signed_i8_attach`；
`count=-1/attach=1/size=141` 合法。CLI-only
`sdk-4655-companion-raw-transition` 保留 unresolved host gate，只产生 conditional
candidate 与 companion/raw/attach SHA-256 摘要，不回显 body、不作 raw conversion，
也不调用 SDK/callback、host message 或 network。它没有 HTTP/UI，并未修改
`sdk-json-4655` 的既有契约。

focused targets 已通过；完整单线程构建与 CTest `136/136`、0 failed、`88.96 s`；
Svelte 沿用本轮 0 errors / 0 warnings。本批未跑 full API。正式 PID `17824`，EXE
SHA-256 `8623ec5d1326cd0dbdb901acab849fc5ee5fd6986549a1ae74c4ee757b9567ed`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 为
`native_cpp=true`、`python_runtime=false`。PID `36100` / EXE
`a5fb392bf25fe6f2d224bc681212ca4049ad6364391b40b8f5730f311e07da0a` 已转为历史。
回滚副本 `output/tdx-tool-formal-a5fb392b-pre-zig-4655-rollback-20260813.exe` 的 SHA-256
同为 `a5fb392bf25fe6f2d224bc681212ca4049ad6364391b40b8f5730f311e07da0a`。

## ZIG selector last-10 raw-f32 更正

ZIG 现只检查末端最多 10 组相邻 raw-f32 值，以固定 `1e-5` 判定 selector；不再要求
整段序列恒定。OHLC mapping 与正阈值主状态机未改。focused `native-operators`、
`language` 通过；正式 day/12 `X=[99,3×11]` 的 ZIG 与 CLOSE selector 数组完全相等，
仍标记 `explicit-read-only-lookahead`。本阶段未重新执行 full CTest，文档中的
`136/136` 是上一 4655 阶段结果。

4655 targeted evidence 已改为 compact-v2（`213228 B`、7 functions），旧 wide evidence
另存 targeted-wide；这只是证据收窄，production 语义不变。正式 PID `25764`，EXE
SHA-256 `0e2b58844f822ea526e57f944484dad6ecac15d7c0994f925f3f2f60985fe694`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `17824` / hash
`8623ec5d1326cd0dbdb901acab849fc5ee5fd6986549a1ae74c4ee757b9567ed` 为历史。回滚
`output/tdx-tool-formal-8623ec5d-pre-zig-selector-rollback-20260813.exe`，SHA-256
`8623ec5d1326cd0dbdb901acab849fc5ee5fd6986549a1ae74c4ee757b9567ed`。

## BARSSINCE raw-f32 与 SDK 4650 未实现边界

依据 `output/ida-tcalc-tr-barssince-targeted-20260813.json`，BARSSINCE 的 first scan
现以 raw-f32 operand 工作，仅 canonical sentinel 与 exact `±0.0f` 被跳过；started 后
计数路径不变。`1e-50` 下溢样本全 missing，leading sentinel 后 `1e-8` 样本输出
`0..4`。NXTS 是 379 库唯一消费者，共 1 formula / 5 direct calls。

4650 targeted script/JSON 为 `output/ida_probe_tpbus_4650_targeted_20260813.py` 及对应
输出。raw shape 可以闭合，但 host code/market、prior state、time、`+408/+804/+72/
+496/+80` 对象状态仍未解决，故本轮明确不实现 shape-only projection。随后
`output/ida-tpbus-sdk-4650-previous-state-targeted-20260813.json` 已闭合指定四地址、
22 个完整函数和 58 个关键 offset xref；结果确认五字段仍不足，完整 typed 输入会退化为
宽泛宿主对象镜像并需要 live/server time policy，因此仍维持 do-not-implement。

focused native-operators/language 通过；未运行 full CTest（`136/136` 仍归上一 4655
阶段）或 full API。正式 PID `35408`，EXE SHA-256
`47ca2f7ea97c4a88a3a7904ddf496bf4c470428f2ca70c1e6413992b3c21a6b7`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `25764` / hash
`0e2b58844f822ea526e57f944484dad6ecac15d7c0994f925f3f2f60985fe694` 为历史。回滚
`output/tdx-tool-formal-0e2b5884-pre-barssince-rollback-20260813.exe` 的 SHA-256 同为
`0e2b58844f822ea526e57f944484dad6ecac15d7c0994f925f3f2f60985fe694`。

## MA 与 BARSLAST family 的 raw-f32 启动边界

`output/ida-tcalc-ma-targeted-20260813.json` 完整闭合 `sub_1000AD00`。MA 的动态 N 现
按每柱 raw-f32 后截整，`2.99999999 -> 3`；仅该 typed helper 移除通用
`period_at` 的 double/1,000,000 行为。source first gate 和窗口跳过均识别 canonical
sentinel，current-to-past running sum 与输出仍逐步落 float32并固定除 N。

相邻证据 `output/ida-tcalc-barslast-targeted-20260813.json`（126 insns）和
`output/ida-tcalc-barslastcount-targeted-20260813.json`（249 insns）确认两者主体已对齐；
本批只让 leading finite sentinel 保持 missing，不改变 BARSLAST 的 post-start reset/count
或 BARSLASTCOUNT 的 near-one/zero 回扫。覆盖为 MA 125、BARSLAST 12、
BARSLASTCOUNT 5 个系统公式。

native-operators/language 均通过；真实 day/800 与三个正式 HTTP focused 合约通过，HTTP
数组分别为 MA `[null,null,11.246665954589844,...]`、BARSLAST
`[null,null,0,1,2]`、BARSLASTCOUNT `[null,1,2,0,0]`，均为 `native-cpp` 且请求体不保留。
本批未跑 full CTest/full API，`136/136` 仍归上一 4655 阶段；Level2 4650 边界未改。

正式 PID `34664`，EXE SHA-256
`b6e914030227e508e9234846173540b6e981f215a04801094a32908ebd48f34e`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `35408` / hash
`47ca2f7ea97c4a88a3a7904ddf496bf4c470428f2ca70c1e6413992b3c21a6b7` 为历史。回滚
`output/tdx-tool-formal-47ca2f7e-pre-ma-barslastcount-rollback-20260813.exe`，SHA-256 同为
`47ca2f7ea97c4a88a3a7904ddf496bf4c470428f2ca70c1e6413992b3c21a6b7`。

## REF 双 operand raw-f32 gate 与 SDK 4671 边界

`output/ida-tcalc-ref-targeted-20260813.json` 完整闭合 `sub_10019DF0`：REF 只在 source 与
offset 都不是 canonical raw-f32 sentinel 时启动；offset 使用原生相对/绝对容差和
float-to-int 截整，非法值 carry 前一 raw output，选中的 source sentinel 输出 missing。
覆盖为 131 个系统公式、554 次调用。native-operators/language、真实 day/5 和三条正式
HTTP focused 合约均通过；普通结果为
`[null,11.1899995803833,11.289999961853,11.2600002288818,11.25]`，另外两条锁定
source sentinel 与 offset sentinel carry，HTTP body 不保留。

4671 targeted script/JSON 闭合 `sub_1006764B` 22 条指令及两个 vector helper：请求侧只
复制宿主已缓存的 opaque vector，callback dispatcher 无 4671 专用状态分支。故没有新增
identity-copy domain/CLI，也没有改 server/web。本批未跑 full CTest/full API；历史
`136/136` 仍归 4655。

正式 PID `25428`，EXE SHA-256
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `34664` / hash
`b6e914030227e508e9234846173540b6e981f215a04801094a32908ebd48f34e` 为历史。回滚
`output/tdx-tool-formal-b6e91403-pre-ref-rollback-20260813.exe`，SHA-256 同为旧 hash。

## 连板天梯 Web surface

`/data/limit-ladder` 已接现有 GET `/api/v1/market/limit-ladder`，不改变 native schema。
页面覆盖 category/activity/sort/order/q/offset/limit，按后端分页呈现封板、炸板、连板与
高度，并显式对照 upstream advancement rate 和本地复算；不显示 raw、不制造 live quote。
本地成员跳转只在 identity 完整时启用，SectorBrowser 新增原生已有的
`research-industry` family，避免研究行业被错误降格。

Svelte check 为 0/0，Web build 通过（仅既有 chunk warning）；五条 focused GET 验证
248 条 live source、industry/concept/activity、分页和非法 category=400，formula
mismatch=0。本批无 C++ 变更，未跑 full CTest/full API。

正式 PID `22616`，EXE SHA-256
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256
`da7da0814cb7745e9ab52537ad80c7e84b3cfc0cd033c5c0af8af761247558dd`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `25428` / 旧 web hash `40726ee7...62eeb`
为上一 REF 阶段。

## 开盘与盘后成交 Web surface

`/data/session-turnover` 已接现有 GET `/api/v1/market/session-turnover`，不改变 native v1
schema。页面覆盖 universe/activity/sort/order/market/q/offset/limit，严格使用服务端排序
和分页；展示统计日价格涨跌、总成交、开盘与盘后成交额及占比，不展示 raw，也不制造
实时行情、市值或行业字段。证券跳转只允许受支持市场和六位代码。

Svelte check 为 0/0，Web build 通过（仅既有 chunk warning）；五条 focused GET 验证
A 股 5,515 行、ETF 1,624 行、双活跃 3,838 行、`SZ000001` 单票和非法 universe=400。
本批无 C++ 变更，未跑 full CTest/full API。

正式 PID `34564`，EXE SHA-256
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256
`2bec35c186633960ef0d00e0ca2142aa9b1cd54b5d0e792f0268a5a3061464be`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `22616` / web hash `da7da081...558dd`
为上一连板天梯阶段。

## 资金信号后续表现 Web surface

`/data/flow-followup` 已接现有 GET `/api/v1/market/flow-followup`，不改变 native v2 schema。
页面联合呈现 margin/northbound 两组逐日历史和四组 model，支持日期、可用性与返回数控制；
不回显 raw/upstream text。历史与分档后续表现均标记为相关性研究，不是预测或投资信号；
北向 `1040/0` 占位段和 current pair 显式标为不可用。

Svelte check 为 0/0，Web build 通过（仅既有 chunk warning）；focused 合约得到历史
2,122/2,237 行、占位 466 行，四模型 11/11/16/16 档，并锁 current signal=false 和
非法模型过滤 400。正式重启后一次 WinHTTP 12030 经短重试恢复 live。本批无 C++ 变更，
未跑 full CTest/full API。

正式 PID `20508`，EXE SHA-256
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256
`1929f3d7cb4cabeda51998e8f512512f82ac995ab1071800fd43f93d6992f371`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `34564` / web hash `2bec35c1...1464be`
为上一成交页面阶段。

## 1803 / 18031 record projection CLI

`level2 project` 新增 `sdk-1803-depth-record-projection` 与
`sdk-18031-queue-record-projection` 两个 CLI format，薄层复用已有 typed domain。前者要求
exact 32,016 B raw/hex；后者要求 exact 20,012 B raw/hex 和显式 market 0..2 / 六位 code。
输入 raw 上限 384 KiB、hex 文本上限 768 KiB，未知参数、路径外协议选项和错误 identity
均拒绝；输出继续保持 body-retained=false、SDK/callback/message/network 全零。

两个 affected test executable 和 `tdx-tool` 构建通过；测试实际覆盖 raw/hex CLI、packed
record、大小、必填 identity 与严格参数。完整 CTest `136/136`、0 failed、72.77 秒；正式
help smoke 通过。HTTP/Level2Lab 原有格式与 schema 未改，未跑 full API。

正式 PID `23872`，EXE SHA-256
`4ece5208d192711327888a29879a07a41d7889c42e37d8fc97dbf14642d5d16b`，web SHA-256
`1929f3d7cb4cabeda51998e8f512512f82ac995ab1071800fd43f93d6992f371`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `20508` / EXE `6c4d7c53...f3bc2` 为历史；
rollback 为 `output/tdx-tool-formal-6c4d7c53-pre-record-cli-rollback-20260813.exe`。

## fnReqData 单证券计划 POST / Level2Lab

已有 `build_level2_sdk_fnreqdata_call_plan` 与
`build_level2_sdk_fnreqdata_18031_call_plan` 现通过 POST `/api/v1/level2/build` 和
Level2Lab 暴露。HTTP 使用精确 flat whitelist：普通计划为 format/market/code/data_type，
1801/1802 另需 cursor_raw/count，固定体类型禁止这两个 override；18031 为
format/market/code/side_mode_raw/selected_price。所有结果继续保持 12 ABI slots、
SDK invoked=false、wire/request/subscription=false、input_retained/file_accessed=false。

`tdx-server-level2-tests`、`tdx-server-catalog-tests`、Svelte 0/0、Web build、完整 build 与
CTest `136/136`（66.86 秒）通过。正式 smoke 覆盖 u32 max、fixed 0/1、18031 f64 bits
`0x4024800000000000`、path 与 fixed override 拒绝。

正式 PID `10404`，EXE SHA-256
`90d95a18162d36441d36d4591eb95f25b5d146e94c4cd72647892f77f59f9b13`，web SHA-256
`1edf97c14f8b17d5e5ef2989709ba47d652a2a847c597edc50a56f46daf91cd2`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `23872` 为上一 CLI 阶段；rollback：
`output/tdx-tool-formal-4ece5208-pre-fnreqdata-surface-rollback-20260813.exe`。

## ZIG 百分比原生状态机

`ZIG(X,N)` 的正阈值路径现直接复现 `sub_100227A0`：输入、阈值和 selector 以 raw float32
进入；状态机按相邻局部极值、百分比反转阈值及 `1e-7/1e-5` 容差维护带符号候选；输出
按原生 float32 slope 回画，并保留未确认候选后再绘制最后一段。负值、missing 和 float
overflow 阈值仍返回 DLL 先清空的全零向量。绝对价 `ZIGA` 的独立实现和 schema 未改。

native-operators、language、完整 formula-engine tests、完整单线程 build 和 CTest `136/136`
（65.96 秒）通过。正式 `WAVE`/`NXTS` 40 根日线执行通过；本批无 HTTP/Web schema 变化，
未跑 full API。

正式 PID `464`，EXE SHA-256
`cfb8ed2317545c67509b0477bd3b346f72f20954f17c2bc537b360775b042def`，web SHA-256
`1edf97c14f8b17d5e5ef2989709ba47d652a2a847c597edc50a56f46daf91cd2`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `10404` 为上一阶段；rollback：
`output/tdx-tool-formal-90d95a18-pre-zig-native-state-rollback-20260813.exe`。

## 期权工作台与 root-relative 资源来源

Web 新增 `/data/options`，直接消费既有期权目录、到期日、期权链和波动率接口；支持市场/标的/
合约/看涨看跌筛选，以及合约链、ATM、IV/Greeks 和到期详情。页面没有路径、文件或上传入口。

`server_market_realtime` 现对 expiry、volatility、chain 的规则/节假日来源做统一 root-relative
投影，根外路径直接拒绝。公开响应声明 `path_scope=tdx-root-relative`；真实 DCE A2609 响应仅含
`T0002/hq_cache/code2name_qq.ini` / `T0002/hq_cache/neednote.dat`，不含正式根绝对路径。

`tdx-server-option-surface-tests` 与 `tdx-options-tests` 通过；Svelte 0/0、Web build 437 modules、
完整 build 与 CTest `137/137`（65.45 秒）通过。正式 smoke 得到 catalog 62、chain 31 strikes /
62 quoted / 62 calculated IV、A2609-C-3400 expiry `2026-08-18`、历史/隐含波动率
`11.959512031837638%` / `0.01%`；未跑 full API suite。

正式 PID `33236`，EXE SHA-256
`cd1793637f52345a43a5978c3e40020848cc7af8321159d95d3d4884b8464cce`，web SHA-256
`40cf58f4cfd5e942404ff98454464be0b444859f05e572f7b373f9ac9e0e8aed`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `464` 为上一 ZIG 阶段；rollback：
`output/tdx-tool-formal-cfb8ed23-pre-option-surface-rollback-20260813.exe`。

## 本地参考档案与 fund-reference 兼容边界

`/data/local-reference` 现统一消费 `/api/v1/market/historical-securities`、`index-events` 和
`fund-reference`。服务端抽出独立 `server_local_resource_paths` 投影层，把三类响应的
`sources[].path` 和 `local-file:` endpoint 转为 TDX-root-relative，并对根外来源直接报错；
公开响应声明 `path_scope=tdx-root-relative`，不回显正式安装根。

基金解析按 `output/ida-tdxw-spec-fund-loaders.json` 的 `sub_4F4A50` 对齐：ETF 起止日期允许
为空或单侧为空；只有两端都存在时才派生 lifecycle。空值输出 null，非空值仍做八位日期校验。
正式目录返回 4218 条，其中 snapshots 2115、ETF 1672、LOF 431、reference instruments 1988、
`native_status_unset=2`；`SZ159025` 的空日期样本不再令 endpoint 返回 400。

`tdx-fund-reference-tests`、`tdx-historical-securities-tests`、`tdx-index-events-tests`、
`tdx-server-local-resource-surface-tests` 和 catalog tests 通过；Svelte 0/0、Web build 439 modules、
完整 build 与 CTest `138/138`（测试耗时合计 63.33 秒）通过。正式 smoke 覆盖三个 schema、
root-relative 来源、无根泄漏、零网络、非法七位 `as_of_date` 返回 400；未跑 full API suite。

正式 PID `26472`，EXE SHA-256
`abbff3821c137e65e08630aa07373325530dd10f1605f040bb3e203fde4f131d`，web SHA-256
`b907273787fe625d39ada3519218e259f84fcc6209f356ea76596e979c5d2abc`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `33236` 为上一期权阶段；rollback：
`output/tdx-tool-formal-cd179363-pre-local-reference-rollback-20260813.exe`，SHA-256
`cd1793637f52345a43a5978c3e40020848cc7af8321159d95d3d4884b8464cce`。

## 港股本地档案与 singular source 安全投影

`/api/v1/market/hk-actions` 与 `/api/v1/market/hk-finance` 现共用
`server_local_resource_paths`：投影器既支持 `sources[]`，也支持财务接口的 singular `source`，
把 `path` 与 `local-file:` endpoint 转为 `/` 分隔的 TDX-root-relative 值，并拒绝根外来源。
catalog/OpenAPI 同步声明此路径边界。Web 新增 `/data/hk-reference`，只发送既有业务筛选参数，
不增加文件、路径、URL 或上传入口；公司行动保留复权倍率/加项，财务保持报告币种和未命名的
原生币种折算码边界。

正式根返回公司行动 32400 条、2438 只证券、`19730713..20261210`，财务 3238 条、32 个
分类码、`20050731..20260630`。`HK00001` 为 51 条行动与 1 条财务；三个来源均为
`T0002/hq_cache/...`，无绝对根，`network_requests=0`。focused 四目标、Svelte 0/0、Web build
441 modules、完整 build 与 CTest `138/138`（64.14 秒）通过；正式页面/两条成功查询/非法代码
400 合约通过，未跑 full API suite。

正式 PID `11040`，EXE SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`，web SHA-256
`fbd587b6d028419287e6d070af50968795a1b49f7d9d45524104a70b1e43b5ba`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `26472` 为上一阶段；rollback：
`output/tdx-tool-formal-abbff382-pre-hk-local-reference-rollback-20260813.exe`，SHA-256
`abbff3821c137e65e08630aa07373325530dd10f1605f040bb3e203fde4f131d`。

## 路演 Web surface 与 MA 静态闭包

既有 `tdx-roadshows-native-v1` 现由 `/data/roadshows` 与个股“公司路演”面板消费；未新增协议或
任意 TQLEX 输入。正式主表 2511 upstream / 2495 normalized，业绩说明会 2391，平安银行
`ly:0_000001` 为 38 条；两个页面 200，market/code 不成对返回 400。roadshows 与 recon focused
tests、Svelte 0/0、Web build 445 modules 通过；本批 C++ production 未变，因此未重跑 full
CTest，上一阶段仍为 `138/138`。

`output/ida-tcalc-ma-targeted-20260813.json` 闭合 MA 187 instructions + ftol2 55、frontier 0；
现 `tcalc_moving_average` 与 handler 的动态周期、sentinel、倒序 f32 累加及除法一致，未改解释器。

正式 PID `5760`，EXE SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`，web SHA-256
`860818fd92289d42134ef08b59cd0142a41b7e94b7b14ec3ee16035f01d54b2b`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `11040` 为上一阶段；executable rollback 仍为
`output/tdx-tool-formal-abbff382-pre-hk-local-reference-rollback-20260813.exe`。

## 扩展市场行情 Web surface

`/data/expansion-market` 现消费既有 7727 合约目录、150B 行情快照、18B 分时和 16B 逐笔解析结果；
没有新增 native transport 或 parser schema。Web 严格限制市场、1..9 位扩展代码、YYYYMMDD 与分页，
并明确 instrument market/query 仅过滤已抓取窗口。正式目录 total=143900；`29:A2609` 快照五档完整，
`20260805` 分时 345 点、逐笔 focused 5 条，A 股市场被 expansion endpoint 以 400 拒绝。

`tdx-native-tests`、Svelte 0/0 与 Web build 447 modules 通过；本批没有 C++ production 变更，未重跑
full CTest/full API，上一阶段仍为 138/138。当前正式 PID `17772`，EXE SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`，web SHA-256
`7ddbaba11960804a4b51e107ca955e70ded526aeb4b277fb9eab57632bb60ef6`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `5760` 为上一阶段；executable rollback 不变。

## 4654 / 4651 / 4655 project surface

既有 `sdk-4654-dual-snapshot-transition`、`sdk-4651-dual-snapshot-transition` 与
`sdk-4655-companion-raw-transition` 现由固定 `POST /api/v1/level2/project` 和 Level2Lab 消费。
4654 要求精确 48 B decoded companion；4651 要求 decoded `+2` 的 little-endian u32 为
`0xffffffff`，并接受 domain 原样产出的严格九字段 previous state；4655 要求精确 46 B
caller-cloned companion 和 dispatcher-qualified raw。4655 只输出 unresolved host gate 下的条件候选，
不声称 actual host replacement。

server orchestration 只接受内联 hex 与 format 对应字段，整体 JSON 请求不超过 384 KiB；拒绝 path、
URL、file、upload、server、token、handle、endpoint 和跨格式字段，不回显 companion/decoded/raw body。
公开结果保持 file/SDK/callback/message/network/subscription/authorization side effects 全为 false/0。

三个 domain focused executables、server project 与 catalog focused tests 通过；Svelte 0/0、Web build
447 modules、完整 build 与 CTest `138/138`（0 failed，66.59 秒）通过。正式 POST 锁定 4654 replace、
4651 replace→retain round-trip、4655 unresolved host gate、非法 path=400，并验证 features/OpenAPI 与
Level2 页面；未跑 full API suite。

正式 PID `33660`，EXE SHA-256
`75e11ac73bb68e1190bde4a953e8acba10da393c6987d2fb9eb53145590ae5fb`，web SHA-256
`71a3bc22a8999cb638e8c6e0f520a5d6f521cb6c6aa2ab68f5af6bcb27bb8f39`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `17772` 与 `6622e99e...918f8` 为上一阶段；rollback：
`output/tdx-tool-formal-6622e99e-pre-level2-465x-surface-rollback-20260813.exe`，SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`。

## 0x053E Web consumer

`/api/v1/market/speed` 的现有 `tdx-market-speed-native-v1` 现由个股“涨速盘口”页签消费；没有修改
decoder、transport 或公开 schema。Web 类型覆盖服务端涨速、五档、OHLCV、transport 和未命名 raw
尾字段，后者不推断语义。页面使用固定 market/code query、可见时 5 秒刷新，不提供 endpoint、host、
路径、文件或任意 request body。

`tdx-native-tests`、Svelte 0/0 与 Web build 449 modules 通过；正式 `SZ000001` 为 1/1 记录、买卖
各 5 档、connection_attempts=1，新页签 200。本批无 C++ production/API schema 变更，未重跑 full
CTest/full API；上一完整基线仍为 138/138。

正式 PID `12628`，EXE SHA-256
`75e11ac73bb68e1190bde4a953e8acba10da393c6987d2fb9eb53145590ae5fb`，web SHA-256
`9bcb7d1e038a9b830446d22f32a6aeba58f210f414f01c9a3e532eb2c8be7e01`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `33660` 为上一阶段；executable rollback 不变。

## TOPRANGE / sub_10007170

`output/ida-tcalc-toprange-targeted-20260813.json`（22473 B，SHA-256
`0814473bf24c388b6f7cf1886159bc931d802a022746edd6717d5f71bb33f49b`）完整包含 TOPRANGE 的
144 条指令与伪码，direct callee 为 0。新的私有 helper 复现 source-only leading sentinel gate、
started raw-f32 current/prior、向过去回扫和严格 `>9.999999747e-6`；不再因内部 missing 停止，
也不再用 double 输入比较。LOWRANGE 未动。

native-operators 与 language focused 通过；完整 build 与 CTest `138/138`（0 failed，66.76 秒）
通过。正式两组判别向量为 `[0,1,0,3]` 和 `[null,null,2,3,4]`。379 系统源码调用数为 0，
影响边界为 custom/user formulas；未跑 full API suite。

正式 PID `20808`，EXE SHA-256
`cf4da92cf849ad4cecdce5a15ebb635c53c561db29bba768c00da472eec35a12`，web SHA-256
`9bcb7d1e038a9b830446d22f32a6aeba58f210f414f01c9a3e532eb2c8be7e01`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `12628` 为上一阶段；rollback：
`output/tdx-tool-formal-75e11ac7-pre-toprange-native-rollback-20260813.exe`。

## 行情流状态 Web consumer

既有 `/api/v1/market/stream/status` 现在由系统页消费。前端类型逐项覆盖 Hub 的 subscriber/security
计数、poll/reconnect counters、last error/source/success、interval/backoff/session throttle 与
`fast_hq_boundary`；页面每 5 秒只读刷新该固定 GET。它不会新增订阅、调用 FastHQ、改变 7709
连接策略，或开放 endpoint/path/file/raw request 输入。

`tdx-market-stream-tests`、Svelte 0/0 与 Web build 449 modules 通过。正式响应为
`tdx-market-l1-stream-status-v1` / `0x0547`，当前空闲态 subscriber/security=0/0、effective interval
15000 ms、FastHQ used=false；系统页新 bundle 为 `assets/index-AXPa2fLz.js`。本批 C++ production 与
API schema 未变，未重跑 full CTest/full API；上一完整基线仍为 138/138。

正式 PID 仍为 `20808`，EXE SHA-256 仍为
`cf4da92cf849ad4cecdce5a15ebb635c53c561db29bba768c00da472eec35a12`，web SHA-256 为
`6419fd8c46c069a5627aa4278551f9d07de91fb37092294ea1bfff405f7ae66a`；health 匹配且
`native_cpp=true`、`python_runtime=false`。上一 web `9bcb7d1e...8be7e01` 为 TOPRANGE 阶段，
executable rollback 不变。

## PEAK / PEAKBARS / TROUGH / TROUGHBARS

`output/ida-tcalc-peak-trough-targeted-20260813.json`（389949 B，SHA-256
`0e79e8308eafb1af852bc6c62ce139a065086f9545857ca813dcbbefdae77ea6`）闭合四个 handler 的
211/217/211/216 条指令与 17 个 reachable helper，frontier=0。新的私有 typed projection 只从末柱
读取 raw-f32 order，复用同一 ZIG 输出，以原生相对/绝对容差建立并轮转 peak/trough 候选队列；最终
未确认端点也可成为候选，value 与 bars 均按 raw-f32/sentinel 边界输出。旧的严格三点检测已删除，
ZIG 主状态机、公开 schema 与 future/read-only registry 未改。

native-operators、language、context-and-library focused 均通过；完整 build 与 CTest `138/138`
（0 failed，66.94 秒）通过。正式 40 根 `SZ000001` day 样本末柱为
`P=11.630000114440918/PB=9/T=11.25/TB=0`；内置 XT 末柱为箱底
`11.47499942779541`、箱顶 `11.39739990234375`、箱高 `-0.6762486100196838`，均标记
`explicit-read-only-lookahead`。未跑 full API suite。

正式 PID `35944`，EXE SHA-256
`ea666f23ac9af9cb7decb9d2608014bd48194e52f73227e8560a7c6fba0dcbdd`，web SHA-256
`6419fd8c46c069a5627aa4278551f9d07de91fb37092294ea1bfff405f7ae66a`；health hash 匹配，
`native_cpp=true`、`python_runtime=false`。PID `20808` 为上一阶段；rollback：
`output/tdx-tool-formal-cf4da92c-pre-peak-trough-native-rollback-20260813.exe`，SHA-256
`cf4da92cf849ad4cecdce5a15ebb635c53c561db29bba768c00da472eec35a12`。

## IF/IFF raw-f32 与 TPBus PushType 115

`IF/IFF` 数值输出现在经过 `native_float` 写回，复现 `TCalc!sub_1000B670` 的 dword store；IFN、
StringSeries IF、condition leading/started 语义不变。evidence 为 73731 B / 96 instructions / 0 callees，
SHA-256 `8df630be46cb119fa6b1cb8c560b3541d8bf7d63a1c871670e53d4d7c4550971`。focused 锁定
16777217→16777216、0.1→0.10000000149011612、leading/internal missing 与 IFN 独立边界。

新的 `level2_tpbus_115.cpp` 是独立 domain TU；public typed request 只接收已取得的 PushBody 和摘要
limit。外层只消费首个 u16-length segment，内层顺序消费 u32 discriminator/u16 body length/body；
0/1→111、其他→112，zero body skip，截断明确停止。每个非空 body 仅保留顺序、raw discriminator、
mapped type、size、SHA-256 与可选既有 111/112 摘要。CLI reader 独立有界处理 raw/hex，384 KiB；
没有 server/UI 入口或 EventBus/host/SDK/network 行为。四地址 evidence JSON 为 91268 B，SHA-256
`a7dbb5161c541c3bf3e584d83acf6ac0e428e9f47d491cb473274de1b50c77d7`。

公式全库审计 Web 同时补上可展开的 20 个真实上下文缺口与 5 个适用性边界；精确 binding 合并去重，
不伪造券商私有值。formula/Level2 focused、Svelte 0/0、Web build 449 modules、完整 build 与 CTest
`139/139`（0 failed，13.12 秒）通过；正式 IF、audit、page 与 tpbus-115 CLI smoke 通过，未跑 full API。

正式 PID `32500`，EXE SHA-256
`44f513e8c5b8978395306743dd6078bf21a0d0826c7a75a1af2fedbfc91754fe`，web SHA-256
`e6dc95ac299879cdeca17fc5c6df230c8c11fdf4c2a5f082f3df6482e3156a64`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `35944` 为上一阶段；rollback：
`output/tdx-tool-formal-ea666f23-pre-if-tpbus115-native-rollback-20260814.exe`，SHA-256
`ea666f23ac9af9cb7decb9d2608014bd48194e52f73227e8560a7c6fba0dcbdd`。

## ROUND / CEILING / FLOOR 与云工作流 Web consumer

`output/ida-tcalc-scalar-rounding-targeted-20260814.json`（143561 B，SHA-256
`24a5974f9ab23d7fd78502e6dae12d6d3b7bf6e7f6505ae50c859bfc5d6b9629`）闭合 ROUND、
CEILING、FLOOR 的 54/125/128 条指令与 `__ftol2_sse` helper。三者现分别执行 raw-f32
`±0.503000020980835` 取整和 relative/absolute tolerance 的 ceiling/floor；ROUND 每柱 sentinel gate，
CEILING/FLOOR 为 leading-only gate，started sentinel 继续走 native i32。EXP/LN/LOG 未随手修改。

新 `formula_engine_native_scalar_tests.cpp` 与 `native-scalars` named domain 避免让已有 2411 行
native-operators 测试越过软阈值。三个 formula focused 域、完整 build、CTest `139/139`（0 failed，
12.59 秒）通过。正式 day5 样本锁定 `ROUND(1.497)=2`、`ROUND(-1.497)=-2`、
`CEILING(1.00002)=2`、`FLOOR(-1.00002)=-2`。

Web 新增 `CloudWorkflows.svelte` 并接入 `/protocol/workflows`，只消费既有固定 workflow catalog/run
API；scalar 参数、选择键、limit/page/max-pages/timeout 均按 server 边界验证，不增加任意 URL、路径、
文件或 raw request 入口。Svelte 0/0、production build 451 modules；正式 catalog=6，
`index-valuation` 为 master 180 / selected 1 / detail groups 1，三项非法查询均为 400。未跑 full API。

正式 PID `20792`，EXE SHA-256
`230837462287aa23136c93598ac4227c5c470a6b3c29155e09d34093af4db1a3`，web index SHA-256
`45b19054a9a142e19e78f34db5422bda668c544d99015008695f12b30d1e6543`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `32500` 为上一阶段；rollback：
`output/tdx-tool-formal-44f513e8-pre-round-workflows-rollback-20260814.exe`，SHA-256
`44f513e8c5b8978395306743dd6078bf21a0d0826c7a75a1af2fedbfc91754fe`。

## EXP / LN / LOG 与 Cloud/JSN 覆盖 consumer

`output/ida-tcalc-scalar-rounding-targeted-20260814.json`（143561 B，SHA-256
`24a5974f9ab23d7fd78502e6dae12d6d3b7bf6e7f6505ae50c859bfc5d6b9629`）同时包含 EXP/LN/LOG
的 166/167/167 instructions。scalar domain 现复现其 raw-f32 type-3 广播和普通 Series 状态机：EXP
上界 88，LN/LOG 正数容差；leading sentinel 留空，started invalid/sentinel carry previous，LN/LOG
绝对首柱的独立 gate 保留。runtime 只把直接 `NodeKind::number` 送入 type-3 helper，bar-derived
常量仍走 Series，其他 scalar handler、schema 与 379 系统库覆盖不变。

Web 新增 `CoverageAudit.svelte` 与 `/protocol/coverage`，精确类型化 cloud variants 与 JSN variants
两个现有 schema。页面只做本地清单审计，显示 109 fixed cloud variants、622 JSN templates、588 typed、
34 generic-only 及 gap families；不执行云请求、不下载 JSN、不写 baseline，也不将 generic 支持标成
业务命令覆盖。

native-scalars/language focused、Svelte 0/0、Web build 453 modules、完整 build 与 CTest `139/139`
（0 failed，11.32 秒）通过。正式 day5 锁定字面量 `EXP(1)`/`LN(0.5)` 全柱 f32 广播、Series
`LN(0.5)` 首柱 null、`EXP(89)` 全 null；coverage page 200，cloud gaps=0、JSN gaps=34。未跑 full API。

正式 PID `22240`，EXE SHA-256
`1ea2ee8694166660aecdb0476f1b9619174e10182b3e71e55ea4bea061a7ac66`，web index SHA-256
`5d90c8cffe68dafcaa0647ef3f7ef9b59118ff7fced8706ec3205eb7257fdb8a`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `20792` 为上一阶段；rollback：
`output/tdx-tool-formal-23083746-pre-coverage-exp-log-rollback-20260814.exe`，SHA-256
`230837462287aa23136c93598ac4227c5c470a6b3c29155e09d34093af4db1a3`。

## ACOS / ASIN / TAN / SIGN / SGN / FRACPART

两份新 targeted evidence：trigonometric JSON 136711 B / SHA-256
`6e1666d4c51cfb15b5adc2be1f22c290b289e2be04f5c9d86d598777ccaf2c9d`，六 handler 为
184/200/89/89/89/158 instructions；fraction-sign JSON 38647 B / SHA-256
`4fb014b061263417e67e180209267f9888d076e158f8547dba71eebb80e56d30`，两 handler 为 65/114。

scalar TU 新 typed helpers 只覆盖证据自包含部分：ACOS/ASIN/TAN 的 literal type-3 与 raw-f32 Series
carry；SIGN/SGN 的 per-bar sentinel + inclusive ±1e-5；FRACPART 的 leading gate、signed adjustment、
safe i32 与 f32 output。ATAN/COS/SIN 普通 Series 的 DLL 分支依赖 `Src[6*size]` metadata，未把该未知
值猜入解释器；系统 379 源码没有这六个函数调用，public schema 不变。

native-scalars/language、tdx-tool link 与 CTest `139/139`（0 failed，48.46 秒）通过。正式 day5 smoke
锁定 ACOS/ASIN/TAN 的 f32 CRT 输出、SIGN/SGN `1/-1` 与 FRACPART integer-indefinite `4294967296`；
request body 不保留。未改 Web/Level2，未跑 full API。

正式 PID `16816`，EXE SHA-256
`aedd9b545147943f7d1486f8fea57ebeaa03b5ae446d246faa7ce8e7c94de5a0`，web index SHA-256
`5d90c8cffe68dafcaa0647ef3f7ef9b59118ff7fced8706ec3205eb7257fdb8a`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `22240` 为上一阶段；rollback：
`output/tdx-tool-formal-1ea2ee86-pre-trig-fraction-sign-rollback-20260814.exe`，SHA-256
`1ea2ee8694166660aecdb0476f1b9619174e10182b3e71e55ea4bea061a7ac66`。

## TPBus 115 HTTP / Web surface

`tpbus-115` 已从 typed domain/CLI 扩展到现有 POST `/api/v1/level2/decode` 与 Level2Lab。API 仅接受
内联 `payload_hex` 和 `limit`，复用 384 KiB 上限并严格拒绝路径字段；首个 outer segment 与 inner
tuple 的游标、zero-body skip、截断 stop reason、0/1→111/其他→112 均由同一个 domain 给出。
每个 body 只输出 size、SHA-256 和有界既有摘要，不回显原始字节；EventBus、host、SDK、message、
wire、network 与 entitlement 均保持未访问/未执行。

独立 115、server Level2、catalog focused、Svelte 0/0、Web build 和完整串行 build 均通过；CTest
`139/139`、0 failed（49.84 秒）。正式 API 锁定 valid/truncated/path-reject/OpenAPI/page 五项合同，
未跑 full API。当前 PID `22696`，EXE SHA-256
`8a443ece0a78f59117cd9bc3c93a4c9b330e7041ea92c291b12dc3c53efca2c3`，web index SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `16816` 为历史；rollback：
`output/tdx-tool-formal-aedd9b54-pre-tpbus-115-rollback-20260814.exe`，SHA-256
`aedd9b545147943f7d1486f8fea57ebeaa03b5ae446d246faa7ce8e7c94de5a0`。

## CONST / ROUND2 native float boundary

新证据 `output/ida-tcalc-const-round2-targeted-20260814.json`（29843 B，SHA-256
`a9a4a50cb18b2aa3840db8c94f1510f065c32465e425636b713bc21ed161a463`）闭合 CONST 18 条、
ROUND2 101 条指令和两个直接 helper，frontier=0。scalar domain 已把 CONST 改为末柱 raw-f32
广播；ROUND2 改为首柱 precision raw-f32 截整/0..4 clamp，以及逐柱 f32 scale、原生符号偏置与
f32 输出。CONSTA、ATAN/COS/SIN 不在本批证据边界内，保持不变。

native-scalars、language 与 tdx-tool link 通过；本批未改公共 schema/transport/cache，故未重复完整
CTest，最近完整基线仍为 `139/139`（49.84 秒）。正式 day5 得到 C=16777216、P/N=
±1.23000001907349 全柱，engine native、body retained=false。当前 PID `32380`，EXE SHA-256
`cf4ea0bc7df181f7310fcaedb3d9c53da73ffba5a08c020ef31ef5807640342e`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；PID `22696` 为历史，rollback：
`output/tdx-tool-formal-8a443ece-pre-const-round2-rollback-20260814.exe`，SHA-256
`8a443ece0a78f59117cd9bc3c93a4c9b330e7041ea92c291b12dc3c53efca2c3`。

## CONSTA final-offset raw-f32 semantics

`output/ida-tcalc-consta-targeted-20260814.json`（15625 B，SHA-256
`8cab0f00d4fde8e865f965ec048cf465e5f72cbd42c445b827f3b01e7b701c6d`）闭合
`sub_1000F360` 36 条指令与唯一 `__ftol2_sse` helper，frontier=0。scalar domain 现只读 offset
序列末柱 raw-f32，native 截整并 clamp `0..count-1`，再把对应 source raw-f32 广播；focused 以
`1.99999999 -> f32 2` 区分旧 double 截整路径，并覆盖 selected source f32、missing offset 与 oversized
offset。CONSTA 在 379 系统库为 0 calls；ATAN/COS/SIN 的 buffer metadata 来源尚未闭合，保持不变。

native-scalars、language 与 tdx-tool link 通过；本批未改共享 parser/schema/transport，故未重复完整
CTest，最近完整基线仍为 `139/139`（49.84 秒），未跑 full API。正式 day5 A/B 分别为
`11.2600002288818` / `11.25` 全柱，engine native、body retained=false。当前 PID `31856`，EXE
SHA-256 `31452ab93214a1ae803c4ddab766cd0f72aff524450a68bee5a1cb829c32d9d1`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；PID `32380` 为上一阶段，
rollback：`output/tdx-tool-formal-cf4ea0bc-pre-consta-rollback-20260814.exe`，SHA-256
`cf4ea0bc7df181f7310fcaedb3d9c53da73ffba5a08c020ef31ef5807640342e`。

## IF / IFF / IFN / RANGE native float boundary

`output/ida-tcalc-ifn-range-targeted-20260814.json`（40251 B，SHA-256
`e414d1e1744c9d16826a834d05be81c288f9755b3c56b2eb71082a280b125f39`）闭合 IFN 96 条、
RANGE 153 条指令，helper/frontier=0；IF/IFF 沿用既有完整 IF handler 证据。IF/IFF/IFN 与字符串
IF 现在先把 condition 收窄到 raw-f32，再按 exact zero 选支；IFN 的 selected numeric branch 同样
写回 raw-f32。RANGE 以 bounds-only joint-leading gate 启动，随后 value/bounds raw-f32 和内部 sentinel
均参与带 `1e-7 + 1e-5` 容差的双边判断。IFN/RANGE 在 379 系统源码为 0 direct calls，public schema
不变。

native-operators、native-scalars、language 和 tdx-tool link 通过；本批未改共享 parser/schema/
transport，未重跑 full CTest/API，最近完整 CTest 仍为 TPBus 115 阶段 139/139（49.84 秒）。正式
day5 I/J/N/S/R 分别为 `0.10000000149011612/0.10000000149011612/16777216/1/1`，engine
`tdx-source-interpreter-v1`、body retained=false。当前 PID `14624`，EXE SHA-256
`0c2e62f79911cf82a7c0bbff05c84c0924b2a338b25431a649c23b628ce506a2`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；PID `31856` 为上一阶段，
rollback：`output/tdx-tool-formal-31452ab9-pre-ifn-range-rollback-20260814.exe`，SHA-256
`31452ab93214a1ae803c4ddab766cd0f72aff524450a68bee5a1cb829c32d9d1`。

## ADD / EQUAL / NOT_EQUAL operator semantics

`output/ida-tcalc-add-equal-targeted-20260814.json`（63420 B，SHA-256
`64e8386daa893d239c84481dab5772563da44dbd7048d45ef360d9a69fccc3d8`）闭合 ADD 147 条、
EQUAL/NOT_EQUAL 各 113 条指令以及两个 helper，frontier=0。operator domain 现让 `+` 的输入/输出
均落 raw-f32 并传播 sentinel；`=` 采用 strict ±1e-5，`<>` 采用 inclusive ±1e-5，二者都直接比较
raw-f32，且 canonical sentinel 作为有限原生值参与。379 源码词法影响为 ADD 96/316、EQUAL
69/185、NOT_EQUAL 2/2，public schema 不变。

新增独立 `native-binary` 测试 TU/命名域，未继续增大 2432 行的旧 native-operators TU；native-binary、
language、native-operators 与全部 formula-engine domains 通过，tdx-tool link 通过。未重跑 full
CTest/API，最近完整 CTest 仍为 139/139（49.84 秒）。正式 day5 A/B/F/I/E/N/MM/MV=
`16777216/0.30000001192092896/1/1/0/1/1/1`，末柱 CLOSE+1=`12.25`，body retained=false。

当前 PID `2800`，EXE SHA-256
`b8f871b3da1c0ca8a7b939a8a74aa9b96916c99f9e1c718db1d78ddc41ea6d92`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；PID `14624` 为上一阶段，
rollback：`output/tdx-tool-formal-0c2e62f7-pre-add-equality-rollback-20260814.exe`，SHA-256
`0c2e62f79911cf82a7c0bbff05c84c0924b2a338b25431a649c23b628ce506a2`。

## Unary minus lowering

隔离 compile probe `output/tcalc-unary-minus-compile-probe-20260814.json`（3370 B，SHA-256
`18dcca15710f049f9525c7b28cadf2c0757f32e53ae755deeea367583112ab26`）证明 TCalc 将
`-CLOSE` 降为 `-1.0F * CLOSE`，显式 `-1*CLOSE` 使用同一 operand/operator graph，负整数字面量也
先落 raw-f32。production 因而以 `tcalc_negate -> tcalc_multiply` 复用已闭合的 `sub_10004130`
输入/结果 f32 与 sentinel 语义；不加载 TCalc.dll，不改变 schema。379 系统源码影响为 21 formulas/
34 occurrences；系统源码 `%` 为 0，未扩实现。

native-binary、language、native-operators、全部 formula-engine domains 与 tdx-tool link 通过；未跑
full CTest/API，最近完整 CTest 仍为 139/139（49.84 秒）。正式 day5 末柱 C/F/M/O 为
`-11.25/-0.10000000149011612/null/null`，body retained=false。当前 PID `28016`，EXE SHA-256
`e56a01e20b2e1a63d6f632a06239588c3afc6cceff8d94f1583dd1e69c3bd0a1`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；PID `2800` 为上一阶段，
rollback：`output/tdx-tool-formal-b8f871b3-pre-unary-negation-rollback-20260814.exe`，SHA-256
`b8f871b3da1c0ca8a7b939a8a74aa9b96916c99f9e1c718db1d78ddc41ea6d92`。

## Numeric literal raw-f32 nodes

`output/tcalc-numeric-literal-compile-probe-20260814.json`（1094 B，SHA-256
`ff9dcd7110e35c7d91ddae6c9946aa3936b9f5e91b558b112f46f4bc3dabdad8`）锁定 numeric node：
16777217→raw-f32 16777216，0.1→0.10000000149011612。runtime 的 `NodeKind::number` 现于广播前
落 f32；parser/AST/schema 不变。379 系统源码影响为 330 formulas/3425 literals。

全部 formula-engine domains 与 tdx-tool link 通过；overflow-to-i32 夹具改由有限
`2147483648` 节点驱动，
不丢 MOD/INTPART/ROUND/CEILING/FLOOR 判别，render WIDTH 同步使用 `double(0.1F)/4`。未跑 full
CTest/API，最近完整 CTest 仍为 139/139。正式 day5 I/D/A/C=`16777216/
0.10000000149011612/16777216/11.350000381469727`，day40 CCI=`-45.719879150390625`。
当前 PID `31728`，EXE SHA-256
`7738a999e23a8f9d09142791a16062dcd30cd676456deb479d8542633bc04517`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；PID `28016` 为上一阶段，
rollback：`output/tdx-tool-formal-e56a01e2-pre-numeric-literal-rollback-20260814.exe`，SHA-256
`e56a01e20b2e1a63d6f632a06239588c3afc6cceff8d94f1583dd1e69c3bd0a1`。

### Parameter-slot addendum

Formula active parameters are also dword floats. Execution, response `parameters`, and the derived MTM N now
use the same effective f32; direct-engine f32 overflow is rejected while the existing HTTP `1e9` cap remains.
All formula-engine domains and tdx-tool link passed again; formal N/P/Q are all `16777216`.

Final PID `33640`, EXE SHA-256
`8fd3b43478fb823cd35213dfcda385afcc6472dc05ce1b7b7b66dc23fae3c49a`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `31728` is the
literal-only intermediate; rollback is
`output/tdx-tool-formal-7738a999-pre-parameter-float-rollback-20260814.exe`, SHA-256
`7738a999e23a8f9d09142791a16062dcd30cd676456deb479d8542633bc04517`.

## Core K-line field raw-f32 boundary

Targeted evidence `output/ida-tcalc-price-field-handlers-targeted-20260814.json` is 555004 B with SHA-256
`33c32e34369cbc449c4be0e4ceee6f4c39e4087744391e229b0f4e28379fb66a`. OPEN/HIGH/LOW/CLOSE are
four 58-instruction handlers copying raw f32 from 35-byte record offsets 7/11/15/19. VOL/AMOUNT write f32
after their existing host unit rules; two remaining deep frontier calls are retained as host-classification
boundaries and are not guessed here.

Formula binding now narrows OPEN/HIGH/LOW/CLOSE/AMOUNT, formula VOL, and the internal raw-volume view.
Ordinary-stock VOL narrows the source first, divides by 100, then stores f32. Top-level point OHLCAV remain the
caller-owned document values, so no public schema changed. Direct system-library reach is 252/379 formulas
(OPEN 63/159 calls, HIGH 90/180, LOW 88/190, CLOSE 203/655, VOL 56/133, AMOUNT 8/13).

The native-binary domain, all formula-engine domains, and the tdx-tool link passed. No full CTest/API was run;
the latest full suite remains TPBus 115 at 139/139 (49.84 seconds). Live day5 CLI evidence is
`output/formula-price-field-float-live-20260814.json`, SHA-256
`0172653f3f39d21b4f48ac5269f4b2592e656f91f92a9016b16b5d8ba3dde78f`; the formal final O/H/L/C/A/V
are `11.229999542236328/11.270000457763672/11.180000305175781/11.25/848358784/755980.8125`,
native-cpp with request-body retention false.

Final PID `25212`, EXE SHA-256
`9bb181de4c87bdcc2461ec87c0234828dd48dc68ff4242f4cec291d77062a645`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `33640` is historical;
rollback is `output/tdx-tool-formal-8fd3b434-pre-price-field-float-rollback-20260814.exe`, SHA-256
`8fd3b43478fb823cd35213dfcda385afcc6472dc05ce1b7b7b66dc23fae3c49a`.

### Auxiliary K-line raw-f32 addendum

`output/ida-tcalc-auxiliary-field-handlers-targeted-20260814.json` (438719 B, SHA-256
`c990604f58f8d0ba2fffbce3f58f51baa0922b72948c7ec5151e972fb5e07b15`) closes three 58-instruction
record+31 f32 handlers for ZSTJJ/QHJSJ/HKSHORTVOL and the 113-instruction VOLINSTK u32-to-f32/zero branch.
Existing open-interest and HK-short availability gates remain strict. System reach is 3/379 formulas.

All formula-engine domains and tdx-tool link passed. Live `47:IFL9` day5 evidence is
`output/formula-auxiliary-field-float-live-20260814.json`, SHA-256
`ef69770e38fde8a49d75699764f2671cb73c40058522b82340ae5a64166b934b`; its final I/C/Z/Q are
`272336/272336/4608.39990234375/4608.39990234375`. No full CTest/API was run.

Final PID `27868`, EXE SHA-256
`bfdc14bc0e4c118c21030dc362c6ae61bc821e4a0e1f2e2f95d93f78c5335bd0`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `25212` is the
core-field intermediate; rollback is `output/tdx-tool-formal-9bb181de-pre-auxiliary-field-float-rollback-20260814.exe`,
SHA-256 `9bb181de4c87bdcc2461ec87c0234828dd48dc68ff4242f4cec291d77062a645`.

## Scalar market-context raw-f32 boundary

Targeted evidence `output/ida-tcalc-scalar-context-handlers-targeted-20260814.json` is 30667 B with SHA-256
`8d3c5a49655fa9cdbf6ae94e4887812ae832938b17792645e65ad3f72dfa68e3`. Its CAPITAL,
TOTALCAPITAL, MINDIFF, and MULTIPLIER roots contain 63/49/45/39 instructions and all write f32 result
buffers. CAPITAL/TOTALCAPITAL read a type-105 f32 share count before the existing unit conversion,
MINDIFF compares a source f32 against the native f32 `1e-5` floor, and MULTIPLIER converts signed i16 to
f32. Direct host callees remain an explicit classification boundary; this batch does not invent a new divisor rule.

Only these four named values are narrowed in caller scalar, symbol, and point-series context groups.
FINANCE/FINVALUE/DYNAINFO and unrelated context values remain doubles. Current automatic capital now follows
source-f32 -> `/100` -> result-f32, while automatic MINDIFF follows source-f32 -> native floor. No public schema or
context key changed. Direct system-library reach is 13 formulas: CAPITAL 11/14 references and MINDIFF 2/3;
TOTALCAPITAL/MULTIPLIER have no current system-source references.

The native-binary domain, all formula-engine domains, and the tdx-tool link passed. Live day800 CLI evidence is
`output/formula-scalar-context-float-live-20260814.json` (303012 B, SHA-256
`f429c750228c19c35bf4dcdd34fc27b7e374b2f2401dacb139e28ae6ab00735c`); final C/T/D values are
`194056000/194059184/0.00999999977648258`. Three focused HTTP contracts cover automatic context, explicit
scalar overrides, and point series; all returned 200/native-cpp with request-body retention false. No full CTest/API
was run; the latest full suite remains TPBus 115 at 139/139 (49.84 seconds).

Final PID `25268`, EXE SHA-256
`5978ba25e10b4fd71ce78837cb5004c746ba03df07cbd462fbbc891094b8f6e6`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `27868` is historical;
rollback is `output/tdx-tool-formal-bfdc14bc-pre-scalar-context-float-rollback-20260814.exe`, SHA-256
`bfdc14bc0e4c118c21030dc362c6ae61bc821e4a0e1f2e2f95d93f78c5335bd0`.

## Numbered financial-context selector and raw-f32 output

Targeted evidence `output/ida-tcalc-numbered-context-handlers-targeted-20260814.json` is 357331 B with
SHA-256 `99c1c3e7cfac97d6fccb691693130c0fd81e9f1bc12991374189c05ac6df898e`. FINVALUE
`sub_10011A00` contains 169 instructions; the shared FINANCE/DYNAINFO `sub_10026B20` contains 2004.
Both read the final argument as raw f32, convert that selector through `__ftol2_sse`, and write a float result
buffer. The evidence does not reconstruct the live host callback or claim to synthesize finance/quote records.

FINANCE, FINVALUE, and DYNAINFO now resolve one final-bar selector, then narrow every value of the existing
numbered context binding to raw f32. Missing, float overflow, and the canonical sentinel map safely to null.
The context schema and keys are unchanged, and caller-supplied FINVALUE as-of series retain their point alignment.
System-library reach is FINANCE 46 formulas/112 calls, FINVALUE 4/5, and DYNAINFO 20/78: 59 unique formulas.

The native-binary and context-and-library domains, all formula-engine domains, and the tdx-tool link passed.
Live day5 evidence `output/formula-numbered-context-float-live-20260814.json` is 16878 B, SHA-256
`8afc4760126bc17acaf402e9a2ec82f400c92a195601de8920cddd256bc4110e`; final F/D/V values are
`16777216/-16777216/16777220`. Three focused POST contracts returned 200/200/400 and retain no successful
request body. No full CTest/API was run; the latest full suite remains TPBus 115 at 139/139 (49.84 seconds).

Final PID `36312`, EXE SHA-256
`d56acfa73629e315c21eaddf66c7aee0ce18bb6f6b893af123ccac0bdd56a43f`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `25268` is historical;
rollback is `output/tdx-tool-formal-5978ba25-pre-numbered-context-float-rollback-20260814.exe`, SHA-256
`5978ba25e10b4fd71ce78837cb5004c746ba03df07cbd462fbbc891094b8f6e6`.

## Professional-value final selectors and the opaque TPBus PB boundary

Targeted evidence `output/ida-tcalc-professional-value-targeted-20260814.json` is 485653 B with SHA-256
`124fe804cd556272799f8ba1fcf5e3a1e29bc54d201bb80c7cb124fbec7fb013`. The GPJYVALUE,
BKJYVALUE, and SCJYVALUE type-174 handlers read all three selectors from the final raw-f32 argument slots and
write float result buffers. Live host lookup and professional-record/date construction remain caller-owned context.

The interpreter now resolves one final `NAME#data#field#date_mode` binding and narrows every returned point to
raw f32 without changing the context schema or source policy. Reach is 15 unique system formulas: GPJYVALUE
11/24, SCJYVALUE 11/32, and BKJYVALUE 4/4. The native-binary and context-and-library domains, tdx-tool link,
one real SZ000001 day20 sample, and three focused POST contracts passed. No full CTest/API was run; the latest
full baseline remains TPBus 115 at 139/139.

The current installation was also rescanned for TPBus 113/114/116 protobuf ownership. Only `tpbus.dll` contains
`MaintainData.HQPUSHPB`; it contains no `.proto`/descriptor strings, no installed business descriptor can be tied
to that topic, and the captured EventBus registry has no consumer. The supported boundary therefore remains
unknown-schema protobuf wire inspection, with no invented typed fields.

Final PID `480`, EXE SHA-256
`1300851173dd12c56f969b421b60ed1dbe2b0e4919b14cc4ad7e58c49ad7a1fc`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `36312` is historical;
rollback is `output/tdx-tool-formal-d56acfa7-pre-professional-final-selector-rollback-20260814.exe`, SHA-256
`d56acfa73629e315c21eaddf66c7aee0ce18bb6f6b893af123ccac0bdd56a43f`.

## STRCMP final-handle comparison and broadcast

Targeted evidence `output/ida-tcalc-strcmp-targeted-20260814.json` is 63500 B with SHA-256
`8e1d87490bbadb1f202e24ec8e00740669b050f138aadfad6dcc9e652a610b2f`. Native
`sub_1000F200` has 108 instructions: it reads the two final raw-f32 string handles, resolves each once through
the existing string pool, compares once, and broadcasts one 0/1 value. It does not compare strings per bar.

`STRCMP` now compares `StringSeries.back()` and broadcasts without changing string builders, IF, STRLEN,
the public schema, or context. The system library reaches this path through 2 formulas/32 calls (R标记数 and
G标记数), whose CODE-versus-literal contracts remain constant. The native-binary and language domains and
the tdx-tool link passed. Live SZ000001/day5 evidence
`output/formula-strcmp-final-handle-live-20260814.json` (13874 B, SHA-256
`d16532698d4adba82612d95044db6ac00d579da9316e312dde49475058c69513`) has complete S/T arrays
`[1,1,1,1,1]` and `[0,0,0,0,0]`. Three focused POST contracts returned 200/200/400; successful bodies are
not retained. No full CTest/API was run; the latest full baseline remains TPBus 115 at 139/139.

Final PID `4560`, EXE SHA-256
`a005f2fb1e60580087a4f1217e352da296907fd6db100e0a5e20b071a0e120d8`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `480` is historical;
rollback is `output/tdx-tool-formal-13008511-pre-strcmp-final-handle-rollback-20260814.exe`, SHA-256
`1300851173dd12c56f969b421b60ed1dbe2b0e4919b14cc4ad7e58c49ad7a1fc`.

## SIGNALS_QS final selectors, f32 output, and sparse modes

Targeted evidence `output/ida-tcalc-signals-qs-targeted-20260814.json` is 36690 B with SHA-256
`6a5da89e4e86e94bf5ee509fae5d277c3afa475d68ca8e03009a4cf9a8f3e620`. Native
`sub_100111B0` has 173 instructions. It truncates both final raw-f32 arguments, asks host type 35 for sparse
dated rows, writes matched values through f32, retains missing for mode 0, carries prior output for mode 1, and
fills absent dates with zero for mode 2.

The new dedicated broker binding consumes only caller-supplied `SIGNALS_QS#id#mode` aligned series. It does
not fetch broker signals, invoke the host callback, access credentials, bypass authorization, or use the network.
System-library reach is 6 formulas/13 calls. The context-and-library domain and tdx-tool link passed. Live
SZ000001/day5 evidence `output/formula-signals-qs-native-semantics-live-20260814.json` (15807 B, SHA-256
`76fc655453dd8ff383139b5d4284d1003fa30d677df9fd2a9aace3311cc9ae4f`) records D as
`[16777216,0.100000001490116,-16777216,3,4]`, carried C as `[null,5,5,5,7]`, and zero-filled Z as
`[0,5,0,0,7]`. Focused POST contracts returned 200/400/400 for explicit context, missing binding, and dynamic
selector safety. No full CTest/API was run; the latest full baseline remains TPBus 115 at 139/139.

Final PID `34560`, EXE SHA-256
`b30373b159169616839e4c312afa70c5b271bec21ad0e3f7900af1f2c4af6760`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `4560` is historical;
rollback is `output/tdx-tool-formal-a005f2fb-pre-signals-qs-native-rollback-20260814.exe`, SHA-256
`a005f2fb1e60580087a4f1217e352da296907fd6db100e0a5e20b071a0e120d8`.

## L2_AMO final selectors and type-168 consumer boundary

Targeted evidence `output/ida-tcalc-l2-amo-targeted-20260814.json` is 464291 B with SHA-256
`e5f8699a5d65fe0b99e69338684d090e0e678d9f4416716b81ef53870d33286e`. Native
`sub_10036EE0` has 56 instructions: it reads both selectors from the final raw-f32 argument slots, truncates and
validates each as unsigned `0..3`, then copies one f32 per 184-byte type-168 row from
`72 + 4 * (second + 4 * first)` when host state exists.

The dedicated `level2_amount_binding` consumes only caller-supplied `L2_AMO#first#second` aligned series. It
does not acquire Level2 data, invoke the type-168 callback, access credentials, bypass authorization, or use the
network. System-library reach is 5 formulas/34 calls. The context-and-library domain and tdx-tool link passed.
Real SZ000001/day5 K-lines plus an explicitly synthetic caller-owned L2 context produced
`[16777216,0.100000001490116,-16777216,null,4]` in
`output/formula-l2-amo-final-selector-live-20260814.json` (12491 B, SHA-256
`2ea9cbd19821e42d5181c3494c16358a8e47ee1ad7808e1e359e38fa2532cc24`). This is a consumer-semantics
fixture, not real Level2 data. Focused POST contracts returned 200/400/400 for explicit context, missing binding,
and dynamic selector safety. No full CTest/API was run; the latest full baseline remains TPBus 115 at 139/139.

The current process instance is PID `4560`, EXE SHA-256
`88c48ea38f0762616d1daf05b8a30637de56363b7447eb982f06a52040a3162e`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `34560` is historical;
the older STRCMP process also used PID `4560` due to PID reuse. Rollback is
`output/tdx-tool-formal-b30373b1-pre-l2-amo-native-rollback-20260814.exe`, SHA-256
`b30373b159169616839e4c312afa70c5b271bec21ad0e3f7900af1f2c4af6760`.

## Native RGB conversion and the static HQPUSHPB boundary

Targeted evidence `output/ida-tcalc-rgb-targeted-20260814.json` is 14035 B with SHA-256
`9cefca0bbf8df9783acc4a4ad5a99c01a345e71549ef61ad9a34a8b357dfdb93`. Native
`sub_1000A6A0` has 84 instructions and no direct callee/frontier. Each channel is read as raw f32, converted by
x87 toward-zero to an integer, reduced to low-u32, retained as its low byte only when unsigned `<255`, and
otherwise replaced with 254. The packed `R | G<<8 | B<<16` value is stored as f32. The interpreter now safely
models integer-indefinite without changing COLORREF packing, render IR, or the public schema.

System-library reach is 6 formulas/18 calls; five formulas/nine calls use 255 and had a visible one-level color
difference. The render-ir domain and tdx-tool link passed. Real SZ000001/day5 evidence
`output/formula-rgb-native-channel-live-20260814.json` (17257 B, SHA-256
`fa92a2f21a073f8657218b7ee7a1b05ebd980e757a86edeffe0b7d0120d109b2`) has complete R/N/F series
254, `0xFE01FE`, and `0x030201`. Focused POST contracts returned 200/200/400 for native channels, missing
conversion, and arity rejection; successful bodies are not retained. No full CTest/API was run; the latest full
baseline remains TPBus 115 at 139/139.

The HQPUSHPB targeted scan (`output/ida-tpbus-hqpushpb-consumer-targeted-20260814.json`, 228125 B,
SHA-256 `ce226c13e55fbf6d84fc63912472ae43f6b260d6d7a0e46d8260988ffb4e25d0`) found only the
publisher's two topic xrefs and no exact/wildcard observer or protobuf descriptor/parser in the current install.
TPBus 113/114/116 therefore remain opaque wire inspection rather than invented typed business decoders.

Final PID `33964`, EXE SHA-256
`7194ec950edc472d4f9fd018d0f71106b6dea51f3bc1e7a6bf58ccebb793af68`; web SHA-256 remains
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`. PID `4560` is historical;
rollback is `output/tdx-tool-formal-88c48ea3-pre-rgb-native-rollback-20260814.exe`, SHA-256
`88c48ea38f0762616d1daf05b8a30637de56363b7447eb982f06a52040a3162e`.

## Level2 Lab bounded result disclosure

The request-builder, decoder, quote-transition, host-projection, and dual-snapshot result blocks now default to
summary-only rendering and do not stringify full response documents. The first explicit expansion recursively
previews at most the first 100 items of every array and reports each truncated path with total, shown, and omitted
counts; full JSON requires a second explicit action. A new request or format switch resets stale result/detail state.

Authorized decode, quote, host, and snapshot captures are cleared only after success and only when the current
input still equals its submitted snapshot. Failures and concurrently edited inputs are retained, and every capture
has an explicit clear action. Previous-state, 4651 `previous`, and explicit `context` editors are retained for
continued projection. This is Svelte-only; native endpoints and schemas are unchanged. `npm run check` completed
with 0 errors/0 warnings and `npm run build` succeeded with only the existing >500 KiB chunk warning. No CTest or
API suite was run; the latest full CTest baseline remains TPBus 115 at 139/139.

Final PID `19348`, EXE SHA-256
`7194ec950edc472d4f9fd018d0f71106b6dea51f3bc1e7a6bf58ccebb793af68`; web index SHA-256
`378da4337ceb42c55444a2ccd4aa3c8f78735a9b8e79517fb962258c892ada00`. Health matches with
`native_cpp=true` and `python_runtime=false`. PID `33964` and web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee` are historical.

## Bounded TQLEX/PBRPC tables and TPool XML lifetime

TQLEX and PBRPC retain their shared `ResultSets` responses in full; only the visible DOM is bounded through fixed
200-row client-side pages. Columns are inferred from the first 200 rows, the current range and page controls are
explicit, and both a new response and a result-set switch reset the page. TQLEX validates `page >= 0`,
`page_size=1..5000`, and, in all-pages mode, `max_pages=1..20` with
`page_size * max_pages <= 50000`. Every request carries `max_pages` (1 for single-page mode); defaults are 20/10.

TPool keeps its 512 KiB inline-XML limit. It snapshots submitted XML and clears the editor only after a successful,
still-current request when the user has not changed that input. Failure, request invalidation, and in-flight edits
retain it for retry. The file input resets after every read, oversized-file, failure, or empty-selection path; an
explicit action clears both XML inputs while retaining the source name. Copy distinguishes server non-retention
from the browser's success-only clearing.

These are Svelte-only changes with no backend or schema change. `npm run check` completed with 0 errors/0 warnings
and `npm run build` succeeded with only the existing chunk warning. The `tqlex`, `pbrpc`, and `tpool` routes returned
200 and referenced the new asset. No CTest/API suite was run; the latest full CTest remains TPBus 115 at 139/139.

Final PID `10812`, EXE SHA-256
`7194EC950EDC472D4F9FD018D0F71106B6DEA51F3BC1E7A6BF58CCEBB793AF68`; web SHA-256
`E9FA36DC240FE4B2EB4CA88872008324A0091F7A0DC4F1F1FE43B99B391CB75C`. Health matches with
`native_cpp=true` and `python_runtime=false`. PID `19348` and web SHA-256
`378DA4337CEB42C55444A2CCD4AA3C8F78735A9B8E79517FB962258C892ADA00` are historical.

## TQLEX HTTP planning and a downscalable PBRPC assembly budget

The TQLEX HTTP surface now builds a typed plan with `page=0..1000000`, `page_size=1..5000`, and
`max_pages=1..20` (default 10). All-pages requests additionally require
`page_size * max_pages <= 50000`; single-page requests use an effective maximum of one page. This HTTP-only
contract does not narrow the existing CLI or shared-domain limits.

The typed PBRPC C++ query path gained an additive trailing `max_assembled_bytes` parameter, propagated through
CLI `--max-assembled-bytes`, the HTTP query, PbrpcConsole, and OpenAPI. Its range is `1..134217728`; the default
and hard maximum remain 128 MiB. Each fragment is checked against the remaining budget before append, so a
one-byte excess cannot be partially appended. The 64 MiB per-HTTP-response limit and all `max_rounds` limits and
defaults are unchanged.

`tdx-native-tests`, `tdx-server-tqlex-tests`, `tdx-server-catalog-tests`, and the `tdx-tool` link passed.
`npm run check` reported 0 errors/0 warnings, and `npm run build` succeeded with only the existing >500 KiB chunk
warning. Focused HTTP coverage includes five invalid requests returning 400, the OpenAPI parameter contract, and
successful TQLEX/PBRPC routes. The full API run completed 209/225 contracts in 226 requests: generic TQLEX,
PBRPC, OpenAPI, and features contracts all passed; the 16 failures are existing local-data/formula-audit baseline
failures, with no failure in this change's contracts. The report is
`output/api-contracts-tqlex-pbrpc-budget-final-20260814.json`, SHA-256
`3BA836D85093E028E3C937F1E48B425365247EC009B8D1D2E95199FDC360E5E6`. No full CTest was run; the latest
full 139/139 baseline remains the TPBus 115 stage.

One deployment copy encountered a locked destination executable; old-hash PID `14592` never listened and was
stopped. The correct formal instance is PID `36440`, EXE SHA-256
`C120DBFAAC9EF67E98B2B4697B9A42AA0DCD6B3CFB655ED65684C3783F89DD38`, web SHA-256
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`; health matches with
`native_cpp=true` and `python_runtime=false`. PID `10812` / web SHA-256
`E9FA36DC240FE4B2EB4CA88872008324A0091F7A0DC4F1F1FE43B99B391CB75C` is the previous formal identity, and
PID `14592` is a non-listening, non-formal historical instance. Rollback is
`output/tdx-tool-formal-7194ec95-pre-tqlex-pbrpc-budget-rollback-20260814.exe`, SHA-256
`7194EC950EDC472D4F9FD018D0F71106B6DEA51F3BC1E7A6BF58CCEBB793AF68`.

## HK audit context parity and numeric-fidelity contract (2026-08-14)

The HTTP formula audit now collects proven HK `FINANCE#selector` bindings exactly as the existing CLI path does,
then materializes them from local HK finance data. The former 13 evaluation errors on `31:00700` are gone: every
affected system formula passes with a latest numeric value. A defensive interpreter preflight also classifies truly
unmaterialized `FINANCE#/FINVALUE#/DYNAINFO#` keys as `context_unavailable` with exact required/unavailable lists;
supplying the keys restores normal evaluation.

The coverage recon contract now preserves the four proven ZTPRICE/DTPRICE host-type-120 fidelity gates instead of
requiring zero degradation. The loaded 380-formula set is 376 numeric-safe + 4 gated (system baseline 375+4), while
56 presentation-return surrogates remain a separate category.
The price-annotation contract now accepts the exact `kline.price_precision` provenance emitted when a real K-line
provides precision metadata, while retaining a separate fixture for `native-constructor-default`.

The formula-engine, server-formula-library, and recon-contract focused executables and the `tdx-tool` link passed.
The three final formula contracts passed 3/3. The final full run is 212/225 in 226 requests: coverage, HK audit, and
price annotations all recovered from the old baseline; the remaining 13 failures are existing data baselines. Report
SHA-256: `FBEFB32836DF45B7F5E6A7CDBABDB2ED27EAF46E3D9E42D26DD7072258587658`.
No full CTest was run; the latest complete baseline remains 139/139.

Formal PID is `33136`; EXE SHA-256 is
`228F5261F52D9D19B2E33B08D1A5203C79DC42441FD899629BA7FACBAFB3ACD6`, web SHA-256 remains
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`, and health matches with native true /
python false. PID `21544` / EXE `76EB2DA6...F869D` is the intermediate pre-price-contract deployment. Direct rollback is
`output/tdx-tool-formal-76eb2da6-pre-price-annotation-contract-rollback-20260814.exe`, SHA-256
`76EB2DA6232693C1679F37A01C15FCCC5FFA64E20CDEA33D740D15DF804F869D`.

## Rolling-market reconciliation without static snapshots (2026-08-14)

The remaining thirteen API failures were stale recon snapshots, not decoder failures. Contracts now preserve the
strong invariants while allowing the source populations to roll: published convertible-bond/BSE-IPO units still
reconcile row by row, while not-yet-published values must be an exact normalized-null/raw-empty pair; corporate and
private bond projections use set-cardinality identities, explicit bidirectional differences, and dynamic client-master
equivalence; HK events, corporate-transition families, fund statistics, and fifteen financial screens require populated
families plus exact summary sums. The 17 overview factors are recounted from record signals. The security-filtered
calendar requires every returned row to resolve to `SH688783`, retain raw evidence, remain HTML-safe, and preserve
applicable recent-IPO units, without assuming three time-sensitive event families will remain attached forever.

`tdx-recon-contract-tests` and the `tdx-tool` link passed. The former failures passed 13/13 on the formal service, and
the final full API run passed **225/225** with no failures. The full report is
`output/api-contracts-full-reconciliation-formal-20260814.json`, SHA-256
`3878059D4D481904B2221E28E5A68008B2A0CE816A6F1E9BE7C2916D114C5E93`; the focused thirteen-case report SHA-256
is `1D9EE452F21A50943FC3C1B312B4453B0C636CD68A5BA20CAE3FC19CC53E3550`. No full CTest was run; the latest
complete baseline remains 139/139 from TPBus 115.

Formal PID is `30820`; EXE SHA-256 is
`1D189E40A11171A098C58D449E5288AEF1537C9E001AB6EF9FDEF87185DBEF9C`, web SHA-256 remains
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`, and health matches with
`native_cpp=true` / `python_runtime=false`. PID `33136` / EXE `228F5261...ACD6` is historical. Direct rollback is
`output/tdx-tool-formal-228f5261-pre-reconciliation-rollback-20260814.exe`, SHA-256
`228F5261F52D9D19B2E33B08D1A5203C79DC42441FD899629BA7FACBAFB3ACD6`.

## 2026-08-14: exact ZTPRICE / DTPRICE host context

The interpreter no longer uses the generic `round(price*(1±rate)*100)/100` approximation. Evidence
`output/ida-tcalc-refx-limit-price-targeted-20260813.json` (SHA-256
`271F3A27E28769A00F8C80FA9A130BC3A22F957A5821991FE0D5D03C2F4FA46F`) closes the final-bar rate,
100/1000 precision, ordinary/special-market biases, the Z-path intermediate float landing, and native signed low-dword
conversion. Type-120 classification is produced by `sub_5A3810`, not copied from one TNF field; the corresponding
evidence SHA-256 is `008092FAA8F4E668C2B0D5AF55B40A3AEA4FF44E77978E7B17908E7596936BAD`.
Callers must therefore provide the u16 bindings `HOST_TYPE120_SECURITY_CLASS_RAW` and
`HOST_EVALUATOR_MARKET_WORD_RAW`; normalized market/code is never guessed. B007/C128/C129/C130 are now
numeric-safe explicit-context formulas. Coverage is 379/379 for the system library and 380/380 with the opt-in user
formula, with zero degraded numeric outputs and 24 explicit-context formulas.

Missing materialized HK FINANCE/FINVALUE/DYNAINFO keys are also classified as `context_unavailable` before
evaluation, without weakening explicit L2/account bindings. The complete formula-engine executable and recon focused
tests passed. Formal focused API passed 4/4 and full API passed **225/225**; the full report SHA-256 is
`DAEBA5969D839E3D10266EBF1B49393D6188E54A395C62765559FDC38C2E7134`, focused report SHA-256 is
`59877DEDD7B37077C91E79848D1F8D081AE42A84DF70372704509BD4C627A7FE`. A live 800-bar `sz/000001`
day sample ended at close 11.25 with Z=12.380000114440918 and D=10.130000114440918. No full CTest was run;
the latest complete baseline remains 139/139 from TPBus 115.

Formal PID is `31280`; EXE SHA-256 is
`1D6354054F73F47BDB4A7C17E5F089727EF941C9D7294F3A66E798B7CA02B0A6`, web SHA-256 is
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`, and health matches with
`native_cpp=true` / `python_runtime=false`. PID `30820` is historical. Direct rollback is
`output/tdx-tool-formal-1d189e40-pre-zd-exact-context-rollback-20260814.exe`, SHA-256
`1D189E40A11171A098C58D449E5288AEF1537C9E001AB6EF9FDEF87185DBEF9C`.

## 2026-08-14: scalar templates for limit-price host raw context

`formulas context-template` and its GET API now emit the two exact ZTPRICE/DTPRICE host inputs under
`formula_scalar_bindings` as one null `u16-scalar` placeholder each. They are no longer duplicated once per K-line
stamp in `series`. Existing broker-private, account-state, and authorized L2 series retain their exact DATE|TIME shape;
the template never obtains, derives, or fabricates either raw value. CLI/server diagnostics now name both accepted
context containers.

The formula context-and-library, server formula-library, catalog, and recon focused tests passed. Svelte check is 0/0;
the web build succeeded with only the existing large-chunk warning. The new B007 recon contract passed 1/1, stage full
API passed **226/226**, and formal focused API passed 5/5. The full report SHA-256 is
`A08FD619BE8C993C25584139D80F331ABFBF7578364ED582F959F282020F8954`; the formal focused report SHA-256 is
`58C84EDAF2117381220A8880B0FE6898B6DA110302012F58DD3EA9B6F86EA682`. A formal `sz/000001` five-bar
sample retained the exact final Z/D values 12.380000114440918 / 10.130000114440918. No full CTest was run; 139/139
remains the latest TPBus115 baseline.

Formal PID is `2536`; EXE SHA-256 is
`B600B503FC31250019023E1803972149A05F5FE55B737BF226295CF9070755A4`, web SHA-256 is
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`, and health matches with
`native_cpp=true` / `python_runtime=false`. PID `31280` is historical. Direct rollback is
`output/tdx-tool-formal-1d635405-pre-context-template-scalar-rollback-20260814.exe`, SHA-256
`1D6354054F73F47BDB4A7C17E5F089727EF941C9D7294F3A66E798B7CA02B0A6`.

## 2026-08-14: scalar-only context templates skip K-lines

The HTTP and CLI surfaces now analyze the selected template before fetching K-lines. When
`series_binding_count=0`, market/code requests return `stamp_source=not-required-scalar-only`,
`kline_fetch_skipped=true`, requested identity metadata, and `bar_count=0`; no quote request is made. Templates with
any series binding retain the existing exact K-line stamp path. A CLI smoke with nonexistent `sz/999999` still
returned the two B007 scalars, proving the skip boundary.

Server formula-library, catalog, and recon focused tests passed. Stage focused passed 4/4, stage full API passed
**226/226**, and formal focused passed 5/5. The full report SHA-256 is
`645B28765465F95F0276716D2E5E178EA76226C40F64685473B51A46883406EF`; formal focused SHA-256 is
`66E19209032EED785DBB0A5B8BA0FBB178994EBC7428C611D8F11E4367078A1C`. This follow-up did not change web
assets or run full CTest; 139/139 remains the TPBus115 baseline.

Formal PID is `9092`; EXE SHA-256 is
`559C785798C06C9812D191C0304F28D5AB9C02670BE7D15C90783A8559F7E8C5`, web SHA-256 remains
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`. PID `2536` is historical.
Direct rollback is `output/tdx-tool-formal-b600b503-pre-context-template-skip-rollback-20260814.exe`, SHA-256
`B600B503FC31250019023E1803972149A05F5FE55B737BF226295CF9070755A4`.

## 2026-08-14: ATAN/COS/SIN numeric raw-f32 paths

The existing targeted trigonometric evidence now backs both representations the current interpreter can express.
Direct literals use the final operand, narrow it to raw f32, and broadcast the f32 result; ordinary numeric Series
narrow each operand/result to f32 and preserve missing points independently. `COS(16777217)` therefore evaluates the
native `16777216F` operand and returns `0.62632298469543457`.

The separate handler branch keyed by `Src[6*size]` passes through a `6*size+2` float composite buffer. The interpreter
has no such composite value type, so this batch neither invents metadata nor claims that branch. ATAN/COS/SIN remain
unused by the 379 system formulas; this is custom-formula compatibility with no schema, HTTP, or web change.

The native-scalars and language domains passed after a serial `tdx-tool` link. A real `sz/000001` day-five CLI and
formal POST returned final A/C/S/L values
`1.4821404218673706 / 0.25168964266777039 / -0.96780800819396973 / 0.62632298469543457` with native execution and
no retained POST body. No full CTest/API run was warranted; 139/139 remains the TPBus115 baseline.

Formal PID is `22988`; EXE SHA-256 is
`A04ECEEFA4155FDF5662434A64477A51D076873CB0624BB03A58E3943AECAE7C`, web SHA-256 remains
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`, and health matches with
`native_cpp=true` / `python_runtime=false`. PID `9092` is historical. Direct rollback is
`output/tdx-tool-formal-559c7857-pre-atan-cos-sin-rollback-20260814.exe`, SHA-256
`559C785798C06C9812D191C0304F28D5AB9C02670BE7D15C90783A8559F7E8C5`.

## 2026-08-14: current-worktree stage regression

All 390 static TCalc registry names were reconciled with the current interpreter classification. Remaining names are
explicitly market/calendar symbols, automatic or caller-owned context, L2, trading state, plugin, or action boundaries;
already completed `INSORT/INSUM` paths were not reimplemented. Missing materialized
`FINANCE#/FINVALUE#/DYNAINFO#` keys are now preclassified as exact `context_unavailable` bindings before evaluation,
without increasing errors, while supplied keys remain executable.

The complete serial MinGW build passed. Current full CTest is **140/140**, zero failures, in 32.87 seconds. The only
full-build diagnostic, a GCC 15 false positive in a test byte fixture, was removed with an equivalent fixed-size
little-endian write; all eight formula-engine domains and formula-context-aggregate passed again afterward. Formal HTTP
focused audits report HK `380/380 reported, 241 passed`, A-share `331 passed, 24 context_unavailable`, and futures
`230 passed`, all with errors=0/unreported=0. No full API run was made. HK/A-share CLI report SHA-256 values are
`8F38C9366FD9302237555FF6A840DE499DBADDACEBECBB510FCE6F121DB61516` and
`7FB16A4591B41C6C5AA911B3C6512DA39B40FC441146C4B2C92D3A45DBD6CC6B`.

The built and served executables already match, so no restart was required. Formal PID is `22988`; EXE SHA-256 is
`A04ECEEFA4155FDF5662434A64477A51D076873CB0624BB03A58E3943AECAE7C`, web SHA-256 is
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`, and health reports native true / Python false.
This 140/140 run supersedes the prior TPBus115 139/139 stage baseline.

## 2026-08-14: root-relative TPool HTTP paths

`GET /api/v1/pools` no longer exposes the configured `root`, absolute directory names, or absolute pool `source`
values. It emits `path_scope=tdx-root-relative`; the returned source round-trips directly into
`GET /api/v1/pools/evaluate`. That GET now accepts only a root-relative XML path, rejects absolute/traversal/unknown
inputs, and keeps both top-level and inspection source fields relative. Inline POST evaluation, CLI commands, and the
domain's local-file representation remain unchanged; HTTP scan failures still redact the configured root.

The TPool surface and catalog focused tests, Svelte check (0/0), and four stage plus four formal HTTP contracts passed.
After this schema change the complete serial build was warning-free and full CTest passed **140/140**, zero failures,
in 45.82 seconds. No full API suite was run; the web change is type-only and existing assets remain valid. Formal PID
is `12312`; EXE SHA-256 is `37A639B5FD0501D7156DF3977928E55C94F643B1F431466F7A4EC99BFB972986`, web SHA-256 is
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`, and health reports native true / Python
false. PID `22988` is historical. Direct rollback is
`output/tdx-tool-formal-a04eceef-pre-tpool-relative-surface-rollback-20260814.exe`, SHA-256
`A04ECEEFA4155FDF5662434A64477A51D076873CB0624BB03A58E3943AECAE7C`.

## 2026-08-14: TPBus 4650 dispatcher capture preflight

`level2 preflight --format tpbus-4650-dispatch-preflight` is a CLI-only typed diagnostic for an explicit,
legally obtained raw 4650 body. It evaluates the recovered signed-i8 `sub_10068065` size expression exactly, reports
the bounded raw market/code identity and the `raw[5] || raw[86]` / `raw[0]==1` caller gates, and fingerprints a
selected `raw+96` candidate without emitting the body. Raw and hex readers are bounded before decoding at 384 KiB.

The result intentionally keeps `dispatcher_fully_qualified=false` and `state_projection_closed=false`. Host identity,
target lookup, prior vector, live/server time, and broad mutable quote/subscription state remain unavailable, so the
tool never runs `sub_1007A75E`, writes host state, calls an SDK, dispatches a message, accesses a network, or bypasses
authorization. This does not reverse the earlier do-not-implement decision for a complete 4650 state projection; it
only productizes the statically closed validator/caller-gate boundary.

Both Level2 preflight focused targets passed. The complete serial MinGW build succeeded and full CTest passed
**141/141**, zero failures, in 11.36 seconds. No HTTP/UI/schema changed and no full API suite was run. Formal PID is
`18344`; EXE SHA-256 is `365145AE97F6C78CFE8B6DECB08B53F9C75DEA1D222917C90325C0F060BBA172`, web SHA-256 is
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`, and health reports native true / Python
false. PID `12312` is historical. Direct rollback is
`output/tdx-tool-formal-37a639b5-pre-tpbus4650-preflight-rollback-20260814.exe`, SHA-256
`37A639B5FD0501D7156DF3977928E55C94F643B1F431466F7A4EC99BFB972986`.

## 2026-08-14: strict UTF-8 JSN discovery output and context classification

Discovery field-profile previews remain bounded to 96 source bytes, but truncation now backs up to a UTF-8
code-point boundary before appending `...`. This fixes reports that became invalid JSON when a multibyte character
crossed byte 96, without changing source JSN values, GB18030 decoding, tables, or the public schema. A real 601-file
scan produced 17,053 samples, including 620 bounded previews; both CLI and formal HTTP output passed whole-document
strict UTF-8 and standard JSON parsing. The formal HTTP artifact SHA-256 is
`5A075AA634247A053779164D9BB33946D17A57EBD4648CEF89D4908F58EA0822`.

Formula audit classification also keeps missing materialized `FINANCE#/FINVALUE#/DYNAINFO#` keys as precise
`context_unavailable` rows before evaluation, rather than evaluator errors, while leaving explicit Level2/account/
private bindings unavailable unless supplied. Formal `31/00700` over 700 bars reported all 379 rows, 240 passed and
zero errors; `sz/000001` over 800 bars reported all 379 rows, 329 passed, 25 context-unavailable and zero errors.

JSN variants and formula context-and-library focused tests passed, followed by a complete serial MinGW build and
full CTest **141/141**, zero failures, in 46.38 seconds. The schema did not change; a real discovery HTTP contract was
run instead of the full API suite. Formal PID is `6852`; EXE SHA-256 is
`D1E041DD439256E8A86081DDD6682C42755E5C4554109A70E3FFF0CFB7288708`, web SHA-256 is
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`, and health reports native true / Python
false. PID `18344` is historical. Direct rollback is
`output/tdx-tool-formal-365145ae-pre-jsn-utf8-hk-context-rollback-20260814.exe`, SHA-256
`365145AE97F6C78CFE8B6DECB08B53F9C75DEA1D222917C90325C0F060BBA172`.

## 2026-08-14: unified UTF-8 prefixes and Windows-safe local timestamps

The shared `utf8_prefix` helper now owns byte-bounded JSON previews: JSN discovery remains limited to 96 source
bytes, cloud selector diagnostics to 256 bytes, and failed API-contract response excerpts to 4096 bytes. Each result
ends at a complete UTF-8 code point. The discriminator fixture puts a Chinese lead byte exactly at offset 4095; the
reported excerpt is 4095 bytes and the whole failure report is strict UTF-8/valid JSON.

Windows CRT `%z` formatted the local zone name through the active code page, which made otherwise valid JSON contain
GBK bytes. `local_timestamp_text` now computes the UTC offset and emits only ASCII
`YYYY-MM-DDTHH:MM:SS±HHMM`. All 90 active production translation units were migrated and active `%z` usage is zero.
An inventory found 103 invalid historical JSON files among 1,694 `output/*.json` artifacts; they remain untouched as
historical evidence, while newly generated reports use numeric offsets.

The native, cloud-variants, JSN, and recon-contract focused targets passed, followed by a complete serial MinGW build
and full CTest **141/141**, zero failures, in 59.13 seconds. Five formal responses (health, cloud variants, JSN
discovery, hot history, block rotation) passed strict UTF-8/JSON parsing. Focused API contracts passed 4/4; report
`output/api-contract-utf8-formal-focused-20260814.json` has SHA-256
`7225A5B808589664F04C5983FC103DD7147549A93CBBFAF90AABB4926415B217`. Real cloud variants remain 109/109 and
the regenerated report SHA-256 is `DDAF24E09626EDE111F9982F43218C96961B94F7A515C910423B4EC280058947`.
No full API suite was run.

Formal PID is `34252`; EXE SHA-256 is
`C1B2B845AB375956CF48C6653E344070C0B2FECF899C2FEB531AA7DC91529A36`, web SHA-256 is
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`, and health reports native true / Python
false. PID `6852` is historical. Direct rollback is
`output/tdx-tool-formal-d1e041dd-pre-unified-utf8-timestamps-rollback-20260814.exe`, SHA-256
`D1E041DD439256E8A86081DDD6682C42755E5C4554109A70E3FFF0CFB7288708`.

## 2026-08-14: interpreter, Level2, and user-surface completion audit

The regenerated library report covers the 379 system formulas plus one installed user formula: **380/380** source
available, syntax supported, numeric-signal safe, and presentation faithful; degraded numeric outputs and unsupported
presentation directives are both zero. `output/formula-coverage-completion-audit-20260814.json` has SHA-256
`49B08451CD009D2717CC44B03795C8EE6B6A4393EE4DC98A4CC67162872763DD`.

With explicitly read-only future evaluation enabled, the 800-bar SZ/000001 runtime audit reported all 380 rows:
351 passed, 24 caller-owned explicit-context rows, four market-inapplicable rows, and one period-inapplicable row,
with zero errors, zero unreported rows, and zero automatic-context gaps. Its SHA-256 is
`E6378DE8C5DF9AC6A2FC435296063F7A199773193A80036E41283B16CD427310`. The remaining explicit rows require
authorized SIGNALS/L2/DDE or exact type-120 raw values; no host value is synthesized.

The public Level2 header exposes 31 non-command functions plus five build/decode/project/session/preflight command
families. Every evidence-closed domain is consumed by a command or formula workflow and covered by one of 18 independent Level2/server
focused targets in the current 141-test suite. Broad host-state, live-time, SDK/authorization, and parser-less opaque
branches remain explicit preflight/opaque/do-not-implement boundaries rather than guessed semantics.

Current Svelte check is 0 errors/0 warnings and the production web build succeeds (only the existing >500 KiB chunk
notice remains). The formal full API suite passed **226/226**, zero failures; strict UTF-8/JSON report
`output/api-contracts-completion-audit-formal-full-20260814.json` has SHA-256
`068929D55743A628786011C6C6A74DEDB5C4034E094E71C7D8870DFA474D8F42`. This audit changed no production code;
the current complete build/CTest baseline remains 141/141, zero failures, in 59.13 seconds. Formal PID is `34252`,
EXE SHA-256 `C1B2B845AB375956CF48C6653E344070C0B2FECF899C2FEB531AA7DC91529A36`, web SHA-256
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`; health matches, native true, Python false.

## 2026-08-14: caller-owned context import and explanatory replay

`formulas context-import` and `POST /api/v1/formulas/context-import` now turn an explicitly caller-owned capture into
the existing `tdx-formula-explicit-context-v1` document. The importer requires `ownership_confirmed=true`, validates
every binding and `DATE|TIME` stamp against the template, rejects missing values by default, and only emits bounded
diagnostics when partial import is requested. It never opens an SDK, account session, file discovery path, or network
source. The FormulaLibrary exposes the same strict import flow and can immediately replay the materialized context.

Explicit future evaluation optionally attaches `tdx-formula-future-replay-v1`. The domain evaluates bounded successive
K-line prefixes and records only changes to points that existed in the previous prefix; the newly appended point is not
misclassified as repaint. CLI bounds are 1..64 observations and 1..10000 events. Scan, backtest, account, order, SDK,
subscription, network, and entitlement-bypass flags remain false.

AUTOFILTER keeps its v1 single flat/long/short projection and now adds an ordered decision trace. Every raw candidate
gets an additive accept/reject explanation, before/after position, effective action, composite flag, and stable rejection
reason. Raw and filtered candidates are unchanged, the trace is read-only, and `CLOSEALLD/CLOSEALLK` remain excluded.
The web view lazily renders at most the latest 100 decisions.

The root workbench, DataCenter, and ProtocolLab use route-level dynamic imports. The production entry chunk is 81,321
bytes instead of roughly 1.53 MB; 105 JS chunks are emitted and the largest is 262,156 bytes, with no Vite 500 KiB
warning. Deep-route serving for `/protocol/formulas` and `/data/block-rotation` was smoke-tested through the native
server.

Incremental verification passed the context importer and future-replay focused targets, formula language and recon
targets, Svelte check (0/0), production web build, and `tdx-tool` link. Context-import full API was 228/228; future-replay
full API was 229/229 (`EA03B795157608ECC6DFE584D355E0C273D291920C797EE47775BDCB3FC8ADAE`); the subsequently
added AUTOFILTER trace contract passed focused 1/1. A final worktree combination of context import, future replay,
AUTOFILTER trace, and the formula deep route passed 4/4; report SHA-256 is
`35E135A9B8BEF79B7A2D424FBD28C69DB091C493A6B2D61DB77C3E294AFD34CD`. Full CTest and the now-230-case full
API suite were not rerun.
Current worktree EXE SHA-256 is `9055F1ED3EC2C1155B72ECA00FF4ACC74EF5B58BC0F65B388747330A03E01951` and web index
SHA-256 is `E29140C04EEFA00067A40A1AA27754CE5D1B367912875A41D855FDF9781B3964`; this worktree has not been
installed or restarted on port 8765.

TPBus push types 113/114/116 remain opaque because the current installation has a publisher but no statically bound
consumer or descriptor. The 4650 state transition still requires live time, identity, and a broad mutable host graph.
Both remain explicit evidence-blocked boundaries rather than guessed typed decoders.

## 2026-08-14: TPBus consumer recheck and the closed 4650 price subset

The 113/114/116 consumer search now also covers the installed `QHPlugins/TTPlugin.dll`. It is byte-identical to the
analyzed copy (`8B549CEC549EAD6B3B5D07F71C6DE47AB24A0B7BF76D7189FD663344B564FBE2`), contains no
`MaintainData.HQPUSHPB` string, and none of its eight push-facing owners compares push types 113, 114, or 116. The
current installation still contains the exact topic only in `tpbus.dll`, so those protobuf business fields remain
evidence-blocked.

The complete 4650 host transition still requires live time, identity, and a broad mutable object graph, but its
120-byte `raw+96` price subset is independently closed. The new typed
`tdx-level2-tpbus-4650-price-primitives-v1` document and `level2 preflight --format
tpbus-4650-price-primitives` reproduce `sub_10066A34`, `sub_10066B6B`, and `__ftol2` from snapshot offsets `+36 u32`,
`+54 u32`, and `+114 f32`. The unresolved target `+72` scalar is caller-supplied as an opaque u32. Output contains
raw fields, the two price projections, and a snapshot digest only; it does not echo the body, compare live host
identity, execute the handler, call an SDK, dispatch a message, subscribe, or access a network.

Compact evidence SHA-256 is `E7B032F7768ED5064F99852F66CB8CEEE788D90D143C1E630EB2F938A880BBEA`. The new
price-primitives focused target and existing 4650 preflight regression target both pass. Verification report SHA-256
is `BB78797BEB4C46EAD348AFCDF7F9403B44899695DA06BCF920F44749B860C3D5`. Full CTest, full API contracts, live SDK/
network sampling, and deployment were not run.
