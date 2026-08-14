# 原生 C++ 公式渲染流水线模块化

日期：2026-08-11

## 结果

`formula_render.cpp` 原有 1,419 行，其中单个 `render_primitive_document()` 同时负责基础模型、
序列样式、原语元数据、事件构建和收尾。拆分后根函数缩减为 77 行，只保留固定顺序的渲染
流水线：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `formula_render_internal.hpp` | 124 | 类型化渲染目录、`PrimitiveRenderContext` 与内部端口 |
| `formula_render_common.cpp` | 289 | 样式、数字格式化和共享原生语义辅助函数 |
| `formula_render_series.cpp` | 137 | 普通曲线及 `VOLSTICK`/`COLORSTICK` 序列渲染 |
| `formula_render_metadata.cpp` | 413 | 绘图原语元数据与收尾元数据 |
| `formula_render_events.cpp` | 589 | 稀疏绘图事件生成 |
| `formula_render.cpp` | 77 | 基础模型、参数求值和五阶段编排 |

17 种特殊绘图函数到 Render IR `kind` 的映射已从连续条件判断迁入编译期类型化目录，普通
指标线继续使用 `line` 默认值。渲染过程是无状态策略组合，因此使用函数端口和只读上下文，
没有引入不必要的继承或运行时对象分配。

## 等价性与验证

- 9/9 个原实现区段逐字核对一致；
- 17/17 个特殊渲染类型映射核对完整；
- `tdx-formula-engine-tests` 专项单测通过；
- `tdx-tool` 编译链接通过；
- 使用真实通达信目录运行平安银行 `CPBS`，生成 2 个绘图原语；首个仍为
  `DRAWTEXT/text`，schema 仍为 `tdx-formula-render-ir-v1`；
- 结构证据保存在 `output/formula-render-modularization.json`，样例结果保存在
  `output/formula-render-modularization-sample.json`。

本轮没有改变解析器、传输或 Render IR schema，依照增量验证规则没有运行完整 CTest 或
契约套件。当前目录不是 Git 工作树，未执行 `git diff --check`。拆分后最大的生产实现文件
是 1,297 行的 `technical_signals.cpp`。
