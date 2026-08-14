# 通达信客户端功能面与高价值数据入口

> 更新时间：2026-08-01。本页关注通达信自身还有哪些可复现能力，不讨论
> HTML 展示。证据来自当前安装 `C:\new_tdx` 的配置、公开 `7709/TCP`
> 主站和 `static.tdx.com.cn:7615/TQLEX` 的真实响应。

## 结论

当前客户端公开能力远多于已经实现的行情、板块和分钟线。收益最高的新方向
不是继续扩展页面，而是三条数据链：

1. `7615/TQLEX` 的因子、异动、F10 和资料接口；
2. `7709/TCP` 的财务、股本变迁和服务端证券目录；
3. `reqformat=22` 的 `PBRPC` 策略接口，包括竞价策略、主力资金和龙虎榜
   明细。

三条数据链现在都能由普通 Python 进程直接查询，不需要启动或注入
TdxW。`PBRPC` 已从 `TdxZdView100.dll` 恢复消息布局、两阶段取数流程和
客户端原始 `.proto`。排除 XML 注释后，当前启用的 29 个唯一 PBRPC
ReqId 已全部至少完成一次真实成功请求。

## 客户端配置揭示的功能规模

只读扫描 `T0002/cloud_cfg/*.xml` 得到：

| 项目 | 数量 |
| --- | ---: |
| XML 功能页 | 约 120 |
| 当前启用的 `datasource` 配置 | 329 |
| 有名称的服务入口配置 | 283 |
| 唯一 Entry | 49 |
| 唯一数字 `ReqId` | 63 |

扫描器会先剔除 XML 注释，因此这些数字不包含客户端保留但已停用的旧
模板；例如 `500007` 仍可作为历史证据看到，但不会再计入当前功能面。

这些 XML 不只是界面布局。每个数据源通常同时给出：

- 逻辑服务 Entry，例如 `HQServ.hq_nlp_factor`；
- `ReqId` 和服务端模块，例如 `200626 / mod_Factor.dll`；
- 参数模板、分页方式和上下游控件绑定；
- 返回列名及中文业务含义。

可使用以下脚本重复扫描，CSV 中保留了请求 Body 和返回字段：

```powershell
python doc/90-scripts/inventory_tdx_cloud_features.py `
  --root C:\new_tdx `
  --format csv `
  --output output\tdx-cloud-features.csv
