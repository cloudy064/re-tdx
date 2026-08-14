# 原生 C++ TPool 流程运行时模块化

日期：2026-08-11

## 目标

拆解 692 行的 `tpool_flow.cpp`。原文件同时维护周期映射、JSON/时钟工具、首次调度判定、
无状态拓扑投影、有状态周期推进、事件历史和告警差异。

## 落地结果

原根文件已移除，公开 TPool API 与两个 flow schema 保持不变：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `tpool_flow_support.cpp` | 122 | 八周期目录、JSON/时钟/集合支持 |
| `tpool_flow_schedule.cpp` | 69 | 首次启动边界和运行时状态恢复 |
| `tpool_flow_projection.cpp` | 122 | 无状态拓扑投影和候选传播 |
| `tpool_flow_state.cpp` | 324 | 有状态一秒调度、周期执行、成员和事件历史 |
| `tpool_flow_diff.cpp` | 82 | 规则/错误告警集合差异 |

原 `switch(period)` 改为八项 `PeriodDefinition` 编译期目录，并检查编号和名称唯一性。
`FlowScheduleDecision` 与 `FlowRuntimeState` 成为模块内部模型，投影、调度推进和告警 diff
不再共享同一个实现文件。512 条事件历史上限和只读、不写回 TDX 状态的语义保持不变。

## 增量验证

- `tdx-tpool-tests` 与 `tdx-tool` 增量构建通过；
- 专项测试覆盖拓扑投影、首次推进、重复周期、one-shot、状态重置和告警 diff；
- 现有双单元 fixture 代表执行包含两 cells、一 flow 和一 function，引用有效、图无环，
  `tdx-tpool-read-only-flow-projection-v1` 投影成功；
- 未改变共享公式求值、传输或公开 schema，未运行完整 CTest。

## 后续候选

排除 L2 和共享 `jsn.cpp` 后，下一候选是解释器相关的 692 行
`native/src/cloud_calc_builtins.cpp`，应使用云计算专项测试验证。
