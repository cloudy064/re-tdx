# DRAWGBK_DIV 原生连续区域与 Alpha 语义

## 结论

`TdxW.exe!sub_977560` 在绘图类型 `21.0` 时进入 `sub_95A6C0`，后者正是
`DRAWGBK_DIV(COND,COLOR1,COLOR2,MODE,RANGE)` 的原生入口。它只把
`abs(COND-1)<0.0001` 视为成立，并将最大连续成立区间一次性交给
`sub_95A330` 绘制。

原网页对全部背景统一设置 `opacity:0.24` 是错误的。精确模式为：

| MODE | 原生语义 | 填充 Alpha |
|---:|---|---:|
| 0 | 纵向双颜色渐变；同色时实心填充 | 255 |
| 1 | 横向双颜色渐变；同色时实心填充 | 255 |
| 2 | 仅用 `COLOR1` 画边框 | 无填充 |
| 3 | `COLOR2` 实心填充，`COLOR1` 画边框 | 255 |
| 10..20 | 只用 `COLOR1` 做 GDI+ ARGB 实心填充 | `255*(MODE-10)/10` |

系统“时空隧道”使用的私有模式 `17` 因此不是未知模式，而是 Alpha 字节
`178`，即 `178/255≈0.698`。第二个颜色在该模式中不参与填充。

## 原生区域和范围

`sub_95A6C0` 先找连续条件区间 `[start,end)`；`sub_95A330` 的水平范围从首根
中心减半个 bar 间距延伸到末根中心加半个间距。`RANGE` 语义为：

- `0`：整个指标窗格；
- `1`：连续区间内所有 bar 的最高价/最低价极值；
- `2`：连续区间首根 OPEN 到末根 CLOSE；
- 其他值：当前分支不做已知价格范围变换，继续标记为未知。

官方 `MODE=0/1` 由 `sub_7F3760` 调用 GDI GradientFill 路径；`MODE=2/3`
分别走画框/实心 GDI 填充；`MODE=10..20` 进入 `sub_695A80`，使用
`GdipCreateSolidFill` 和 `GdipFillPolygonI`，ARGB 高字节正是上表公式计算的
Alpha。

只读证据：

- `output/probes/ida-tdxw-render-candidates.json`；
- `output/probes/ida-tdxw-drawgbk-div-renderer.json`；
- `output/probes/ida-tdxw-background-fill-helpers.json`；
- `output/probes/ida-tcalc-drawgbk-band-xrefs.json`；
- `output/probes/ida-tcalc-drawgbk-band-pointers.json`。

新增通用只读辅助脚本 `doc/90-scripts/ida_string_pointer_scan.py`，用于发现 IDA
没有自动建立 data xref 的压缩/静态注册表字符串指针。

## 实现

C++ IR 现发布原生类型、入口、条件容差、连续区间、价格聚合、合成方式和精确
Alpha 字节。仍保留每根命中事件以兼容既有消费者，但以
`background_region_leader` 标记每个原生区域的唯一绘制事件，并附带起止索引。

Svelte 只绘制 leader：纵/横渐变覆盖整个连续区间，`RANGE=1/2` 使用区间聚合
价格；模式 `17` 使用 `COLOR1` 和 8 位 Alpha `178`。`.background-layer` 的统一
`opacity:0.24` 已删除。未知模式不再猜成纵向渐变。

`formula-background-inline-post` 现在同时要求不透明模式 0 和模式 17 的
`178/255` 语义，并拒绝旧的 `vendor-defined`/统一透明度结果。

## 验收

- CTest 101/101；
- Svelte 检查 0 错误、0 警告，生产构建成功；
- 背景与带状填充专项 2/2，quick 8/8；
- 完整 API 契约 196/196，实际请求 197 次，报告为
  `output/api-contracts-formula-background-full.json`；
- 正式 HTTP 样本确认类型 21、区域 `0..19`、模式 17、Alpha `178/255`；
- EXE SHA-256：
  `594DDFF0FF149A107ADA02703CEB022B9F8373F3896578FC93734B6F651845D1`；
- 网页包：`assets/index-CV0P01oI.js`、`assets/index-BnALMsTe.css`；
- 全量巡检后正式服务重启为 PID `28796`，仍只监听 `127.0.0.1:8765`。

字体度量、抗锯齿、坐标整数化和其他绘图函数仍不宣称逐像素等价。