```

## 新确认的 7709 功能

### 服务端证券目录：`0x044D / 0x044E`

服务端返回的不是单纯 A 股列表，而是市场完整品种目录，带名称、小数位、
昨收和类别相关字段。2026-07-31 实测数量：

| 市场 | 服务端记录数 |
| --- | ---: |
| 深圳 | 23,890 |
| 上海 | 27,649 |
| 北京 | 366 |

目录包含指数、A/B 股、ETF、债券、可转债等。它可用于：

- 不依赖本地 `.tnf` 更新证券主表；
- 发现新股、新债和退市/停牌品种；
- 用服务端小数位修正不同品种的价格解码；
- 对比本地缓存，判断客户端数据是否过期。

### 批量财务基础信息：`0x0010`

一次请求可查询多只股票，每条固定 143 字节。已确认字段包括流通股本、
总股本、EPS、资产、负债、收入、利润、股东人数、报告更新日和上市日。

实测：

| 代码 | 更新日 | 流通股本 | EPS | 净利润 |
| --- | --- | ---: | ---: | ---: |
| `SZ000001` | 2026-04-25 | 194.056 亿股 | 0.67 | 145.23 亿元 |
| `SH600000` | 2026-04-30 | 333.058 亿股 | 0.52 | 178.61 亿元 |
| `BJ920001` | 2026-07-25 | 0.887 亿股 | -0.04 | -552.29 万元 |

价值较高：可以在全市场低成本计算流通市值、总市值、换手率和基础财务
筛选，不必逐股访问 F10。

### 股本变迁与除权事件：`0x000F`

平安银行实测返回 79 条、覆盖 1990—2026 年，事件类型包括：

- 除权除息；
- 送配股上市；
- 股本变化；
- 增发、回购、缩股及重整调整。

它可以闭合前复权/后复权因子、历史流通股本和历史换手率。相比只使用当前
股本，这对长期回测更重要。

客户端本地 `T0002/hq_cache/gbbq` 是同一 29 字节记录的加密全量缓存：4 字节
记录数之后，每条前 24 字节为三个 Blowfish 块、末 5 字节保持清文。原生工具现可
用 `market capital --source local` 或 HTTP `source=local` 直接读取；默认在线
路径不变。平安银行本地 79 条与当前 `0x000F` 的 79 条逐字段完全一致。本地
DAY/LC1 K 线请求复权时会同时使用该文件，因此能够真正离线完成 qfq/hfq。

### 特殊涨跌停表：`0x0452`

当前实测返回 `SZ000010` 一条特殊限制，上限约 `1.66`、下限约 `1.36`。
这个表应作为普通 5%/10%/20% 规则之外的覆盖层，避免 ST、恢复上市或
特殊交易状态被错误计算。

### 单标的小走势图：`0x0FD1`

普通股票和板块指数均返回。实测 `selector=0/1/2` 分别得到
`80/47/61` 个价格点；`selector=3` 返回更长结构，当前单序列解析器不能
解释。该接口确实可用，但不同 selector 的业务语义还需要与客户端页面或
K 线逐点对齐，暂列“结构确认”，不应提前命名。

## 新确认的 TQLEX / F10 功能

网关地址为：

```text
http://static.tdx.com.cn:7615/TQLEX?Entry=<逻辑入口>
```

请求是普通 HTTP POST + JSON。已用 Python 标准库直接请求验证，无登录
票据、无本地 DLL 依赖。

### F10 与资料

| 功能 | Entry | 实测结果 |
| --- | --- | --- |
| 公司概况 | `CWServ.tdxf10_gg_gsgk` | 发行方式、上市日期、发行价、承销商等 |
| 主营构成 | `CWServ.tdxf10_gg_jyfx` | 平安银行最新期返回 17 行 |
| 财务报表 | `CWServ.tdxf10_gg_cwfx` | 资产负债表返回 114 个报告期 |
| 个股总评 | `CWServ.tdxf10_gg_ggzp` | 综合/资金/基本面/消息/主题评分及市场、行业排名 |
| 盈利预测 | `CWServ.tdxf10_gg_ybpj` | 未来三年 EPS、利润、收入预测和机构数 |
| 估值历史 | `HQServ.hq_nlp_gpsj` | PE/PB/PS/PCF、分位数、市值；当前页 20 行、总计 1,210 行 |
| 热点题材 | `CWServ.tdxf10_gg_rdtc` | 平安银行返回 10 个题材及入选原因 |
| 题材行情 | `HQServ.hq_nlp_tcihq` | 返回相关板块、涨跌和排序 |
| 市场排名 | `CWServ.tdxf10_gg_zxts_rqpm` | 当前名次、上次名次、前 100 名列表 |
| 沪深股通持仓 | `CWServ.tdxf10_gg_zlcc` | 返回日期、持股比例/数量和变化 |
| 公司研报 | `CWServ.tdxf10_gg_gszx` | 当前页 20 条、总计 1,107 条 |
| 公告 | `CWSearch.tzx_rcache` | 平安银行返回 91 条，含分类、来源和 PDF 地址 |
| 新闻 | `CWSearch.tzx_rcache` | 平安银行返回 100 条，含来源和发布时间 |

这里最值得复用的是“估值历史 + 题材原因 + 公告/研报”，因为它们无法从
简单行情快照推导，且服务端已经提供结构化结果。

### 基金筛选与持仓穿透

`jj_ccfx_dqcc.xml` 中旧的 PBRPC `500007` 已被注释，当前客户端实际使用
普通 JSON 入口 `HQServ.hq_nlp_risk_return`：

| ReqId | 功能 | 实测 |
| --- | --- | ---: |
| `500050` | 按类型、公司、规模、成立年限筛选基金 | 自动分页共 1,187 只 |
| `500051` | 指定基金的行业持仓 | `000326 / 20250630` 返回 11 个行业 |
| `500052` | 指定基金的个股持仓 | `000326 / 20250630` 返回 106 只 |

基金主表包含净值、涨跌、换手、持仓集中度、行业集中度、基金规模、公司、
成立日和基金经理；持仓明细包含数量、市值和占比。季度末不一定有完整
持仓穿透，实测半年报和年报日期可返回数据。

上述链路现已固定到纯 C++ `market fund-analytics` 的三类 `reported-*`
视图。默认报告期明细为空时会按半年报/年报倒序寻找最近有数据的完整披露期；
响应分别保留 `requested_report_date`、`resolved_report_date` 和回退标志。
显式报告期不自动回退，且缓存键与默认回退请求隔离。

新增的 `tdx_tqlex.py` 已把这条链路工具化。使用 100 行页长自动请求
12 页，`500050` 共合并出 1,187 只普通股票型基金；随后可把主表的
`fund_code/reportDate` 传给 `500051/500052`。同一工具还验证了：

- `200661 / flag=2`：T+0 机会，当前返回 37 条；
- `2044`：异常波动 877 条，另有一个 `total_num=877` 汇总结果集；
- 小写 `page/pageSize` 和大写 `Page/PageSize` 都会沿用模板原始键名；
- 分页只合并满页的主结果集，不会重复追加每页相同的汇总结果集。

## 云配置中已验证的策略/事件功能

以下入口由当前安装的 XML 直接给出，并已真实请求返回 `ErrorCode=0`。

### 因子库与盘中机会

Entry：`HQServ.hq_nlp_factor`

| ReqId | 功能 | 实测 |
| --- | --- | --- |
| `200626` | 普通因子目录和 120 日回测 | 33 个因子 |
| `200636` | 指定普通因子的股票列表 | 因子 24 完整分页 2,524 只 |
| `200646` | 股票因子看板 | 完整分页 5,186 只；客户端默认只显示 200 |
| `200650` | 形态因子目录和 120 日回测 | 57 个因子 |
| `200651` | 指定形态因子的股票列表 | 十字星因子 115 只 |
| `200660` | 分时雷达 | 当前 462 条，安全分 61—100 |
| `200661 flag=1` | 个股盘中机会 | 当前页 50 条 |
| `200661 flag=2` | T+0 机会 | 46 条 |
| `200662` | 积突、高低、上下、破立、踩拉、托压信号 | 多个 flag 模板已恢复 |

因子目录不只是名称，还返回服务器统计的：

- 今日、近 5 日收益；
- 过去 120 日入选后的 1/3/5/10/20/60 日平均收益；
- 最大回撤；
- 夏普比值。

2026-08-06 已新增纯 C++ `market factors` 和固定
`/api/v1/market/factors`，将上述六条请求统一为 `catalog`、`members`、
`dashboard`、`patterns`、`pattern-members`、`intraday-radar` 六个视图。
普通/形态明细中的成交量、市值、行业等并非 `hq_nlp_factor` 返回列，而是
原客户端 `syscol` 行情列；工具通过显式 `quotes=1`/`--quotes` 才调用公开
`0x054C` 补齐，避免把客户端拼接字段误称为因子服务字段。工具默认自动翻页，
`--first-page` 可复现原客户端首屏。百分数字段按上游百分点原值输出，不除以 100。
`security` 视图使用完整 `200646` 按 `market+code` 精确定位，再按因子名称
连接 `200626`，返回普通因子 ID、描述、族、输出位置和回测；平安银行当前
命中 7 个且目录连接 7/7。`formulaName` 属于服务端 `mod_Factor.dll`，不是
TCalc 公式代码：例如“MACD 水上金叉”与本地只判断金叉的 `MACD买入` 不等价，
“RSI<30”也不等价于本地向上穿越 20 的 `RSI买入`，因此接口明确不声称
可由相似 TCalc 公式精确复核。形态因子仅发现 `200651` 正向明细，没有直接
反向看板，当前不为单票串行扫描 57 个形态因子。

随后已将这两个边界转成显式、可选的完整矩阵，而不是隐藏扫描成本：

- `standard-matrix`：33 个普通因子逐一完整分页 `200636`，当前 33/33 成功、
  16,863 条因子—证券关系；
- `pattern-matrix`：57 个形态因子逐一完整分页 `200651`，当前 57/57 成功、
  4,825 条关系、3,032 只唯一证券，其中 16 个因子当前为空；
- `security&include_patterns=1`：复用完整形态矩阵反查单票，并保留命中的
  原 `200651` 成员行作为证据；平安银行当前命中“龙腾四海”；
- `overview`：从完整 `200646` 统计 33 个因子的覆盖率、平均安全分、当日/
  十日收益、信号数分布和 365 组实际共现，再与普通矩阵逐因子对账。

真实对账说明 `200646` 的因子标签集合不能替代 `200636` 直接成员：当前仅
6/33 个因子集合完全一致，27 个存在差异。净利润同比增长在看板为 2,670 只，
直接成员为 2,524 只，交集 2,524、看板独有 146、直接独有 0；济安线金叉
则两侧都存在独有证券。接口同时输出 `dashboard_only_count`、
`direct_only_count`、交集和 Jaccard，不将任一侧静默覆盖另一侧。

CLI 的 `--snapshot` 为任意未截断因子视图建立稳定身份并原子更新 JSON 状态；
普通/形态目录按因子 ID，成员/看板按证券 ID，分时雷达按“证券+因子+入选
时间”识别。被 `limit` 截断或因子级请求部分失败的结果会拒绝保存，避免一次
不完整请求制造大批假移出。

这比 TCalc 的公式名称列表更进一步：TCalc 提供本地公式定义，这里提供
通达信服务端预计算的选股结果和历史表现。

### 异动与停牌风险

| 功能 | Entry / ReqId | 实测 |
| --- | --- | ---: |
| 异常波动统计 | `HQServ.hq_nlp_abnormal_gold / 2044` | 总计 877 条 |
| 异动停牌风险 | `HQServ.hq_nlp_tcihq / 200720` | 176 条 |
| 利润断层 | `HQServ.hq_nlp_caps / 500601` | 当前 69 条；客户端安全分过滤后 65 条 |

异动数据包含个股与分类指数的 3/10/30 日涨幅和偏离值；停牌风险包含计算
区间、偏离值、触发标准、触发价格和近十日异动次数。它们是客户端已经
计算好的事件列表，不需要自行扫描全市场 K 线。

两条现已固定为纯 C++ `market anomaly-risk` 和
`/api/v1/market/anomaly-risk`，与 PBRPC `500107` 的十九类异常证券列表保持
独立。当前 `2044` 代码稳定排序完整分页为 863 只，沪深北分别 457/385/21，
10 条带参考价提示；`200720` 为 178 只，3 条 `N014=3` 映射为“触发异动”。
`N010` 是比例，接口同时保留原比例并输出乘 100 后的百分点；零停牌/复牌日
转换为空值。统计返回的 N029—N037 尚无配置或模块证据证明字段名，因此原值
放入明确的未解析对象，不用相似概念冒充。

PBRPC `500107` 的两个普通 JSON 从属请求也已固定为
`market abnormal-details`：`500109 summary` 结构化刷新时间、服务端报告条数
和可空预测准确率；`500108 explanation` 结构化原因、累计偏离、股数、万元/元
双金额及异常起止日。两条当前均声明 `ColNum=2`、实际宽度为 1，接口以解码
内容为准并暴露 `consistent=false`；未上榜证券的一行空 `Detail` 转为空关系。

`500601` 已进一步固定为纯 C++ `market profit-gaps` 和
`/api/v1/market/profit-gaps`。响应将 `Bglx` 明确映射为一季/半年/三季/年度
的预告、快报或正式报告，并把断层涨幅与安全分转换为数值；默认执行 XML 中
`Aqf>=60` 的客户端过滤，也可设 `minimum_safety=0` 审计完整上游集合。当前
事件日期为 2026-06-26 至 2026-08-06，华海药业命中 2026-07-06 半年度预告、
断层涨幅 7.59%、安全分 79。原网格中的现价、成交量额、市值和行业为宿主
`syscol`，不属于 `500601` 返回字段。

### 其他已返回数据的模型

| 功能 | Entry / ReqId | 实测 |
| --- | --- | ---: |
| 指数估值比概览 | `HQServ.hq_nlp_tciFetcher64 / 200000` | 180 个指数 |
| 单指数已实现波动率序列 | `HQServ.hq_nlp_indexCorr64 / 200003` | 当前两年 485 日 |
| 指数历史波动率及分位数 | `HQServ.hq_nlp_indexCorr64 / 200004` | 当前 100 个指数 |
| 全收益指数与价格指数差 | `HQServ.hq_nlp_copilot / 200770` | 当前年度 68 组 |
| 北向资金区间统计 | `HQServ.hq_nlp_fuan / 500501` | 16 档 |
| 融资融券率区间统计 | `HQServ.hq_nlp_tciFetcher64 / 200009` | 11 档 |

前三条指数模型现已固定为两个纯 C++ 业务接口。`market index-volatility`
提供 `catalog/history/security` 三个视图，`Window` 明确表示滚动交易日数，
所有波动率与分位字段均保持百分点单位；不存在的指数返回空关系而不是错误。
`market total-return-gap` 将 `zoneZDFGap` 解释为“全收益指数区间收益减价格指数
区间收益”，同时保留两侧起止指数值。当前 68 组全部为正，均值约 2.229 个
百分点；深证成指为 4.578% 对 5.534%，差 0.956 个百分点。

当前配对中 39 个价格指数、62 个全收益指数使用内部市场号 62，工具保留
`62:<code>` 身份，不映射成伪造的沪深代码。原网格的“当日收益差”由两侧
实时 `syscol` 在客户端计算，`200770` 仅返回区间差，因此固定接口显式标记
`daily_gap_available=false`。

后两条资金分档也已并入纯 C++ `market flow-followup`。`200009/200010` 的
Type 1/2 分别形成融资率和融券率模型，`500501/500502` 的 Type 1/2 分别形成
北向净流入和净买入模型；每个模型同时返回客户端固定分档、样本数、沪深300
后续 1/3/5（两融另含 10）日平均涨幅/上涨概率，以及从文字摘要结构化出的
当前余额、比率或资金额。北向两条摘要会配对检测最新 `1040/0` 占位值；分档
历史保留，但当前信号和当前分档均标为不可用。

### 基金风险、能力与仓位模型

在当期持仓 `500050—500052` 之外，客户端还暴露了完整的基金分析模型：

| ReqId | 功能 | 实测 |
| --- | --- | ---: |
| `500030` | 收益、风险、最大回撤及夏普/索提诺等风险调整指标 | 20 行样本 |
| `500031` | 单基金与基准的每日收益走势 | 48 |
| `500032` | 月收益、波动率、胜率及牛熊平均收益 | 20 行样本 |
| `500033` | 单基金与基准的月度收益走势 | 37 |
| `500035` | C-L/T-M/H-M 三种模型的择时与选股能力 | 20 行样本 |
| `500055` | 区间平均换手/集中度及其稳定性 | 20 行样本 |
| `500056` | 单基金区间行业平均持仓 | 6 |
| `500057` | 单基金各报告期换手、集中度和股票仓位 | 7 |
| `500060` | 报告仓位与指定日估算仓位 | 20 行样本 |
| `500062` | 全市场股票/混合基金估算仓位日序列 | 242 |

上述十条与前述三条报告期持仓现已统一固定为纯 C++ `market fund-analytics` 与
`/api/v1/market/fund-analytics`：

- `risk/risk-history`：累计/年化及超额收益、波动、下行风险、最大回撤、
  Sharpe/Sortino/Omega/信息比率、捕获率、Beta/相关系数/R²和逐日基准对比；
- `monthly-risk/monthly-history`：月收益极值、胜率、牛熊收益、月波动和逐月
  基准/超额序列；
- `selection-skill`：C-L、T-M、H-M 三类模型的择时、选股、Alpha 和 Beta；
- `reported-holdings/reported-holding-industries/reported-holding-securities`：
  基金报告期主表、行业持仓和股票持仓，默认明细支持最近完整披露期回退；
- `holdings-stability/holding-industries/holding-history`：换手、个股/行业集中度、
  稳定性、区间行业平均市值以及逐报告期股票仓位；
- `position-estimates/market-position-history`：单基金报告仓位与估算仓位，以及
  普通股票型、偏股混合型和合并市场仓位历史。

风险、月度风险、择时选股和持仓稳定性当前完整集合均为 1,188 只唯一基金；
仓位估算为 1,131 只。服务端风险宽表在页长 200 时返回 ErrorCode 2“输出缓冲
区不足”，固定接口使用页长 50 完整翻页，`first_page=1` 才复现客户端首屏。
2026-08-06 当日仓位尚未发布，接口探测到全零占位后回退 2026-08-05；当天
市场合并估算仓位为 84.48%。

月度明细存在混合单位：基金/基准收益是比率，而超额收益已经是百分点。工具
只对前两者乘 100；例如 000711 的 2026-08 月收益从 `0.0414` 规范化为
4.14%，基准从 `0.0120` 规范化为 1.20%，超额保持 2.9332 个百分点。

`500030` 还返回上/下行捕获率、跟踪误差、信息比率、Omega、Jensen、
Beta、相关系数和 R²；`500035` 不是“指数相关”页面，而是基金经理的
择时/选股能力模型。完整 34 个普通 JSON ReqId 均已真实返回
`ErrorCode=0`，详见
[覆盖记录](../99-log/2026-07-31-tqlex-json-tool.md)。

## 已打通的 reqformat=22 / PBRPC 功能

`reqformat=22` 的 XML Body 是客户端内部描述符，不是 HTTP 请求正文。
`TdxZdView100.dll:sub_100025B0` 解析描述符并构造 protobuf；
`sub_102A3770` 为格式 22 创建类型 `1144` 的 FetchDataHandle，
`sub_102ABDE0` 序列化并交给 `TPData100.dll` 发送。

客户端使用以下外层消息：

```text
pb_rpc_req
  1 Head(PBPublicReqHead)  2 RpcID  3 StartPos
  4 Moduledll             5 ReqByte(JSON 字符串)

