# 原生 C++ 公式运行时与绘图 IR 模块拆分

日期：2026-08-10

## 拆分结果

`formula_engine.cpp` 从 2,813 行继续降至 946 行。解释器现在由四个明确组件组成：

- `formula_analysis.cpp`（1,034 行）：静态语义与能力审计；
- `formula_runtime.cpp`（479 行）：数值/字符串 AST 求值和 K 线规范化；
- `formula_render.cpp`（1,419 行）：样式、图元、文字数字格式和绘图事件 IR；
- `formula_engine.cpp`（946 行）：运行环境装配、逐语句调度、输出和审计结果封装。

新增 `formula_runtime_internal.hpp`，集中定义 `Bar`、`StringEnvironment` 以及求值
入口；新增 `formula_render_internal.hpp`，只向调度器暴露数字格式和单语句图元构建。
二者通过组合传递现有环境引用，没有全局可变状态，也没有引入不必要的继承体系。

## 行为保持

- AST 的数字、字符串、证券名称和上下文序列求值逻辑原样迁移；
- 普通/扩展市场 K 线、成交量单位、分时均价、结算价和空字段处理未变；
- 绘图指令顺序、COLORREF、线宽、蜡烛、线段、背景、图标、文字和数字事件未变；
- `render_ir` schema、源码语义分析、公开 C++ 接口、CLI 与 HTTP schema 未修改。

## 增量验证

- `tdx-formula-engine-tests` 通过；
- `tdx-tool` 增量链接通过；
- 对同一批公开 `sz:000001` 40 根日线重新执行
  `MA + COLORRED/LINETHICK2 + DRAWICON`；
- 拆分前后 40 根输出点逐值相等，完整 `render_ir` JSON 相等；均为 2 个图元、
  4 个图标事件；
- 拆分前证据：`output/native-formula-analysis-render-v30.json`；
- 拆分后证据：`output/native-formula-runtime-render-v31.json`；
- 未运行完整 CTest、API 合约或前端检查，因为公共 schema、传输、缓存与网页均未改动。

## 后续边界

原先近 5,000 行的解释器单文件已经成为不足 1,000 行的组合根。下一轮可以把其中
较长的“上下文 JSON → 运行环境”装配迁成 `FormulaEnvironmentBuilder`，或者回到
`formula_context.cpp` 继续拆证券关系与行业序列；两者已经没有文件级互相阻塞。
