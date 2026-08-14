# 原生 C++ 公式策略流水线模块化

日期：2026-08-11

## 问题

原 952 行的 `formula_strategy.cpp` 同时承担策略 manifest 校验、公式库解析、外部上下文装配、
逐证券求值、横截面扫描、等权组合回测、并发下载与 CLI。不同执行阶段共享大量匿名函数，
修改回测或下载流程时需要在一个文件中穿过整个策略生命周期。

## 调整

| 文件 | 职责 | 行数 |
|---|---|---:|
| `formula_strategy_internal.hpp` | JSON、证券、参数和上下文内部端口 | 56 |
| `formula_strategy_support.cpp` | 公式分析、参数、证券和输入文档公共支持 | 271 |
| `formula_strategy_context.cpp` | 规则级外部市场上下文公开适配 | 19 |
| `formula_strategy_normalize.cpp` | manifest 与公式规则归一化 | 96 |
| `formula_strategy_evaluate.cpp` | 同证券、同时间点的多规则组合求值 | 102 |
| `formula_strategy_scan.cpp` | 多证券近期信号扫描 | 72 |
| `formula_strategy_backtest.cpp` | 等权组合、再平衡、成本和归因 | 256 |
| `formula_strategy_command.cpp` | 三类股票池、并发下载、复权和 CLI 输出 | 205 |

原根文件已移除。拆分围绕策略生命周期的五个公开阶段和两个适配层展开；没有增加虚类，
数据对象仍由既有 `Json` schema 传递，CLI 与业务阶段不再处于同一翻译单元。

## 兼容性

- `tdx/formula_strategy.hpp` 的五个公开入口未改变。
- `all`、`any`、`at-least` 组合语义以及三类股票池来源未改变。
- 定义、求值、扫描和组合回测的既有 schema 未改变。

## 增量验证

1. 构建 `tdx-formula-strategy-tests` 与 `tdx-tool`：通过。
2. 策略专项测试覆盖定义、求值、扫描、复权元数据和组合回测：通过。
3. 平安银行 40 根日线 MA5 扫描：请求 1、求值 1、抓取错误 0、匹配 1，schema 为
   `tdx-formula-strategy-scan-v1`。

策略 manifest 与结果分别位于 `output/formula-strategy-refactor-manifest.json` 和
`output/formula-strategy-refactor-scan.json`。本轮未修改共享公式解释器、传输层或 schema，
因此依照增量验证规则未运行完整 CTest。
