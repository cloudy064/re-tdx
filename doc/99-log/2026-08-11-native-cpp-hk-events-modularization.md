# 原生 C++ 港股事件与历史沽空模块化

日期：2026-08-11

## 目标

拆解 749 行的 `hk_events.cpp`。原文件同时维护四项 JSN 资源映射、通用字段转换、四类事件
归一化、7727 历史沽空对账、缓存抓取、查询汇总和两个 CLI，资源名称、事件类型、视图与中文
标签分别散落在条件分支中。

## 落地结果

原根文件已移除，公开 `tdx/hk_events.hpp` API 与两个 JSON schema 保持不变：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `hk_events_catalog.cpp` | 64 | 四项资源、事件类型、标签和视图的类型化目录 |
| `hk_events_support.cpp` | 153 | JSON 字段、数值、证券身份、日期、路径和本地 JSN 支持 |
| `hk_events_normalize.cpp` | 111 | 分红、权益披露、沽空和上市申请归一化 |
| `hk_events_short_history.cpp` | 176 | 7727 日线沽空序列、滚动统计与 GGRL104 对账 |
| `hk_events_service_fetch.cpp` | 79 | 本地/远端资源抓取、缓存和证券名称补全 |
| `hk_events_service_query.cpp` | 143 | 事件筛选汇总与历史沽空查询编排 |
| `hk_events_command.cpp` | 119 | 两个 CLI 的参数、默认值和输出 |

`HkEventResourceDefinition` 将资源、事件类型、中文标签和查询视图绑定在同一条记录中，并在
编译期检查四类键是否重复。服务抓取顺序、归一化策略选择和视图过滤都从该目录派生。十项
命令默认值集中到 `HkEventCommandDefaults`，没有引入不必要的继承层。

## 增量验证

- `tdx-hk-events-tests` 与 `tdx-tool` 增量构建通过；
- 专项测试继续覆盖四类资源的单位换算、证券身份、日期语义以及历史沽空滚动统计和精确对账；
- 真实本地样本汇总 5,674 行：分红 1,355、权益披露 1,554、沽空 2,565、上市申请 200，
  四项来源齐全并返回 3 行；
- `realtime-corporate` 定向 API 契约通过；
- schema 保持 `tdx-market-hk-events-native-v1` 与
  `tdx-market-hk-short-history-native-v1`；
- 本轮未改变共享解析、传输或 API schema，未运行完整 CTest。

## 后续候选

当前最大生产实现为 744 行的 `native/src/industry_profile.cpp`。