pb_rpc_ans
  1 Head(PBPublicAnsHead)  2 RpcID  3 StartPos
  4 TotalLen              5 RetByteNum  6 RetByte
```

真实调用是两阶段/可分页 RPC：

1. 首包令 `RpcID=0, StartPos=0`；
2. 服务端先分配 `RpcID`，通常不带业务数据；
3. 后续请求带回该 `RpcID` 和当前 `StartPos`；
4. 累积 `RetByte`，直至 `StartPos + RetByteNum == TotalLen`。

HTTP 地址仍是：

```text
http://static.tdx.com.cn:7615/TQLEX?Entry=<XML 中的 datasource name>
```

正文必须是二进制 `pb_rpc_req`，`Content-Type` 为
`application/octet-stream`。直接发送 XML 中的描述符会得到 503。

### 客户端原始 proto

`TdxZdView100.dll:sub_10376A50` 初始化 `proto\` 目录；
`sub_1000C430` 用内置口令解开 `proto\zdproto.dat`。当前安装中已恢复
12 个 schema，包括：

- `protocol_mp.proto`：`pb_rpc_req/pb_rpc_ans` 与公共请求头；
- `QuantDB.proto`：`ReqSelect/AckSelect`、插入、更新、删除和任务进度；
- `TdxTaskArgs.proto`、`TciTaskArgs.proto`：服务任务参数；
- `QuantDBTable*.proto`、`QuantOutPut.proto`：量化任务和结果表。

这证明 `ReqSelect/AckSelect` 属于同一 protobuf 注册系统，但当前云页面的
PBRPC 数据正文仍是 JSON；调用工具不需要安装 protobuf 第三方包。

### 真实返回的功能

下表均为 2026-07-31 的一次盘后请求，行数会随交易日变化：

| ReqId | 客户端功能 | 本次返回 |
| --- | --- | ---: |
| `200404` | 竞价爆量 | 10 |
| `200400` | 烂板转强 | 4 |
| `200401` | 炸板转强 | 7 |
| `200402` | 竞价止跌 | 592 |
| `200403` | 预吞上影 | 77 |
| `200405` | 涨停高开 | 33 |
| `200406` | 5 分钟陡增 | 208 |
| `200225` | 事件驱动 | 322 |
| `200250 / XgName=CXGXG` | 创新高 | 2 |
| `200316` | 强势启动 | 14 |
| `200320 / market=1` | 主板上涨通道、压力/支撑与安全分 | 90 |
| `200325 / market=1` | 主板下跌通道、压力/支撑与安全分 | 1,446 |
| `200340` | 市场/指数分时段主力资金 | 39 |
| `200341 / 881001` | 煤炭行业成分股分时段主力资金 | 32 |
| `200451` | 上涨九转、下跌九转 | 143 |
| `500107 / Type=0` | 全市场龙虎榜/严重异常证券 | 63 |
| `200001` | 180基建相对上证指数的 PE(TTM) 估值比 | 484 |
| `200011` | 融资融券率与沪深 300 后续表现 | 2,113 |
| `200302 / PE_type=1` | 30 个行业的 PE(TTM) | 30 |
| `200300` | 平安银行近三年 PE 历史及分位 | 728 |
| `200301` | 煤炭行业成分股 PE/预期估值 | 32 |
| `200305` | 30 个行业的 PB-ROE | 30 |
| `200303` | 煤炭行业成分股 PB-ROE 估值 | 32 |
| `200452` | 个股 RPS 默认三周期组合 | 62 |
| `200453` | 板块 RPS 默认三周期组合 | 10 |
| `200199` | 煤炭板块成分股区间回测 | 32 |
| `200327 / highLowFlag=1` | 创新高 | 37 |
| `200327 / highLowFlag=0` | 创新低 | 131 |
| `200329` | 横盘突破 | 48 |
| `500503` | 北向净流入与沪深 300 后续表现 | 2,227 |

其中竞价返回昨日涨幅/成交额、竞价涨幅/成交额以及炸板、上影、9:20
匹配额等策略专用字段；`200340` 返回今日、9:35 前、10:30 前、
10:30—11:30、13:00—14:00、14:00—15:00、尾盘 30 分钟七段的主力
净额和成交占比；`500107`
返回异常类型、市场、涨跌幅、振幅、换手、成交额、成交量和主力净额。

估值模型使用客户端原生默认值：PE(TTM)、3% 要求年回报率、近 36
个月；个股 RPS 默认 `10日/90 + 20日/90 + 60日/90`，板块 RPS 默认
相同周期的 85 分位。`200199` 返回的是所选板块每只成分股在区间内的
涨跌、最大涨幅、振幅、最大回撤、成交量额、换手和资金净流入，不是单根
板块 K 线。`500503` 支持超过 64 KiB 的多轮分页，本次分三段拼出
138,158 字节。

2026-08-06 已把这五条估值请求从通用 PBRPC 调试入口固定为纯 C++
`market equity-valuation` 和 `/api/v1/market/equity-valuation`。五种视图
分别为 `pe-industries`、`pe-security-history`、`pe-industry-members`、
`pb-roe-industries`、`pb-roe-members`。当前复验得到 30/726/32/30/32 行；
输出将 A 股市场与 `881xxx` 行业市场分开校验，统一日期和数值，并将
`1/2/3` 判断码显式映射为高估/低估/合理。接口只暴露固定配置，不接受任意
上游 URL；对偶发 `RpcID=-1` 会有限重试并允许返回同参数陈旧缓存。

`200001` 的上游普通 JSON `200000` 先返回指数及当前估值比，再由
PBRPC 返回所选指数的每日估值比走势。2026-08-06 已固定为纯 C++
`market relative-valuation` 和 `/api/v1/market/relative-valuation`，支持
五类指数、五个基准和 PE(TTM)/PB(MRQ)/PS(TTM)。默认宽基主表 180 行，
`SH000026` 返回 2024-08-06—2026-08-05 共 484 个交易日；主表另含 21 行
内部市场号 62，接口保留为 `tdx-62` 而不猜测交易所。`200011` 从 2017-11-15 到
2026-07-30 返回 2,113 个交易日，225,911 字节分四段数据完成拼包。

2026-08-06 已将 `200011` 和 `500503` 固定为纯 C++
`market flow-followup` 与 `/api/v1/market/flow-followup`。最新复验中融资融券
模型为 2,117 行；北向内容数组为 2,231 行，但服务端错误声明
`RowNum=1/ColNum=2`，接口因此明确以解码后的 7 列内容为准并暴露元数据不一致。
北向 `2024-08-19` 起连续 460 行报告 `1040/0`；这些原值仍保留为审计证据，
但有效净流入/净买入置空且 `signal_available=false`。`available_only=1` 只返回
截至 2024-08-16 的 1,771 个可用资金信号。所有后续涨幅统计只使用可用信号，
并明确是相关历史表现而非预测或因果结论。

`200341` 的正确语义是“行业板块成分股资金流”，不是任意宽基指数的资金
明细。对 `880008/999999` 等宽基代码请求会返回 `RpcID=-1`；使用上游
`200340` 中的行业代码 `881001` 后成功返回煤炭行业 32 只成分股。因此
负 RpcID 在这里表示业务参数不被该从属查询接受，不表示协议或服务下线。

2026-08-01 已把这条主从链全量工具化：39 条总表中识别出 30 个
`881xxx` 一级行业，`200341` 分 30 次返回 5,543 条成分记录和 5,543 只
唯一证券，没有跨一级行业重复归属。多个超过 64 KiB 的行业响应也完成了
RpcID 分段拼包。输出为 `output/tdx-intraday-funds.json`，股票反向索引
引用行业成分行，不重复保存七段资金数据。

2026-08-06 又把高频技术选股页从通用 PBRPC 参数层提升为纯 C++ 业务接口：

```powershell
tdx-tool market technical-signals --root C:\new_tdx `
  --view nine-turn --direction up

tdx-tool market technical-signals --root C:\new_tdx --view rps-stock

tdx-tool market technical-signals --root C:\new_tdx `
  --view trend-up --board main
