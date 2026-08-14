# TCalc 公式绘图 IR

## 目标

公式解释器过去能保证数值信号安全，但 `STICKLINE`、`DRAWTEXT_FIX`、
`DRAWICON` 等展示函数只返回数值占位。这样可以选股和回测，却不能把通达信
系统指标的柱、文字、图标和局部线段交给网页复原。本轮在纯 C++ 解释器内增加
独立的稀疏绘图中间表示，运行时不依赖 Python。

## 协议

`formulas evaluate` 与 `/api/v1/formulas/evaluate` 的结果新增：

```text
render_ir.schema = tdx-formula-render-ir-v1
render_ir.bar_reference = points[index]
render_ir.primitives[]
  kind / function / statement
  style.directives[]
  value_source                    普通指标线
  events[].index / arguments      稀疏绘图事件
  events[].string_arguments       动态文字参数
```

普通输出线只记录 `points.values.<输出名>` 引用，不复制逐根数值。事件型绘图只在
条件为真时生成一项，因此数据量与实际标记数量相关，而不是固定按 K 线数量乘以
所有图层。`DRAWKLINE` 和 `DRAWBAND` 仍按每根有效 bar 生成事件；背景绘制只保存
最后一根的当前状态。

当前映射如下：

| 公式函数 | IR kind |
|---|---|
| `STICKLINE` | `stick` |
| `DRAWICON` | `icon` |
| `DRAWKLINE` | `candlestick` |
| `DRAWTEXT/DRAWTEXT_FIX` | `text` |
| `DRAWNUMBER/DRAWNUMBER_DIF` | `number` |
| `DRAWBAND` | `band` |
| `DRAWGBK_DIV` | `background` |
| `PARTLINE` | `part-line` |
| 普通输出 | `line` |

解析器不再把所有绘图指令合并成全局集合。每条语句独立、按源码顺序保存
`COLOR*`、`LINETHICK*`、`NODRAW`、`DRAWABOVE` 和 `DOTLINE`，同时提供已解码
的可见性、线宽和颜色 token。`RGB` 精确返回 Windows `COLORREF` 数值；
`STRCAT`、`CON2STR` 和字符串 `IF` 支持逐 bar 物化及赋值链引用。

## 验收

单元测试覆盖：

- 十根条件为真的 `STICKLINE` 生成十个事件，首事件指向 `points[0]`；
- `COLORRED,LINETHICK2` 顺序和线宽解码保持不变；
- `Z1:=STRCAT('A',CON2STR(CLOSE,1))` 在最后一根生成文字 `A19.0`；
- `RGB(1,2,3)` 得到 `197121`；
- 扫描和回测的数值安全拒绝逻辑继续通过。

当前安装版 `TCalc.dll` 全库分析为：379 条公式、379 条数值安全、90 条含绘图、
90 条可生成 IR、56 条仍标记展示 surrogate、0 条数值输出受污染。保留 56 条
标记是因为结构化参数并不等同于通达信像素渲染器，不用“有 IR”冒充完全等价。

真实端到端样本使用平安银行日线和系统指标 `TJCJL`：120 根 K 线得到 9 个
primitive、126 个稀疏事件，类型为一条普通线、一条说明文字和七组柱线；说明
文字正确物化为“说明: 红色柱为吸货量,绿色为出货量,黄色为天量,蓝色为地量”。
样本保存在 `output/probes/formula-render-ir-TJCJL.json`，全库 v4 报告保存在
`output/probes/formula-semantic-audit-v4.json`。

## 边界

`pixel_renderer_equivalent` 固定为 `false`。IR 保留公式输入语义，但通达信字体、
坐标布局、图标资源、色板和覆盖顺序仍需前端 renderer 决定。IR 只进入只读
evaluate/API 返回；条件扫描与专家回测继续只消费 `points.values` 数值输出。
