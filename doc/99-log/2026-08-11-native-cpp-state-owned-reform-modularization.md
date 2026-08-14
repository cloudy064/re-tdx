# 原生 C++ 国企改革链路模块化

日期：2026-08-11

原 823 行 `state_owned_reform.cpp` 同时承担四维资源目录、JSON 标量、证券与行情关联、三类
归一化、两套排序、五种视图、主从资源及 L1 缓存、响应组合和 CLI。本轮移除旧根文件，拆分为：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `state_owned_reform_internal.hpp` | 内部端口、视图/排序枚举和查询输出状态 | 98 |
| `state_owned_reform_catalog.cpp` | 4 个维度、5 种视图和两类排序目录 | 94 |
| `state_owned_reform_support.cpp` | 标量、市场身份、证券/行情关联、过滤和分页 | 221 |
| `state_owned_reform_sort.cpp` | 分组与重组的类型化排序策略 | 53 |
| `state_owned_reform_normalize_groups.cpp` | 分组及成员关系归一化 | 60 |
| `state_owned_reform_normalize_records.cpp` | 控制人明细和重组预期归一化 | 89 |
| `state_owned_reform_service_fetch.cpp` | 五项主资源、明细资源和 L1 行情缓存 | 122 |
| `state_owned_reform_service_query.cpp` | 五种视图枚举分派与业务编排 | 276 |
| `state_owned_reform_response.cpp` | availability、计数、筛选、缓存和语义文档 | 58 |
| `state_owned_reform_command.cpp` | CLI 参数和文件输出 | 79 |

固定字符串集合不再参与运行时分派：`ViewDefinition + ViewKind` 负责五种视图；分组的
`count/name/id` 与重组的 `date/profit/control/code` 分别映射到强类型排序枚举。公开头文件、
缓存语义、行情收益计算和 `tdx-market-state-owned-reform-native-v1` schema 保持不变。

增量验证只构建 `tdx-state-owned-reform-tests` 与 `tdx-tool`，专项测试通过。真实“整合预期”
样本仍汇总 111 个分组、1,968 条关系、899 只证券和 49 条重组记录；5 个来源全部可用，
计数不一致为 0，样本位于 `output/state-owned-reform-refactor-groups.json`。未运行完整 CTest。
