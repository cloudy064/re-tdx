# 原生 C++ 财务洞察链路模块化

日期：2026-08-11

## 目标

拆解 742 行的 `financial_insights.cpp`。原文件同时维护 15 项 JSN 资源常量、资源到视图和
中文标签的两组条件链、6 种排序映射、通用财务数值投影、15 类归一化、缓存查询和 CLI。

## 落地结果

原根文件已移除，公开 `tdx/financial_insights.hpp` API 与
`tdx-market-financial-insights-native-v1` schema 保持不变：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `financial_insights_catalog.cpp` | 104 | 15 项资源/视图/标签和 6 种排序类型目录 |
| `financial_insights_support.cpp` | 182 | 字段、单位、比率、证券身份、本地 JSN 和排序值支持 |
| `financial_insights_normalize.cpp` | 311 | 15 类客户端财务筛选归一化算法 |
| `financial_insights_service_fetch.cpp` | 55 | 本地/远端抓取、归一化汇总和缓存 |
| `financial_insights_service_query.cpp` | 99 | 视图、证券、关键字、排序和摘要查询 |
| `financial_insights_command.cpp` | 83 | CLI 参数、默认值和输出 |

`ResourceDefinition` 将资源、视图类型和中文标签绑定，`SortDefinition` 统一排序名称与
字段；两张目录均编译期检查重复键。服务抓取顺序、视图验证和 15 项 summary 全部从资源目录
派生。内部 API 隔离在 `tdx::detail::financial_insights`，七项命令默认值集中管理。

## 增量验证

- `tdx-financial-insights-tests` 与 `tdx-tool` 增量构建通过；
- 现有专项测试继续覆盖投资性房地产、分红不足、现金高于市值、股权投资、业绩预告、现金流
  质量、业绩反转、稳健成长、连续质量增长、利润突破和分红方案的单位与公式；
- 真实本地样本汇总 2,480 条、15 项视图和 15 个来源，返回信号最高的 3 条且 availability
  为 `live`；
- `calendar-resilience-web` 定向 API 契约通过；
- 本轮未改变公开 schema、共享解析或传输，未运行完整 CTest。

## 后续候选

排除已明确搁置的 L2 模块后，当前最大生产实现为 731 行的
`native/src/thematic_opportunities.cpp`。