```

固定 API 为 `/api/v1/market/technical-signals`。它支持 `nine-turn`、
`rps-stock`、`rps-block`、`new-high`、`new-low`、`breakout`、`strong-start`、
`trend-up`、`trend-down` 和 `event-driven`；默认复现 XML 的客户端过滤，
`raw=1`/`--raw` 可返回规范化但未应用界面过滤的集合。响应不暴露任意上游 URL，
只返回固定请求编号、配置来源、RpcID、分页轮次和数据大小。

同文件中 ReqId `200250` 的七个固定模型也已并入：`model-new-high`、
`two-day-event`、`liquidity-space`、`limit-break`、`low-turnover-chase`、
`high-turnover-chase`、`high-liquidity-enhance`。响应保留客户端中文名称、
`XgName` 和 XML 已披露的筛选说明；“高流动性增强”未在 XML 披露规则，接口
显式返回 `client_rule_disclosed=false`，不按名称猜测算法。

`sc_jjcl.xml` 的七个竞价策略也已并入：`weak-limit-reversal`（烂板转强）、
`failed-limit-reversal`（炸板转强）、`auction-bottom-reversal`（竞价止跌）、
`upper-shadow-engulf`（预吞上影）、`auction-volume-spike`（竞价爆量）、
`limit-up-gap`（涨停高开）和 `five-minute-volume-surge`（5 分钟陡增）。
规范化层执行 XML 的 `yesterdayGrowth/100`、竞价额占昨日成交额公式，并仅对
涨停高开复现已披露的 `strongStyle=1` 筛选；未披露的选股规则保持为空。

传入 `market=sz&code=000001` 时，API 顺序检查三十二个固定视图并返回单票命中。
2026-08-06 又将同页 `200661/200662` 八个普通 JSON 盘中视图固定化：个股
机会、T+0 机会、积突、高低、上下、破立、踩拉和托压。接口复现安全分过滤，
并把客户端行情列与上游七列信号源分开；`quotes=1` 才按需补公开 L1 快照。
二十三类个股结果按规范化证券 ID 匹配，板块 RPS 则通过本地板块成员关系反查。
每个视图独立报告 `live/stale-cache/truncated/error`；只要有页面失败或旧缓存，
`complete=false` 且 `absence_conclusive=false`，不会把上游失败误报成未入选。
CLI 另支持 `--snapshot FILE`，对相同视图、相同参数的前后集合做实体级
`added/removed/changed/unchanged` 比较，校验成功后再原子替换状态文件。

同一客户端功能面中的 `BK_BKLSHC.xml` 已固定为：

```powershell
tdx-tool market block-backtest --root C:\new_tdx `
  --category industry --begin 2026-07-01 --end 2026-08-05

tdx-tool market block-backtest --root C:\new_tdx `
  --block-code 880471 --begin 2026-07-01 --end 2026-08-05
