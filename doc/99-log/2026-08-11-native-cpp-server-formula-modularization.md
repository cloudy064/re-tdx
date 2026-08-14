# 原生 C++ 公式 HTTP 处理层模块化

日期：2026-08-11

## 目标

拆分 `server_formula.cpp` 中混合的公式目录、审计、云计算、GET/POST 执行、回测和扫描策略
处理器，同时保持 `server_route.cpp` 的路由选择、POST 确认头和全部 HTTP 响应契约不变。

## 结构调整

原文件共 1,056 行，现已移除并拆为七个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `server_formula_inline.cpp` | 302 | 内联源码校验、显式上下文合并和 POST 执行 |
| `server_formula_scan.cpp` | 216 | 多证券扫描与组合策略扫描/回测 |
| `server_formula_query.cpp` | 200 | 公式目录、计算、覆盖率、上下文模板和审计 |
| `server_formula_cloud.cpp` | 124 | cloud-calc 单行/批量与内联 TPool |
| `server_formula_backtest.cpp` | 106 | 内置与内联专家公式回测 |
| `server_formula_execution.cpp` | 106 | 内置公式 GET 执行 |
| `server_formula_support.cpp` | 90 | 复权、并发 K 线抓取、参数和扩展市场能力判断 |

`server_formula_support_internal.hpp` 集中跨处理器共享的 K 线结果与内联请求类型。路由函数和
`server_formula_internal.hpp` 中既有 handler 端口没有改变，因此路由层不依赖实现文件布局。

## 兼容性

- `/api/v1/formulas`、coverage、context-template、audit、cloud-calc、evaluate、backtest、
  scan 与 strategy 路径保持不变；
- `X-TDX-Action` POST 确认逻辑仍由 `server.cpp` 执行，本轮未放宽任何写入边界；
- 16 KiB 公式源码、参数数量/数值、未来函数、扩展市场和显式上下文校验保持不变；
- 公式解释器、绘图环境和策略结果 schema 保持不变。

## 增量验证

- `tdx-tool`、`tdx-formulas-tests`、`tdx-formula-engine-tests` 和
  `tdx-formula-strategy-tests` 编译链接通过；
- `tdx-formulas-tests` 与 `tdx-formula-strategy-tests` 执行通过；
- 新二进制在临时 `127.0.0.1:8877` 上通过 3 个请求：MACD 目录查询、technical 覆盖率、
  深市 000001 的 MACD 日线解释执行；
- 解释执行返回 `tdx-source-interpreter-v1`、`native-cpp` 和 20 个点；
- 临时服务使用 `--max-requests 3` 自动退出，正式 8765 服务未改动；
- 没有执行完整 CTest。

## 后续候选

生产代码当前最大的单文件为 1,039 行的 `native/src/futures_issuance.cpp`。
