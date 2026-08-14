# 原生 C++ 公式横向聚合上下文模块化

日期：2026-08-11

原 820 行 `formula_context_aggregate.cpp` 包含两套相互独立但重复板块解析逻辑的解释器上下文
算法：opcode 1245 `HORCALC`，以及 opcode 1246/1247 `INSORT/INSUM`。本轮移除旧根文件，
拆分为：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `formula_context_aggregate_internal.hpp` | 绑定、universe、成员顺序策略和共享端口 | 93 |
| `formula_context_aggregate_support.cpp` | JSON 支持、板块/自定义板块解析和成员日线装载 | 177 |
| `formula_context_horcalc.cpp` | 原始行情横向求和、排名与五种权重 | 281 |
| `formula_context_indicator_aggregate.cpp` | 指标选择、跨证券求值缓存、INSORT 与 INSUM 六模式 | 339 |

两套算法现在共用 `AggregateUniverseCatalog` 和 `resolve_aggregate_universe`，但没有错误地统一
成员顺序：`AggregateMemberPolicy::horcalc` 保留内置板块证券键排序；
`AggregateMemberPolicy::indicator` 保留原始成员顺序，因为 `INSUM` 最大/最小成员序号依赖它。
`HY./GN./MY.` 前缀、活动行业模式、自定义板块、日线文件计数和 L2/云集合拒绝边界保持不变。

为了缩短日常验证，新增 14 行入口 `tdx-formula-context-aggregate-tests`，复用原
`formula_engine_context_builder_tests.cpp` 与测试支持，只运行 context-builder 域，不再携带语言、
渲染、工作流和公式库全部域。本轮该目标与 `tdx-tool` 增量构建通过，聚焦测试约 1 秒通过，
覆盖 `HORCALC` 求和/排名/加权及 `INSORT/INSUM` 排名、求和、平均、极值和成员序号语义。
按精简验证规则未运行完整 CTest 或五域解释器测试。