```

固定 API 为 `/api/v1/market/block-backtest`。页面的 `CodeList=12:0..4` 分别
对应全部、行业、概念、风格和地域；选中板块后，详情请求切为
`CodeList=2:<板块代码>|1`。返回字段按客户端列名恢复为区间收益、单日最大
涨幅、振幅、最大回撤、换手、成交量/额、净流入和主力净流入，并默认按收益
降序。成员集合是该次上游服务为板块选择的证券，不冒充历史时点成分重建。

### 可复用工具

```powershell
# 查看当前客户端暴露的所有 PBRPC 配置
python doc/90-scripts/tdx_pbrpc.py --root C:\new_tdx --list

# 竞价爆量
python doc/90-scripts/tdx_pbrpc.py `
  --root C:\new_tdx --req-id 200404 `
  --output output\tdx-pbrpc-200404.json

# 一次更新七类竞价信号，并建立股票反向索引
python doc/90-scripts/update_tdx_auction.py `
  --root C:\new_tdx --download --compact

# 取得单票开盘+收盘集合竞价逐点序列
python doc/90-scripts/tdx_auction_series.py `
  --root C:\new_tdx --security sz000001 `
  --download --compact

# 当日/历史 L1 成交明细，以及 09:25/15:00 正式撮合
python doc/90-scripts/tdx_trades.py `
  --root C:\new_tdx --security sz000001 --date 20260731 `
  --download --compact

# 开盘抢筹榜 + 虚拟竞价序列 + 正式撮合 + 当日/昨日竞价参考量额
python doc/90-scripts/update_tdx_auction_quality.py `
  --root C:\new_tdx --count 30 --download --compact

