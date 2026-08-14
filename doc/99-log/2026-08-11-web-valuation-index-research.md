# 相对估值、个股/行业估值、指数波动率与全收益差工作台

日期：2026-08-11

## 结果

新增 `/data/valuation-research`，把四条已有原生公开非 L2 能力合并为一个估值研究域：

- 指数相对基准的 PE-TTM、PB-MRQ、PS-TTM 比值、历史分位和逐日历史；
- 一级行业 PE、个股 PE 历史、行业成分一致预期估值、行业/成分 PB-ROE；
- 5/10/20/30/60/120/250 日指数已实现波动率、历史均值与分位；
- 价格指数与全收益指数的区间回报差，即股息再投资贡献。

相对估值和指数波动率目录支持点击指数下钻历史；行业 PE/PB-ROE 目录支持点击行业下钻
成分股；个股估值结果可继续进入个股工作台。相对估值、个股 PE 分位带和波动率历史使用
可缩放的 TradingView Lightweight Charts 多序列图。

## 结构治理

页面没有复制四套表格和图表：

- `ValuationResearchView.svelte` 322 行，只负责状态、请求和下钻编排；
- `valuationResearch.ts` 223 行保存十种表格/图表与枚举配置；
- `records.ts` 76 行统一嵌套字段访问、数值格式、标签映射和 `DataTable` 列生成；
- `ResearchSeriesChart.svelte` 114 行统一多序列图，基金分析页也已迁移复用。

四个原生模块原先各有不完整的瞬态错误字符串表，现全部复用
`tdx::detail::is_transient_cloud_error`。网页直接引用的市场 API 由 76/104 提升为
80/104。

## 聚焦验证

- 云端韧性、相对估值、个股/行业估值、指数波动率、全收益差五项专项测试通过；
- `tdx-tool` 编译链接通过；
- Svelte 检查为 0 error / 0 warning，生产构建通过；
- 新路由返回 200 且包含 Svelte 挂载点；
- 四个真实主表分别返回 180、30、100、68 条，全部为 `availability=live`。

机器证据位于 `output/web-valuation-research-coverage.json`，四个真实响应为
`output/valuation-research-*-sample.json`。依据增量验证规则，没有运行完整 CTest 或完整 API
契约套件。
