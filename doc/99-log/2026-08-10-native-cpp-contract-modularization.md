# 原生 C++ 公式契约验证器模块化

日期：2026-08-10

## 问题

`recon_contract_formula.cpp` 原先把基础 API、云计算、绘图语义、公式运行时和
扫描/回测工作流的全部验证放在一个约 4,400 行的 `if/else if` 函数中。新增
契约必须继续修改同一翻译单元，职责、编译边界和代码审查范围都不清晰。

## 调整

- `recon_contract_formula.cpp` 只保留 27 行调度逻辑；
- 新增类型化 `FormulaContractValidator` 策略表，按固定顺序调用验证器；
- 契约按 `foundation`、`calculation`、`render`、`runtime`、`workflow` 五个领域
  拆分，最长实现文件约 1,975 行；
- 内部函数声明集中到 `recon_contract_formula_internal.hpp`；
- CMake 使用 `TDX_RECON_SOURCES` 归组侦察与契约源码，不再把相关文件散落在
  总源码清单中。

该重构不修改契约 ID、HTTP 路径、断言名称或 JSON schema。未知契约仍按原行为
返回 `false`，不会被任一领域验证器误接收。

## 后续拆分边界

继续演进时优先保持以下边界：

1. `server.cpp`：传输循环、路由目录、业务控制器分离；固定路由使用类型化目录；
2. `formula_engine.cpp`：词法语法、静态分析、执行编排和 CLI 分离；
3. `formula_context.cpp`：证券元数据、板块、财务时点、横向统计和市场汇总分离；
4. 大型请求常量进入独立 catalog，不嵌入控制流。

生产 `.cpp` 继续执行 5,000 行软上限；接近上限的文件不得再加入新职责。

## 增量验证

- 只构建受影响的 `tdx-recon-contract-tests`；
- `API contract evaluator tests passed`；
- 未运行完整 CTest、完整 API 巡检或真实网络流程，因为本批没有修改解析器、
  传输、缓存、公开 schema 或发布边界。