# 更新 30 个一级行业、全部成分股的七段主力资金及反向索引
python doc/90-scripts/update_tdx_intraday_funds.py `
  --root C:\new_tdx --download --compact

# 0x054B 服务端封单额降序翻页，直到第一条非封板证券
python doc/90-scripts/tdx_category_quotes.py `
  --root C:\new_tdx --sort seal-amount --all-sealed `
  --download --compact

# 0x06B9 更新估值、流通股本、封单、竞价和涨停统计
python doc/90-scripts/tdx_stats.py `
  --root C:\new_tdx --download --compact

# 合并涨停/炸板、实时五档、历史封单、连板和板块梯队
python doc/90-scripts/update_tdx_limit_quality.py `
  --root C:\new_tdx --download --compact

# 全市场龙虎榜；result 对应客户端 Type 控件
python doc/90-scripts/tdx_pbrpc.py `
  --root C:\new_tdx --req-id 500107 --set result=0 `
  --output output\tdx-pbrpc-500107.json
```

工具会从 `cloud_cfg` 自动选择 Entry、模块和请求模板，替换动态参数，
完成 RpcID 握手、分页拼包、尾随 NUL 清理和 JSON 解码。同一 ReqId
存在多个模板时可用 `--body-contains TEXT` 精确选择，例如：

