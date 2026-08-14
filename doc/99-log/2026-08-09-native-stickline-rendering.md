# STICKLINE 原生宽度、空心与中轴柱语义

## 结论

此前 C++ 已产生 `STICKLINE` 稀疏事件，但网页把它们全部转换成普通蜡烛，只在
`EMPTY=1` 时清空填充色。因此 `WIDTH`、虚线空心 `-1`、小数空心参数以及
`EMPTY=2/3` 中轴柱全部会误绘。本轮已拆掉该替代路径：解释器物化可审计的柱体
语义，网页按当前 bar 间距生成独立 SVG 图元，缩放、平移和容器尺寸改变都会重算。

实现仍是纯 C++ 服务加 Svelte 静态网页，不引入 Python 运行时。

## 原生证据与覆盖面

通达信[官方函数表](https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html)
定义 `STICKLINE(COND,PRICE1,PRICE2,WIDTH,EMPTY)`，其中 `WIDTH=4` 是标准间距；
`EMPTY=0/-1/1/2/3` 分别对应实心、虚线空心、实线空心、中轴满占和中轴半占，
后两种明确说明 `PRICE1` 无用。`TCalc.dll` 本地字符串也恢复出相同签名和示例：

- `output/probes/ida-tcalc-stickline-xrefs.json`
- `output/probes/ida-tcalc-stickline.log`

对 `output/probes/formula-analysis-20260809-next.json` 的全库审计得到 34 条公式、
84 个调用。实际参数覆盖 `WIDTH=-1/0.1/0.5/1/2/3/6/10`，并覆盖
`EMPTY=-1/0/0.1/1/1.5/2/3`；因此只判断 `EMPTY==1` 或固定蜡烛宽度都不成立。

## IR 与网页行为

保持 `tdx-formula-render-ir-v1` 向后兼容，图元新增标准宽度、参数位置和模式表；
每个事件新增：

- `stick_width/stick_width_ratio/stick_hairline`；
- `stick_mode/stick_hollow/stick_border_dashed`；
- `stick_price1_used/stick_anchor/stick_occupancy`。

模式映射为 `solid/dashed-hollow/solid-hollow/center-full/center-half`。除
`0/-1/2/3` 外的非零 EMPTY 归入普通实线空心，这覆盖系统 `ICHIMOKU` 的
`0.1/1.5`。中轴模式以指标 pane 中线为起点、以 `PRICE2` 为终点并忽略
`PRICE1`；半占模式再使用一半水平占用。正宽度按 `WIDTH/4 * 当前 bar 间距`
计算，非正宽度保留为一像素发丝线。两条透明价格锚点序列只负责 TradingView
自动缩放，实际柱体由 SVG 绘制。

这仍不宣称复刻原生 GDI 的像素取整、抗锯齿和图元覆盖顺序，
`pixel_renderer_equivalent=false` 保持不变。

## 真实公式验收

| 公式 | 真实数据事件 | 验证语义 |
|---|---:|---|
| `TJCJL` | 7 个图元、250 个事件 | `WIDTH=1`、`solid`、`price-pair` |
| `ICHIMOKU` | 175 个事件 | 小数 EMPTY 映射为 `solid-hollow` |
| `CCL` | 119 个事件 | `center-half`、`pane-middle`、`PRICE1` 不使用 |

固定契约新增 `formula-tjcjl-stickline-live` 和
`formula-ichimoku-stickline-live`；既有 `formula-cross-market-futures-live`
同时增加 `CCL` 中轴半占断言。契约测试还会拒绝只有原始参数、缺少物化语义的旧
蜡烛 surrogate。

## 验证与部署

- 原生 CTest：101/101；
- `svelte-check`：0 错误、0 警告；
- Svelte 生产构建成功，入口 `assets/index-gefURueL.js`；
- 追加边界临时/正式契约：6/6；
- 正式服务全量 API 契约：184/184，185 次网络请求。

结构化证据：

- `output/api-contracts-stickline-temp2-selected.json`
- `output/api-contracts-stickline-formal2-selected.json`
- `output/api-contracts-stickline-formal2-full.json`

正式服务安装在 `dist/tdx-tool`，PID `31120`，仅监听
`127.0.0.1:8765`。EXE SHA-256：
`E062684E96E4827BD67ED751A9AA0712BA26496CC150392929B83B426E28A6A3`。
