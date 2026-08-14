# 原生 C++ 公式扫描与持续监控模块化

日期：2026-08-11

## 目标

拆解 778 行的 `formula_scan.cpp`，分离解释器扫描核心、证券/K 线获取、结果 diff、watch 状态、
单次扫描编排和持续监控，同时保持公式解释器与公开 JSON schema。

## 落地结果

原根文件已移除，形成九个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `formula_scan_support.cpp` | 84 | 公式查找、参数和依赖分析支持 |
| `formula_scan_universe.cpp` | 106 | 证券解析、K 线输入和并发抓取 |
| `formula_scan_engine.cpp` | 64 | 公式求值与命中投影 |
| `formula_scan_diff.cpp` | 71 | enter/exit/update 集合差异 |
| `formula_scan_watch_state.cpp` | 54 | 持久状态与 `.blk` 输出 |
| `formula_scan_command_support.cpp` | 94 | 帮助、配置摘要和路径冲突防护 |
| `formula_scan_execute.cpp` | 141 | 单次扫描参数、上下文和调整编排 |
| `formula_scan_command.cpp` | 19 | 单次扫描命令 |
| `formula_watch_command.cpp` | 153 | 持续监控、降级保护和事件输出 |

公开的 `scan_formula_documents`、diff、watch state 和 block 输出签名保持不变。解释器仍使用
`tdx-source-interpreter-v1`；watch state 仍为 `tdx-formula-watch-state-v1`。

公式引擎测试 runner 新增 `--domain <name>`。不带参数时仍执行全部五个域，CTest 行为不变；
扫描改动可直接运行 `--domain workflow`。

## 增量验证

- `tdx-formula-engine-tests` 与 `tdx-tool` 增量构建通过；
- `workflow` 测试域通过，覆盖扫描、diff、watch state、block 输出、调整与财务上下文；
- 本地“BIAS卖出”样本输入 1、求值 1、公式错误 0、抓取错误 0；
- `formula-render-workflow` 定向 API 契约通过；
- 解释器 engine、扫描 schema v1 和调整模式保持不变，未运行完整 CTest。

## 后续候选

当前最大生产实现为 765 行的 `native/src/disclosures.cpp`。
