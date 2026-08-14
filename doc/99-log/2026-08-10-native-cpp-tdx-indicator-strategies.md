# 原生 C++ TDX 专用指标策略模块拆分

日期：2026-08-10

## 拆分结果

`formula_functions.cpp` 从 2,463 行继续降至 1,493 行。新增
`formula_functions_tdx_indicators.cpp`（1,035 行），集中负责：

- `TDXXLPLBASE/TDXZXNH`；
- `TDXSSRP/TDXPAV/TDXPAVE/TDXNDB/TDXSC` 及其筹码分布辅助算法；
- `TDXMCST/TDXMSI/TDXVTY/TDXSAR/TDXASI`；
- `TDXBB/TDXWIDTH/TDXBOLLM`；
- `TDXNVI/TDXPVI/TDXKDJ`。

18 个专用指标名通过静态
`unordered_map<string_view, IndicatorHandler>` 映射到统一策略签名。域内仍可复用
公共解释器递归调用，例如 `TDXBB/TDXWIDTH` 读取 `TDXBOLLM`，但核心
`evaluate_call()` 只保留一次领域分派。

通用 `MA/EMA/SAR/NEWSAR/FFTRANS` 没有迁入专用模块；这些属于基础公式运算，
继续与 TDX 私有指标状态隔离。

## 行为保持

- `TDXSSRP/TDXPAV` 继续使用历史/当前流通股本、原始成交量和 float32 累积；
- `TDXMCST` 的市场品种成交量倍率仍由上下文预先计算；
- `TDXZXNH` 的典型价/收盘价选择仍依赖市场与证券类别；
- 所有 selector 范围、暖机位置、递归平均、缺失值和异常消息保持原实现；
- 指标公开名称、内置公式正文与返回 schema 未变化。

## 增量验证

- `tdx-formula-engine-tests` 通过；其中直接覆盖
  `TDXSSRP/TDXPAV/TDXPAVE/TDXNDB/TDXSC` 的数值语义；
- `tdx-tool` 增量链接通过；
- 公开 `sz:000001` 40 根日线执行 `TDXKDJ(9,3,0/1/2)` 成功；最后一根
  K/D/J 分别为约 `41.2522/57.1403/9.4759`；
- 输入：`output/probes/function-tdx-indicator.tdx`；
- 证据：`output/native-formula-tdx-indicators-v28.json`；
- 未运行完整 CTest 或 API 合约巡检。

## 后续边界

解释器函数核心已经降到 1,500 行以内。下一轮应转向
`formula_context.cpp`：优先拆横向板块统计、跨证券指标聚合与公共市场汇总；随后
审计 `formula_engine.cpp` 的语义分析、执行和绘图 IR 是否还能进一步独立。