```powershell
python doc/90-scripts/tdx_pbrpc.py `
  --root C:\new_tdx --req-id 200199 `
  --body-contains '2:$$code$$|1' `
  --set code=881001 --set AdjustType=1 `
  --set BeginDate=20250101 --set EndDate=20260731
```

### 客户端实现链

对 `TdxW.exe.i64` 的字符串交叉引用和工厂对象追踪得到：

```text
TdxW.exe:sub_730FC0
  └─ LoadLibrary ZDPlugins\TdxTopicView100.dll
       └─ IData_GetObject()
            ├─ 创建云页面对象（工厂虚表 +84）
            └─ 加载 cloud_cfg\<name>.xml（页面对象虚表 +28）

TdxTopicView100.dll
  ├─ TPData100.dll       会话/传输接口
  └─ TdxZdView100.dll    datasource 解析、reqformat 分派、protobuf/PBRPC
```

`TdxW` 只负责选择 XML、创建页面对象并注入三个宿主回调；它没有
`pb_rpc_req` 或 `reqformat` 字符串。相反，`TdxZdView100.dll` 中同时存在：

- `pb_rpc_req` / `pb_rpc_ans`；
- `reqformat`、`datasource`、`reqid`；
- `ReqSelect` / `AckSelect`、`ReqDelete` / `AckDelete`；
- “发送请求 数据包号、reqformat、name、body”日志格式；
- 静态链接的 Google protobuf 运行时。

这条调用链与真实服务响应已经相互验证：对 `reqformat=2/22`，
`TdxTopicView100.dll` 负责页面对象边界，`TdxZdView100.dll` 负责
描述符和 protobuf，`TPData100.dll` 负责会话与收发；`tpbus.dll` 不在
该功能的数据路径上。

`reqformat=11` 是另一条已经闭合的分支：`TdxZdView100.dll` 把资源任务
经操作码 40 回调给 TdxW，由 TdxW 使用公开 7709 连接和命令
`709/1721` 下载 `bi/<路径>`，不经过 TPData。行业/主题资源已经恢复
5,534 只股票、110 个叶子行业和 1,775 个主题，详见
[JSN 静态资源协议](09-jsn-resource-protocol.md)。

该分支还有一组先前未进入 XML 资源清单的战略主题主表：24 份大类
`.cfg` 通过 `file="func_*.jsn"` 给出 567 个内部主题 ID，随后由
`zttzty/<主题ID>.jsn` 返回逐股入选逻辑与说明。当前已把主表发现、详情
选择性更新和股票反向查询工具化。

同样的 CFG 路由在客户端中远不止战略主题：当前扫描得到 542 个直接
`file=*.jsn` 候选，其中 521 个未出现在 XML datasource 清单。分批验证的
62 个主表已确认市场关注度/舆情热度、机构持仓季度变动、股东人数、盘后
成交、事件要闻、风险亮点、连板梯队和板块多周期异动等功能。

其中四个高收益资源族已经闭合：机构持仓 24 表、股东人数 5 表、资金
强势 5 表、龙虎榜 8 张主视图。龙虎榜主表用 `$ZQDM` 保存事件 ID，动态
资源 `lhbfx/<事件ID>.jsn` 再返回营业部、买卖方向、买入/卖出/净额、历史
成功率、估算成本和收益；它比只按股票汇总的龙虎榜页面多了一层可归因
明细。资金强势榜还直接包含 DDX、主力净流入和区间表现。

机构持仓已继续闭合到三级关系。`cgfxmx1/<市场><代码>.jsn` 返回单票历期
机构分类汇总，`cgfxmx2/<市场><代码>.jsn` 返回当前十大流通股东及股东
ID。官方股东进出页再调用 `CWServ.tdxf10_gg_gdyjcgmx`：`gdjc` 按股东
反查其曾出现的股票，`gdjcxq` 按股东和股票展开逐报告期变化。高盛样本
覆盖 1,579 只股票，爱丽家居子查询返回 5 个报告期。

