# 原生 C++ 公式渲染契约验证器模块化

日期：2026-08-11

## 结果

`recon_contract_formula_render.cpp` 原有 1,268 行，全部集中在一个按 `contract_id` 分支的
验证函数中。拆分后根文件为 20 行的类型化策略分派器，19 个契约 ID 映射到 8 个契约族：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `recon_contract_formula_render.cpp` | 20 | 类型化契约查表和分派 |
| `recon_contract_formula_render_price.cpp` | 146 | 价格注解契约 |
| `recon_contract_formula_render_sequence.cpp` | 226 | `DRAWNUMBER_DIF` 序列注解契约 |
| `recon_contract_formula_render_series.cpp` | 285 | 柱状序列与原生线型契约 |
| `recon_contract_formula_render_presentation.cpp` | 101 | COLORREF 与源码绘制顺序契约 |
| `recon_contract_formula_render_primitives.cpp` | 515 | 12 个真实绘图原语契约 |
| `recon_contract_formula_render_background.cpp` | 100 | 背景连续区间契约 |

契约 ID、验证函数和契约族之间的关系由编译期 `FormulaRenderContractStrategy` 目录维护，
根验证器不再包含业务分支。

## 等价性与验证

- 原文件的 19 个契约 ID 与新注册表 19/19 一致，无遗漏、无意外新增；
- 8/8 个契约族函数体逐字核对一致；
- `tdx-recon-contract-tests` 通过，输出 `API contract evaluator tests passed`；
- `tdx-tool` 编译链接通过；
- 公式渲染契约和 API schema 均未改变。

机器可读证据位于 `output/formula-render-contract-modularization.json`。本轮只调整契约验证器
内部边界，依据增量验证规则未运行完整 CTest。当前目录不是 Git 工作树，未执行
`git diff --check`。
