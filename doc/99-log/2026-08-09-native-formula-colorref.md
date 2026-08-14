# TCalc 公式颜色表与 COLORREF 语义

## 结论

公式网页不再用主题色近似 `COLORRED/COLORGREEN/...`，也不再把
`COLORrrggbb` 直接当 CSS `#rrggbb`。C++ 解释器现在按当前 `TCalc.dll` 的真实
解析路径发布：

- `style.color_ref_available`；
- `style.color_ref`；
- `style.color_source`；
- `style.color_encoding=Windows COLORREF: red | green<<8 | blue<<16`。

Svelte 优先消费该 COLORREF。旧响应兼容表仍保留，但数值也已与 DLL 表一致。

## IDA 证据

`sub_10047770` 对 `COLOR` 前缀后的名称按 34 字节记录查表；
`sub_10090590` 再从 `dword_1030A468[index]` 取颜色值。当前数据库中的关键位置为：

- 名称表：`aColorblack = 0x1012846A`；
- 数量：`word_10128464 = 16`；
- COLORREF 表：`dword_1030A468 = 0x1030A468`。

专用只读导出脚本为 `doc/90-scripts/ida_dump_tcalc_colors.py`，原始结构化结果为
`output/probes/ida-tcalc-colors.json`。导出的完整表如下：

| 指令 | COLORREF | 浏览器 RGB |
|---|---:|---|
| `COLORBLACK` | `0x00000000` | `#000000` |
| `COLORBLUE` | `0x00FF0000` | `#0000ff` |
| `COLORGREEN` | `0x0000FF00` | `#00ff00` |
| `COLORCYAN` | `0x00FFFF00` | `#00ffff` |
| `COLORRED` | `0x000000FF` | `#ff0000` |
| `COLORMAGENTA` | `0x00FF00FF` | `#ff00ff` |
| `COLORBROWN` | `0x00008080` | `#808000` |
| `COLORLIGRAY` | `0x00C0C0C0` | `#c0c0c0` |
| `COLORGRAY` | `0x00808080` | `#808080` |
| `COLORLIBLUE` | `0x00C0C000` | `#00c0c0` |
| `COLORLIGREEN` | `0x0040C040` | `#40c040` |
| `COLORLICYAN` | `0x00808000` | `#008080` |
| `COLORLIRED` | `0x008080FF` | `#ff8080` |
| `COLORLIMAGENTA` | `0x008000FF` | `#ff0080` |
| `COLORYELLOW` | `0x0000FFFF` | `#ffff00` |
| `COLORWHITE` | `0x00FFFFFF` | `#ffffff` |

同一反编译函数还固定了两类字面量：

- `COLOR00C0C0` 被替换成 `0X0000C0C0` 并原样保存，因此 COLORREF 为
  `49344`，显示 RGB 为 `#c0c000`；
- `RGBX1AAE52` 是人类顺序的 RRGGBB，TCalc 会交换红蓝后保存为
  COLORREF `0x0052AE1A`（`5418522`），显示 RGB 为 `#1aae52`。

## 实现与回归

- `native/src/formula_engine.cpp` 固化 16 项表及 `COLOR/RGBX` 解码；
- `web/src/charts/FormulaChart.svelte` 的所有线、柱、文字、图标关联层都从
  `FormulaRenderStyle.color_ref` 取色；
- 单元测试逐项核对 16 个命名色，并验证两个字面量；
- 新增 `formula-colorref-inline-post` 固定 HTTP 契约，只有 token、缺少数值语义的
  旧 IR 会被拒绝。

验收结果：

- CTest 101/101；
- Svelte 检查 0 错误、0 警告，生产构建成功；
- 颜色专项契约 1/1，quick 8/8；
- 完整 API 契约 196/196，实际请求 197 次，报告为
  `output/api-contracts-formula-color-full.json`；
- EXE SHA-256：
  `1881E906B9827807F10E549D2D777FD9B82AC9F8041DC97F533CDBBDB6DD0FF7`；
- 网页包：`assets/index-CmYdjvJm.js`；
- 全量巡检后正式服务已重启为 PID `27700`，以工作区为启动目录，仅监听
  `127.0.0.1:8765`，379 条公式、100 个图标单元、JSN 可用且无 Python 运行时。

## 边界

本轮证明的是公式颜色值及通道顺序，不证明字体度量、抗锯齿、Alpha 混合、
TradingView 坐标取整或 GDI 像素边界；`pixel_renderer_equivalent=false` 继续保留。