公司行动和交易统计也已闭合。28 张公司行动主表覆盖股东/董监高增减持、
质押/解押、股份回购、解禁和分红；7 张大宗交易主表覆盖月度统计、个股
成交、营业部画像和意向申报；4 张股权关联主表给出行业、地方国资改革、
整合关系和公司系及其证券集合。动态层可继续展开单票、月份、行业、质押
机构、营业部和回购年份，详见[公司行动与交易统计归档](../99-log/2026-08-01-corporate-actions-trading-resources.md)。

融资融券和沪深港通又新增 32 张主表。两融链可从个股或 ETF 展开近三月
明细，并从市场日期进入行业、概念、风格排行及约 1,900 日分类趋势；互联
互通链可取得日/周资金、十大活跃股、港股持股历史、2026Q2 陆股通持仓、
港股行业资金和成分。活跃龙虎榜历史异动及热点会议关联股也已恢复。部分
陆股通增减仓排行停在 2024-08-16，只能作为历史快照使用。

## 本地基金参考数据

客户端还维护三份不经过云页面的本地基金资源。`specjjdata.txt` 是固定 7 列的
基金份额与参考值快照；原宿主把万份单位的份额写入证券记录 `+86/+132`，把
单位交易参考值写入 `+244`，并保留但不消费已发布单位净值列。
`specetfdata.txt` 是固定 8 列的 ETF→跟踪标的表，原生记录为 48 字节，包含
内部目录 ID、两个日期以及状态 1/2/3。
`speclofdata.txt` 是固定 6 列、40 字节的 LOF→跟踪标的表，复用同一指数别名
规则，但没有日期窗口。

`market fund-reference` 与 `/api/v1/market/fund-reference` 已按这套语义严格
解析当前 2,113 条快照、1,669 条 ETF 映射和 431 条 LOF 映射。接口支持按证券、
标的和目录 ID 查询，
并复现 `IXIC/NDX/NBI`、上证综指的原生别名规则；全程只读本机文件且不发网络
请求。字段还与 ETF 公开资料交叉对账：`SZ158006` 的本地第 7 列与 `DWJZ`
一致，因此第 6 列保持中性的 `unit_reference_value`，避免误标为收盘价或净值。
`SZ160125` 的本地 LOF 标的为市场 27 的 `HSI`，而现有云端 LOF 表没有标的字段。

## 本地 K 线历史热点区间

`speczshot.txt` 不是技术指标或板块表，而是客户端叠加在 K 线图上的历史热点
区间。原生 loader `sub_5DFD00` 生成 1,110 字节记录，K 线消费者
`sub_9912D0/sub_987540` 按起止日期绘制区间框，并展示主题、证券和收益。

`market hot-history` 与 `/api/v1/market/hot-history` 已类型化当前 210 条多年记录。
第 7 列是终点收盘相对起点前复权基准的区间收益，第 8 列是区间最高价相对同一
基准的峰值收益；`from/to` 采用区间重叠查询，可直接服务于图表可见窗口。
分析文本含额外管道时，接口既保留客户端实际消费的第 9 段，也恢复完整尾部。
现有云端 `RDHS101` 仍负责近期爆炒复盘，两者不互相替代。

## 本地指数图事件注记

`speczsevent.txt/speczsevent_ds.txt` 是指数 K 线的重大事件叠加层。原生类型 0
用于境内指数和 `8800` 板块，类型 1 用于恒生指数族，类型 2 用于纳指、道指和
标普指数族。记录中的 `MMDD` 表示发生日，完整日期表示目标市场绘图交易日；
`recid` 经 `ZSEventUrl` 模板打开正文页。

`market index-events` 与 `/api/v1/market/index-events` 已恢复当前 162 条注记、
18 个休市日映射和正文目标。它与 `market event-impact` 分工明确：前者离线、
保留原生图表日期/recid/11 条旧上证事件，后者联网提供更广的 583 条事件及
前后指数冲击收益。

## 本地港股公司行动与复权因子

`hkqxinfo2.dat/hkqxinfo.dat` 是加密的长期港股公司行动链，不是近期公开资料页的
重复缓存。宿主用固定 Blowfish 密钥解密五列记录，并把累计乘数/偏移写入 85 字节
记录供 K 线复权消费者使用。相邻记录可以精确还原单次股份乘数和加性价格调整；
供股的加性项允许为负，长期合股后累计乘数也可能远小于 `0.0001`。

`market hk-actions` 与 `/api/v1/market/hk-actions` 已类型化当前 31,183 条、
2,380 只港股、1973 至 2026 年的历史，支持事件标签、代码、日期、文本、排序和
分页。它与 `market hk-events` 分工明确：前者全离线、历史长且带原生复权因子；
后者是近期分红公告、权益披露、沽空统计和上市申请。解析结果按两个文件的大小和
修改时间失效缓存，重复单票分页无需再次解密。

## 攻坚优先级

1. **P0：固定 API 契约回归。** 对正常空关系、显式筛选无命中、瞬时负
   RpcID/HTTP 失败和陈旧缓存降级做跨接口自动化冒烟，避免网页把空数据当报错。
2. **P0：重复模板与参数变体矩阵。** 当前 TQLEX 34/34、PBRPC 29/29 个唯一
   ReqId 已有专用业务映射；继续核对同号不同 flag/type/市场分支，而不是重复
   建立通用请求包装。
3. **P1：隐藏 JSN 高价值动态关系。** 在 618 个统一候选中，排除已固定的机构、
   资金、龙虎榜、公司行动、两融、互联互通、期货发行等资源族，只产品化仍有
   更新价值且主从键、单位和空值都能闭合的关系。
4. **P1：公式私有依赖边界。** 继续识别券商私有 `SIGNALS_QS`，但在缺少合法
   数据源时保持不可运行，不用常量或占位值制造选股结果。
5. **P2：L2 逐笔委托/十档。** 仍受完整登录、权限和订阅链约束，短期
   收益低于上述公开接口。
