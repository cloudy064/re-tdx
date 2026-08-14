# 原生 C++ 常用指标计算器策略模块化

日期：2026-08-11

## 问题

原 972 行的 `formula_calc.cpp` 同时包含 26 个指标算法、参数工具、输出顺序映射、
26 路条件分派、结果文档和 CLI。公式目录与执行分派分别维护，新增指标容易出现目录、
输出顺序和处理器不一致。

## 调整

| 文件 | 职责 | 行数 |
|---|---|---:|
| `formula_calc_internal.hpp` | 26 项类型化策略目录、数据对象和内部端口 | 137 |
| `formula_calc_support.cpp` | K 线读取、参数、MA/SUM/EMA/SMA 公共算法 | 161 |
| `formula_calc_trend.cpp` | MA 至 ATR 的基础趋势/摆动指标 | 264 |
| `formula_calc_volume.cpp` | VOL 至 EXPMA 的量价与组合指标 | 155 |
| `formula_calc_advanced.cpp` | DMI、WVAD、EMV、CHO、ADTM、DKX | 182 |
| `formula_calc_registry.cpp` | 目录输出、处理器调度和结果文档 | 121 |
| `formula_calc_command.cpp` | CLI 参数与实时 K 线获取 | 106 |

原根文件已移除。`FormulaDefinition` 统一保存小写查询名、公开代码、最多四个有序输出和
函数指针；目录同时驱动 catalog 与执行，不再存在 26 路 `if/else` 和另一份输出顺序
`map`。编译期静态断言会拒绝重复名称。

## 兼容性

- 26 个公式及参数语义不变。
- catalog schema v1、`tdx-native-compatible-v4` 和结果字段不变。
- 公开 `tdx/formula_calc.hpp` 未改变。

## 增量验证

1. 构建 `tdx-formula-calc-tests` 与 `tdx-tool`：通过。
2. 计算器专项测试覆盖全部 26 项目录和数值夹具：通过。
3. 平安银行 40 根日线 KDJ 实时样本：
   - 输出顺序：`K,D,J`
   - K≈41.2521
   - D≈57.1394
   - J≈9.4775

样本位于 `output/native-formula-calc-refactor-kdj.json`。未修改源码解释器、共享传输或
schema，因此没有运行完整 CTest。

