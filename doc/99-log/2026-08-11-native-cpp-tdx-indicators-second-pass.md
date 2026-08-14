# 原生 C++ TDX 专用指标二次模块化

日期：2026-08-11

## 问题

原 `formula_functions_tdx_indicators.cpp` 有 1,035 行。虽然它声明了 18 项
`unordered_map<string_view, IndicatorHandler>`，但所有名字都指向同一个
`evaluate_tdx_indicator_impl`，后者仍通过 18 组名称判断完成实际分派。因此原注册表
没有形成真正的策略边界，增加指标时仍需修改一个超过 500 行的条件函数。

## 调整结果

原单体已移除，拆为以下实现单元：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `formula_tdx_indicators_internal.hpp` | 18 项类型化策略目录、处理器端口和重名静态检查 | 66 |
| `formula_tdx_indicators_support.cpp` | 共享递归平均算法 | 29 |
| `formula_tdx_indicators_chip.cpp` | SSRP、PAV/PAVE、MCST 筹码分布族 | 326 |
| `formula_tdx_indicators_signal.cpp` | XLPLBASE、ZXNH、NDB、SC 专用信号族 | 279 |
| `formula_tdx_indicators_trend.cpp` | MSI、VTY、SAR、ASI 趋势族 | 271 |
| `formula_tdx_indicators_band_volume.cpp` | BB/WIDTH/BOLLM、NVI/PVI、KDJ 通道量价族 | 164 |
| `formula_tdx_indicators_registry.cpp` | 15 行只读分派入口 | 15 |

`TdxIndicatorDefinition` 同时保存名称、领域和函数指针。18 个公式名现在直接绑定四个
真实领域处理器，统一分派器和运行时哈希表已删除。目录增加编译期名称唯一性断言，重复
注册会在构建阶段失败。

## 行为边界

- 18 个公开函数名和选择器范围不变。
- float32 累积、暖机区间、缺失值和异常文本保持原实现。
- `TDXBB/TDXWIDTH` 对 `TDXBOLLM`、`TDXZXNH` 对 `TDXSAR` 的解释器递归调用保持不变。
- 解释器公开入口 `evaluate_tdx_indicator_function` 的签名和未匹配时返回
  `nullopt` 的语义不变。

## 增量验证

1. 构建 `tdx-formula-engine-tests` 与 `tdx-tool`：通过。
2. 运行 `tdx-formula-engine-tests`：通过。
3. 平安银行 `sz:000001` 40 根日线执行 `TDXKDJ(9,3,0/1/2)`：通过。
   最后一根 K/D/J 分别为约 `41.2522 / 57.1403 / 9.4759`，与拆分前证据一致。

代表性结果位于 `output/native-formula-tdx-indicators-refactor.json`。本轮没有修改解析器、
公共 schema 或市场传输，因此按增量验证规则未运行完整 CTest。

