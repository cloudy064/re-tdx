# 原生 C++ 公式内建函数策略调度模块化

日期：2026-08-10

## 结果

生产文件 `formula_functions.cpp` 从 1,493 行降至 41 行。原来 861 行的顺序分支调度器
改为固定顺序的 5 项类型化策略表，算法按函数族拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `formula_functions.cpp` | 41 | 五策略顺序注册表与最终 unsupported 错误 |
| `formula_functions_support.cpp` | 196 | 数值/字符串兼容、市场证券分类和 TCalc 精度辅助 |
| `formula_functions_transforms.cpp` | 301 | SAR、NEWSAR 和 FFTRANS 原生变换 |
| `formula_functions_windows.cpp` | 178 | 滚动窗口与指数平滑 |
| `formula_functions_host.cpp` | 223 | RAND、外部信号/序列、复权、宿主时钟、LFS、IVOLAT 与筹码入口 |
| `formula_functions_scalar.cpp` | 225 | 条件、极值、数学函数、TMA 和 AMA |
| `formula_functions_reference.cpp` | 170 | XMA、REF、未来引用和 BARS 时序函数 |
| `formula_functions_future.cpp` | 288 | 日历、包含关系、ZIG/ZIGA、峰谷和 REFDATE |
| `formula_functions_core.cpp` | 86 | MA/SUM/HHV/LLV、MULAR、EMA/SMA、SAR 及既有函数族委派 |

`FormulaFunctionStrategy` 定义于 `formula_function_dispatch_internal.hpp`。调度顺序固定为
宿主上下文、标量数学、引用时序、未来形态、核心序列；每个策略未命中时返回
`std::nullopt`，最终调度根统一产生 unsupported 错误。由此新增函数可以进入所属策略，
不再继续扩张一个巨型 `if` 链。

公开公式 API、支持函数集合、未来函数限制、数值精度和错误文本均未改变。

## 等价性与验证

拆分期间保留工作区内临时源快照，验证完成后已删除。共享辅助、原生变换、滚动窗口和
五个策略函数体共 8 个关键区段逐字符一致；策略表顺序和数量检查为 5/5。`SarSeries`
只迁移到内部调度头，没有改变布局或算法。

- `tdx-formula-engine-tests`、`tdx-formula-strategy-tests`、
  `tdx-formula-calc-tests`、`tdx-formulas-tests` 全部通过；
- `tdx-tool` 增量编译、链接通过；
- 代表性真实解释执行使用华海药业 60 根日线和 `ZIGA` 源码，返回
  `tdx-source-interpreter-v1`、`RAW/ABSZIG/SELECTOR` 三个输出及 60 个点；证据保存在
  `output/formula-functions-modularization.json`；
- 当前目录不是 Git 工作树，因此未执行 `git diff --check`。

未运行完整 CTest 或全量 API 契约；本轮修改共享公式函数调度，因此运行了四个直接相关
公式目标和一个真实解释样例，没有重复运行其他无关市场测试。
