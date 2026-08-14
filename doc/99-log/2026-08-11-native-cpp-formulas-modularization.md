# 原生 C++ 公式资源提取模块化

日期：2026-08-11

## 目标

把 812 行的 `native/src/formulas.cpp` 从公式目录、PE 资源读取、图标转换、记录提取和 CLI
混合实现，拆成边界清晰的实现单元，同时保持现有命令、公式正文、资源格式和 JSON schema。

## 落地结果

原根文件已移除，替换为 5 个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `formulas_catalog.cpp` | 116 | 5 类公式目录、18 项恢复公式源码、支持版本 |
| `formulas_pe.cpp` | 221 | PE 映像、资源目录、DLL 校验 |
| `formulas_icon.cpp` | 143 | BMP/PNG 图标转换与精灵图清单 |
| `formulas_extract.cpp` | 260 | 5,072 字节公式记录解析、合并与 JSON 投影 |
| `formulas_command.cpp` | 178 | CSV 输出与两个 CLI 命令 |

内部端口集中在 `formulas_internal.hpp`。固定映射不再散落在条件链和并行 map 中：5 种公式
类型使用 `FormulaKindDefinition` 数组，18 项恢复公式使用 `RecoveredFormula` 数组。未引入需要
运行时所有权管理的继承体系。

公开公式 schema 继续为 v4，图标 schema 继续为 `tdx-formula-icon-sprite-v1`；361 项嵌入公式、
18 项恢复公式和 0 项缺失公式的合并语义保持不变。

## 增量验证

- `tdx-formulas-tests` 与 `tdx-tool` 增量构建通过；
- 公式提取专项测试通过，覆盖 379 项总数、四类计数、恢复正文和图标资源；
- 本地 `TCalc.dll` 代表样本提取 379 项：technical 222、selection 107、expert 15、color-K 35，
  embedded 361、recovered 18、missing 0；
- 静态核对确认旧根文件不存在，CMake 精确包含五个新实现单元；
- 本轮未改共享解析、传输或公开 schema，未运行完整 CTest。

## 后续候选

当前最大生产实现为 810 行的 `native/src/recon_contract_market_corporate_catalog.cpp`。
