# 原生 C++ 业绩预告链路模块化

日期：2026-08-11

原 828 行 `forecasts.cpp` 同时承担资源常量、JSON 标量转换、行业/A 股/港股三类归一化、
全市场汇总、主从资源下载与缓存、四种视图筛选和 CLI。本轮移除旧根文件，拆分如下：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `forecasts_internal.hpp` | 内部端口、资源/视图枚举和类型定义 | 82 |
| `forecasts_catalog.cpp` | 3 项资源、4 项视图和 4 项情绪类别目录 | 52 |
| `forecasts_support.cpp` | JSON 标量、市场身份、文本、过滤和参数校验 | 260 |
| `forecasts_normalize_industry.cpp` | 行业统计与 13 项类别字段归一化 | 82 |
| `forecasts_normalize_security.cpp` | 沪深京逐股预告归一化 | 54 |
| `forecasts_normalize_hong_kong.cpp` | 港股预告、币种和正文/原因归一化 | 51 |
| `forecasts_summary.cpp` | 行业、港股、最新预告与报告期汇总 | 125 |
| `forecasts_service_fetch.cpp` | 板块映射、三资源抓取和主从缓存 | 107 |
| `forecasts_service_query.cpp` | 类型化视图分派、筛选和响应组合 | 209 |
| `forecasts_command.cpp` | CLI 参数与文件输出 | 69 |

查询不再用四段字符串 `if` 判断视图：`ViewDefinition + ViewKind` 目录完成名称解析，随后通过
枚举 `switch` 分派。资源路径和允许的情绪类别也集中到类型化目录；13 项行业类别字段改为
编译期字段表。公开头文件、命令参数、缓存语义和 `tdx-market-forecasts-native-v1` schema
保持不变。

增量验证只构建 `tdx-forecasts-tests` 与 `tdx-tool`，专项测试通过。真实 `latest` 样本从
3 项来源汇总 1,775 条上游预告、30 个行业，抽取 3 条且 `detail_errors=0`；样本位于
`output/forecasts-refactor-latest.json`。按精简验证规则未运行完整 CTest。
