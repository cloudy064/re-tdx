# 原生 C++ 可转债服务模块化

日期：2026-08-11

## 目标

拆解 698 行的 `convertible_bonds_service.cpp`。原文件同时维护四类主数据抓取、五类缓存、
L1 行情批量获取、四种视图过滤/排序/汇总、逐债条款明细和最终响应。

## 落地结果

原根文件已移除，公开查询入口与 `tdx-market-convertible-bonds-native-v1` schema 保持不变：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `convertible_bonds_service_fetch_master.cpp` | 125 | 已上市/待发主表、投影补充和缓存 |
| `convertible_bonds_service_fetch_pricing.cpp` | 109 | 申购、定价资料、L1 行情及缓存 |
| `convertible_bonds_service_query.cpp` | 75 | 参数校验、四视图目录和组合根 |
| `convertible_bonds_service_query_subscriptions.cpp` | 135 | 申购与新债投影查询 |
| `convertible_bonds_service_query_pricing.cpp` | 152 | 批量行情、估值过滤和定价响应 |
| `convertible_bonds_service_query_pending.cpp` | 102 | 待发进度、规模和投影响应 |
| `convertible_bonds_service_query_listed.cpp` | 140 | 已上市目录、动态条款和整体摘要 |

`listed/pending/subscriptions/pricing` 由编译期唯一的 `ViewDefinition` 目录解析为枚举，薄组合根
只负责输入边界和视图调度。公开类只增加四个私有处理方法；抓取缓存与业务查询不再处于同一
翻译单元。

## 增量验证

- `tdx-convertible-bonds-tests` 与 `tdx-tool` 增量构建通过；
- 可转债专项测试通过；
- 真实申购样本返回 323 条匹配、两项来源和 20 条新债投影，限制返回三条，availability
  为 `live`；
- `market-core` 定向契约域通过；
- 未改变共享解析、传输或 API schema，未运行完整 CTest。

## 后续候选

排除 L2 和共享 `jsn.cpp` 后，下一低风险候选为 694 行的
`native/src/strong_stocks.cpp`。
