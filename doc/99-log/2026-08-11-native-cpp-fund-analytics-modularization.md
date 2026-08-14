# 原生 C++ 基金分析链路模块化

日期：2026-08-11

原 837 行 `fund_analytics.cpp` 同时承担 13 个视图、18 种基金风格、三项基准、日期回退、
13 类行归一化、TQLEX 重试/分页、缓存、结果组合和 CLI。现拆分如下：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `fund_analytics_internal.hpp` | 13 项视图策略、18 项风格、3 项基准与内部端口 | 142 |
| `fund_analytics_catalog.cpp` | 视图/风格查找、视图文档和基准投影 | 47 |
| `fund_analytics_support.cpp` | 日期、JSON 标量、基金身份、经理、缓存键与重试判定 | 288 |
| `fund_analytics_normalize_performance.cpp` | 风险、日/月历史、月度风险和择时选股能力 | 151 |
| `fund_analytics_normalize_holdings.cpp` | 报告期持仓、行业、证券与持仓稳定性 | 97 |
| `fund_analytics_normalize_position.cpp` | 单基金仓位估算与全市场仓位历史 | 35 |
| `fund_analytics_normalize.cpp` | 函数指针策略调度与四种排序策略 | 49 |
| `fund_analytics_service.cpp` | 参数默认、TQLEX、报告期/估值日回退、缓存及文档组合 | 311 |
| `fund_analytics_command.cpp` | CLI 参数与文件输出 | 83 |

旧根文件已移除。原 13 路 `if/else` 归一化链改为 `ViewDefinition` 中的行处理函数指针和排序
策略；固定风格与基准不再由运行时 map/并行数组维护。公开接口、单位边界、回退语义及
`tdx-fund-analytics-native-v2` schema 保持不变。

增量验证：`tdx-fund-analytics-tests` 与 `tdx-tool` 构建、测试通过。真实 `risk` 首页继续以
请求 `500030` 返回 20 条记录，抽取 3 条；13 个视图齐全，默认风格为普通股票型，样本位于
`output/fund-analytics-refactor-risk.json`。未运行完整 CTest。
