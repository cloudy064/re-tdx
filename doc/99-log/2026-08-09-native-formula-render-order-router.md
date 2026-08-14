# 公式混合图元源码顺序与工作台深链接

## 结论

公式解释器与网页现使用同一套可验证的源码绘制顺序。IR 中每个图元同时携带：

- `statement_index`：在完整源码语句中的零起始位置，包含不产生图元的赋值语句；
- `render_order`：只对可绘制图元连续编号；
- `render_order_semantics=source-statement-order`。

顶层 IR 另发布 `primitive_order=source-statement-order`、
`primitive_order_contiguous=true` 和 `source_statement_count`。调用方因此不需要根据
函数类型猜测前后顺序。

Svelte 公式图表会先按 `render_order` 稳定排序 TradingView series。遇到同时包含
画布 series 与 HTML/SVG 图元的混合公式时，普通线、柱图、`LINESTICK`、
`PARTLINE`、`DRAWKLINE`、`DRAWBAND` 边界和连续数字会切换到有序覆盖层；
TradingView 仍负责时间轴、价格轴、缩放和平移坐标，图元组则与背景、柱体、线段、
文字、图标使用同一个 `render_order` 作为叠放层级。

这修正了三类真实错序：

- `BSQJ` 源码的“背景柱 → K 线 → 买卖图标”不再把早期柱体永久压到 K 线上；
- `WAVEKX` 的 K 线先于趋势线和条件色线绘制；
- `SQJZ` 的均线先于连续数字，数字不会被后端先创建的 series 覆盖。

`LINESTICK` 的同语句内部顺序仍固定为 `zero-baseline-stick` 后
`indicator-line`。

## 固定契约

新增 `formula-mixed-render-order-inline-post`。真实 POST 源码含 4 条语句：

```text
A:=MA(CLOSE,2);
STICKLINE(1,LOW,HIGH,2,0),COLORGRAY;
X:CLOSE,COLORRED;
DRAWICON(ISLASTBAR,HIGH,1),DRAWABOVE;
```

第一条赋值不生成图元，契约严格要求后续三项为：

```text
STICKLINE statement_index=1 render_order=0
SERIES    statement_index=2 render_order=1
DRAWICON  statement_index=3 render_order=2
```

缺少顺序字段的旧 IR 会被拒绝。

## 公式工作台深链接

真实无头 Edge 检查发现 `/protocol/formulas` 首先因 Vite `base='./'` 把脚本解析为
`/protocol/assets/*` 而黑屏；改成根相对 `/assets/*` 后页面能够挂载，但 hash
路由又把无 hash 的 pathname 改回 `/stock`。

现已同时修复两层边界：

- Vite 产物使用根相对 JS/CSS；
- 路由器在没有 hash 时识别 `stock/ranking/sectors/industry/data/protocol/system`
  七个合法 pathname 根，并原地迁移为 `/#/<原路径>`；
- 新增 `formula-workbench-route` 契约，要求嵌套路由返回 Svelte 挂载点及根相对
  JS/CSS，拒绝旧的 `./assets/*`。

最终 Edge 截图 `output/formula-order-workbench-final.png` 证明直接打开
`/protocol/formulas` 后实际呈现“公式库”，而不是黑屏或个股工作台。

## 验收与边界

- 原生 CTest：101/101；
- Svelte 检查：0 错误、0 警告；生产构建成功；
- 新顺序、深链接、原生图标与 `LINESTICK` 选定契约：4/4；
- 固定 API 完整复跑：195/195，实际请求 196 次；报告为
  `output/api-contracts-formula-order-final-full2.json`；
- EXE SHA-256：
  `9C4393675062F34841F48589FD4CFD02AB1CFD64FC73F7DDAB022878A988F7F2`；
- 网页包：`assets/index-C2kpYgsp.js`。
- 全量巡检后已重启释放一次性缓存；正式服务 PID `38232`，工作区作为启动目录，
  仅监听 `127.0.0.1:8765`。最终重启后 quick 8/8、顺序与 JSN 本地发现专项
  2/2，健康
检查为 379 条公式、100 个图集单元且无 Python 运行时。

本轮保证源码语句的逻辑叠放顺序，但字体度量、抗锯齿、Alpha 混合、TradingView
坐标取整与原生 GDI 仍不声明逐像素相等，`pixel_renderer_equivalent=false`
继续保留。
