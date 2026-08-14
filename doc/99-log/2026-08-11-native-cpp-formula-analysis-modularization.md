# 原生 C++ 公式分析流水线模块化

日期：2026-08-11

## 目标

拆分原 1,034 行的 `formula_analysis.cpp`，使单公式语义分析、整库覆盖率汇总和显式
上下文模板不再共享一个实现文件，同时保持五个公开入口与 analysis schema v7 不变。

## 结构

| 文件 | 职责 | 行数 |
|---|---|---:|
| `formula_analysis_internal.hpp` | `SemanticAudit` 阶段数据对象和内部端口 | 32 |
| `formula_analysis_semantics.cpp` | 字符串/展示替代值的数值污染传播审计 | 98 |
| `formula_analysis_source.cpp` | 单公式解析结果、依赖、未来函数和可执行性判定 | 416 |
| `formula_analysis_library.cpp` | 公式定义、整库覆盖率、能力清单和 390 项注册边界证据 | 429 |
| `formula_analysis_context.cpp` | 显式上下文模板与 K 线时间戳抽取 | 119 |

原根文件已移除。四个阶段通过小型 `SemanticAudit` 对象协作；公开 API 仍由
`tdx/formula_engine.hpp` 声明，没有引入新的公共类型或修改调用方。

## 保持的契约

- `analyze_formula_source`
- `make_formula_source_definition`
- `analyze_formula_library_document`
- `make_formula_explicit_context_template_document`
- `formula_context_stamps_from_kline`
- `analysis_schema_version = 7`
- `formula_engine = tdx-source-interpreter-v1`

## 增量验证

1. 构建 `tdx-formula-engine-tests`、`tdx-formulas-tests` 和 `tdx-tool`：通过。
2. 两个专项测试可执行文件均通过。
3. 使用已有 379 条公式库重新执行 `formulas analyze`：
   - 379/379 语法支持；
   - 339 条可在上下文下执行；
   - 静态注册证据仍为 390 项；
   - 注册边界仍为完全分类；
   - analysis schema 仍为 v7。

样本位于 `output/formula-analysis-refactor-v7.json`。本轮只是实现单元边界调整，没有修改
词法/语法解析器、schema 或传输层，因此未运行完整 CTest。

