# DRAWBAND 原生不透明填充语义

## 结论

`TdxW.exe!sub_959960` 是当前版本公式 `DRAWBAND` 的实际绘制器。它为两侧颜色
分别创建 GDI `CreateSolidBrush` 与 `CreatePen`，构造闭合路径后调用
`StrokeAndFillPath`；整条路径没有调用 `AlphaBlend`。因此原网页对带状层统一使用
`opacity:0.24` 与客户端不符，现已改为不透明填充。

C++ `tdx-formula-render-ir-v1` 的 `DRAWBAND` 图元新增：

- `band_fill_opacity=1`；
- `band_fill_compositing=opaque-gdi-stroke-and-fill-path`。

Svelte 不再给 `.band-layer` 设置统一透明度。`DRAWGBK_DIV` 当时保持独立；随后
已由类型 21 原生分支证明其官方不透明模式及 `10..20` Alpha 模式，见
[DRAWGBK_DIV 专项记录](2026-08-09-native-formula-drawgbk-alpha.md)。

## IDA 证据

调度函数 `TdxW.exe!sub_977560` 从 TCalc 结果记录读取绘图类型；类型值 `5.0`
进入 `sub_959960`。该函数读取两条数值序列及两个颜色值，并按相邻 bar 组成封闭
区域：

1. 为两个颜色分别创建实心画刷和画笔；
2. 按当前/下一根的上下关系选择颜色；
3. `BeginPath/MoveToEx/LineTo/CloseFigure/EndPath` 后执行
   `StrokeAndFillPath`；
4. 两条边界交叉时计算交点，将四边形拆成两个三角形，分别用两侧颜色填充；
5. 函数内无 `AlphaBlend`、`GradientFill` 或透明度参数。

只读证据保存在：

- `output/probes/ida-tdxw-chart-gdi-import-xrefs.json`；
- `output/probes/ida-tdxw-drawband-callers.json`。

这段证据只证明 `DRAWBAND`；`DRAWGBK_DIV` 的结论来自后续独立入口和辅助函数，
不会混用。文字抗锯齿、坐标取整或其他界面层的 Alpha 语义仍未外推。

## 实现与回归

- `native/src/formula_engine.cpp` 发布已证实的不透明 GDI 语义；
- `web/src/charts/FormulaChart.svelte` 移除带状层的 `0.24` 透明度；
- `formula-rgband-fill-live` 固定契约要求上述两个字段，并明确拒绝旧的半透明 IR；
- 公式引擎和契约求值器单元测试均覆盖新字段及旧结果失败边界。

验收结果：

- CTest 101/101；
- Svelte 检查 0 错误、0 警告，生产构建成功；
- `formula-rgband-fill-live` 1/1，quick 8/8；
- 默认 15 秒超时的首轮 full 为 193/196，三项均是 JSN/情报端点
  `WinHTTP 12002`，不是断言失败；干净重启并使用 60 秒单请求上限后 full
  196/196、实际请求 197 次，报告为
  `output/api-contracts-formula-drawband-full-rerun.json`；
- EXE SHA-256：
  `3DCC562493F8CAFE2E0AEF0F351815849A51D3BBA429C111B788288DBA0A0017`；
- 网页包：`assets/index-ALj4lmFh.js`、`assets/index-C4JNPmSg.css`。
