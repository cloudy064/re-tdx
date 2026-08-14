# TCalc 公式解释器语义真实性审计

## 结论

此前的 `379/379` 表示“每条公式都有可解析正文或已验证的原生恢复体”，并不
自动等于绘图、字符串和宿主私有数据都被完整复刻。本轮把覆盖口径拆成数值
语义与展示语义，并对占位返回值做变量级传播审计：

- 379 条公式全部 `numeric_signal_safe=true`；
- 0 条公式存在占位值流入数值输出；
- 90 条公式包含绘图/样式语义，其中 289 条不依赖展示运行时；
- 56 条公式使用数值解释器的展示或字符串占位；这些占位只位于顶层绘图调用、
  文字参数或 `PARTLINE` 的样式参数，不参与选股数值；
- 扫描和回测会在执行前拒绝任何 `degraded_numeric_outputs`，错误报告同时列出
  `degraded_numeric_output_causes`，不再把“能算出一个数字”当作语义正确。

分析 schema 升级为 v3。每条公式新增：

- `numeric_signal_safe`；
- `semantic_fidelity`；
- `semantic_surrogates`；
- `presentation_functions` / `presentation_only_outputs`；
- `presentation_semantics_faithful` / `string_semantics_faithful`；
- `degraded_numeric_outputs` / `degraded_numeric_output_causes`。

## 字符串与绘图边界

全库只有 `CYX`、`SKSD` 和“上榜标注”三条公式使用 `STRCAT/CON2STR`，结果均
只进入 `DRAWTEXT_FIX`。解释器继续保留行业、地域和概念的实际 UTF-8 文本作为
上下文元数据，数值路径使用不可观察的字符串句柄占位。

本轮补齐了通用 `CODE/STRCMP`：解析器不再丢弃字符串字面量，执行时从 K 线
元数据绑定精确证券代码，并按 TDX 公式用法在相等时返回 1。没有用字符串哈希
或把代码转数字，因此前导零不会丢失。`DRAWGBK_DIV` 被确认并实现为纯展示调用；
`PARTLINE` 返回第一参数的真实纵坐标，颜色和线型单独标为展示降级。

## 未支持函数的真实剩余集合

清理 `CODE/STRCMP/DRAWGBK_DIV` 后，20 条不可运行公式只剩两组：

- 14 条 L2 大单/逐笔公式：`L2_AMO`、`LARGEINTRDVOL`、
  `LARGEOUTTRDVOL`、`TRADENUM`、`TRADEINNUM`、`TRADEOUTNUM`、
  `LARGETRDINNUM`、`LARGETRDOUTNUM`；
- 6 条券商私有信号公式：`SIGNALS_QS`。

IDA 静态证据显示 `sub_1006FBD0` 专门识别 `SIGNALS_QS(`，公式装载路径
`sub_10093070` 随后给记录设置 `0x200000` 能力标志；公式编辑/调用路径
`CMainCalcInterface::PopupDlg` 和 `sub_100A0940` 会组合检查这一标志并拦截。
因此它不是普通 OHLCV 派生函数，也没有被填零或用相似指标代替。

## 真实运行验收

使用平安银行 `sz:000001` 的 120 根真实日线，开启完整行情上下文和未来函数
只读模式：

| 项目 | 数量 |
| --- | ---: |
| 公式总数 | 379 |
| 当前品种与周期可适用 | 354 |
| 执行通过 | 354 |
| 执行错误 | 0 |
| 有任意数值输出 | 354 |
| 有最新数值输出 | 354 |
| 市场/品种不适用 | 4 |
| 周期不适用 | 1 |
| 未来函数只读公式 | 20 |

期权 `VOLATILITY` 在没有期权合约上下文时现在归类为市场不适用；金融期货
`JSJG` 在日线审计中归类为周期不适用并标明需要 `1m`，二者不再污染解释器
错误或空输出统计。

## 产物

- `output/probes/formula-semantic-audit-v3.json`；
- `output/probes/formula-semantic-runtime-audit-v3.json`；
- `output/probes/ida-tcalc-semantic-string-xrefs.json`；
- `output/probes/ida-tcalc-signals-qs-xrefs.json`；
- `output/probes/ida-tcalc-signals-qs-flag.json`。

