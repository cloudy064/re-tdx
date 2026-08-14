# 基金风险、能力、公开持仓与仓位分析工作台

日期：2026-08-11

## 缺口选择

解释器边界报告仍显示普通公开非 L2 候选为 0；JSN 变体审计中的 34 个 generic-only
模板均没有下载样本且为零字节。继续按名称猜测协议没有可靠证据。相较之下，原生服务已有
`fund-analytics` 十三类公开数据视图，但网页没有入口，因此本轮优先补齐这条可验证链路。

## 原生侧收敛

`fund_analytics.cpp` 原先分别维护视图定义和 `available_views` 字符串列表，现统一为 13 项
编译期类型目录，请求号、配置文件、分页属性、明细参数和标题从同一来源生成。模块自己的
瞬态错误字符串判断也已删除，改用共享的 `tdx::detail::is_transient_cloud_error`，因此
WinHTTP 发送/接收中断、HTTP 429/5xx、业务忙和 RPC `-1` 使用同一分类口径。

公共 API、CLI 参数和 `tdx-fund-analytics-native-v2` 响应结构未改变。

## 网页能力

新增 `/data/fund-analytics`：

- 收益风险与日收益历史；
- 月度风险、月收益历史和 CL/TM/HM 择时选股能力；
- 报告期基金概览、行业持仓和股票持仓；
- 持仓稳定性、区间行业持仓和逐报告期持仓；
- 单基金仓位估算与全市场基金仓位历史；
- 基金列表点击后自动进入对应明细；
- 日/月收益、持仓历史和市场仓位使用 TradingView Lightweight Charts 多序列图展示。

页面没有堆成一个大文件：181 行编排组件、251 行类型化视图/列/图表配置和 114 行共享
多序列图组件分别承担状态、常量与渲染职责。市场 API 的网页直接引用覆盖由 75/104 提升到
76/104；共享图组件随后也被估值研究页复用。

## 聚焦验证

- `tdx-cloud-resilience-tests`、`tdx-fund-analytics-tests` 通过；
- `tdx-tool` 编译链接通过；
- `svelte-check` 为 0 error / 0 warning；生产构建通过；
- `/data/fund-analytics` 返回 200 且包含 Svelte 挂载点；
- 真实风险主表样例返回 20 条，`000711` 持仓历史返回 9 条，两者均为
  `tdx-fund-analytics-native-v2`、`availability=live`，且报告 13 个可用视图。

证据位于 `output/web-fund-analytics-coverage.json`、
`output/fund-analytics-web-risk-sample.json` 和
`output/fund-analytics-web-history-sample.json`。遵循增量验证规则，未运行完整 CTest 或完整
API 契约套件。
