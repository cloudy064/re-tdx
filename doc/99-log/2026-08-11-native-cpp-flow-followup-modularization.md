# 原生 C++ 资金流后续表现模块化

日期：2026-08-11

## 问题

原 970 行的 `flow_followup.cpp` 同时承担日期和 JSON 支持、六类公开视图、历史数据归一化、
四组预测模型、PBRPC/TQLEX 来源审计、缓存服务与 CLI。视图别名和请求参数散落在条件控制流
中，修改一种模型时容易影响不相关的数据源和输出阶段。

## 调整

| 文件 | 职责 | 行数 |
|---|---|---:|
| `flow_followup_internal.hpp` | 15 项视图别名、4 组模型定义、共享内部端口 | 98 |
| `flow_followup_support.cpp` | 日期、JSON、视图解析与缓存键支持 | 182 |
| `flow_followup_history.cpp` | 历史资金流归一化与占位区间审计 | 132 |
| `flow_followup_model.cpp` | 模型分档、当前信号和后续收益汇总 | 170 |
| `flow_followup_source.cpp` | PBRPC/TQLEX 来源与结果集元数据 | 100 |
| `flow_followup_service.cpp` | 双来源查询、缓存、降级和结果编排 | 380 |
| `flow_followup_command.cpp` | CLI 参数和输出 | 63 |

原根文件已移除。15 个别名统一映射到 6 个公开视图，4 组模型定义集中保存请求类型、分档与
汇总请求 ID、资源文件、模块和信号语义。数据驱动目录替代重复条件分支，但没有引入无状态
继承层级。

## 兼容性

- `tdx/flow_followup.hpp` 的公开查询、服务与归一化接口未改变。
- 六个公开视图、缓存键、降级行为和 CLI 参数未改变。
- 输出 schema 保持 `tdx-flow-followup-native-v2`。

## 增量验证

1. 构建 `tdx-flow-followup-tests` 与 `tdx-tool`：通过。
2. 资金流专项测试：通过。
3. `financing-model` 真实样本返回 3 行；分档和汇总两路 TQLEX 来源元数据均一致，输出
   schema 为 `tdx-flow-followup-native-v2`。

样本位于 `output/flow-followup-refactor-financing-model.json`。本轮没有修改共享解析器、
传输层或 schema，因此依照增量验证规则未运行完整 CTest。
