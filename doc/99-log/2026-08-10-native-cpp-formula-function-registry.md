# 原生 C++ 公式函数族与类型化处理器注册表拆分

日期：2026-08-10

## 拆分结果

`formula_functions.cpp` 从 3,909 行降到 3,353 行。原来直接嵌在
`evaluate_call()` 中的多个职责已迁出：

- `formula_function_support.cpp`：统一 `require_arity()` 和动态周期读取；
- `formula_functions_chip.cpp`：`COST/COSTEX/WINNER/PWINNER/LWINNER/PPART`；
- `formula_functions_context.cpp`：财务、行情、板块、证券状态、复权、交易信号、
  L2 显式绑定等宿主上下文函数；
- `formula_functions_presentation.cpp`：字符串句柄替代、颜色、折线及绘图副作用
  的数值运行时行为；
- `formula_function_dispatch_internal.hpp`：各领域分派器的最小内部接口。

三个领域模块都使用静态 `unordered_map<string_view, FunctionHandler>`，把函数名
映射到具有统一签名的处理器。`evaluate_call()` 只依次询问领域注册表，未命中时
才继续其他数值函数或报告不支持；新增函数不再需要继续扩大尾部条件链。

## 行为保持

- 筹码分布继续使用历史 `CAPITAL`、原有 float32 收窄、价格分箱和工作量上限；
- `FINANCE/FINVALUE/DYNAINFO` 及单点财务函数仍逐点构造原上下文键；
- `DIVFACTOR(0)` 仍继承 `TQFLAG`，无有效模式时返回 1；
- `TOTALMMPAMO(5/6)` 仍明确要求授权 L2，其余公开 L1 字段边界不变；
- 买卖信号仍只产生布尔数值序列，不执行真实交易；
- 字符串和绘图函数仍保留原有 arity、折线数值与无副作用降级语义。

## 增量验证

- `tdx-formula-engine-tests` 构建并通过；
- `tdx-tool` 增量链接通过；
- 公开 `sz:000001` 的 20 根日线公式执行成功，解释器为
  `tdx-source-interpreter-v1`，输出 1 组、20 个点；
- 证据：`output/native-formula-function-registry-v26.json`；
- 未运行完整 CTest 或 API 合约巡检。

## 后续边界

下一轮优先迁移 `formula_functions.cpp` 中连续的日历/交易时段函数族与滚动统计
函数族。随后可把 TDX 专用指标（`TDXKDJ/TDXMCST/TDXSAR` 等）作为策略集合
独立出去，使核心文件最终只保留基础数值运算和领域注册表组合。
