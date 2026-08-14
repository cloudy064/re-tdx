# 2026-08-09 公式文字与数字标注原生语义

## 目标

继续收敛 `tdx-formula-render-ir-v1` 中仍被 marker 近似的绘图函数，把价格文字、
固定文字、数字、换行、对齐和边框恢复为可消费的结构化语义，并保持 C++ 独立
运行。浏览器只负责渲染后端给出的 IR，不解释或转发 Python。

## 证据

- 通达信公式系统帮助给出的签名为
  `DRAWTEXT(COND,PRICE,TEXT)`、`DRAWTEXT_FIX(COND,X,Y,TYPE,TEXT)`、
  `DRAWNUMBER(COND,PRICE,NUMBER)` 和
  `DRAWNUMBER_FIX(COND,X,Y,TYPE,NUMBER)`；文字中的 `&` 表示换行，文字长度上限
  为 250，固定文字 `TYPE=0/1` 分别左/右对齐。
- 官方函数页：<https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html>。
- `TCalc.dll` IDA 字符串和交叉引用结果：
  `output/probes/ida-tcalc-annotations-xrefs.json`、
  `output/probes/ida-tcalc-annotations.log`。共命中 66 处、2 个引用函数；反编译路径
  同时识别 `DRAWCFRAME`、`DRAWABOVE` 和 `NOFRAME`，其中 `DRAWCFRAME` 进入
  独立画框样式。
- 系统公式样本：`CPBS` 有两个 `DRAWTEXT` 调用，平安银行 240 根日线可产生
  15 个真实 B/S 事件；`TJCJL` 产生固定文字事件；`FSCAGE` 的两个
  `DRAWNUMBER` 在最后一根 bar 输出笼子上下限数字。

## 实现

### C++ 解释器与 IR

`native/src/formula_engine.cpp` 完成以下变更：

- 将 `DRAWNUMBER_FIX` 纳入语法、展示函数、数值替身、渲染事件和参数个数校验；
- 给四类标注图元声明 `bar-price` 或 `pane-fraction` 坐标空间、参数索引、换行符、
  250 字符上限及固定文字左右对齐映射；
- 事件物化 `annotation_text`、`annotation_lines`、`annotation_price` 或
  `annotation_x/y`、水平/垂直对齐、文字可用性与 `annotation_frame`；
- 数字使用稳定的 12 位有效数字格式化，UTF-8 文本按码点安全截断；
- `DRAWCFRAME` 不再只留在样式字符串中，而是显式进入 IR。

### Svelte/TradingView 渲染

`web/src/charts/FormulaChart.svelte` 和 `web/src/types.ts` 增加价格标注与固定标注
协议：

- `DRAWTEXT/DRAWNUMBER` 通过 TradingView 的 time/price 坐标变换定位 HTML
  标签；缩放、平移、自动刻度和尺寸变化时重算；
- `DRAWTEXT_FIX/DRAWNUMBER_FIX` 按窗格宽高比例定位并应用左/右对齐；
- `&` 拆成多行；本文当时把 `DRAWCFRAME` 泛化为边框和背景，后续 TdxW renderer
  审计已纠正为只对 `DRAWTEXT` 生效，固定文字与数字不应套框；
- `DRAWICON` 与 `DRAWNUMBER_DIF` 仍使用各自适合的 marker 路径，普通文字和数字
  不再降级成方形 marker。

这仍不是 GDI 像素复刻：字体度量、遮挡顺序和像素取整继续由
`pixel_renderer_equivalent=false` 声明边界。

### 显式公开行情上下文

真实 `FSCAGE` 依赖 `FINANCE(3)` 与 `DYNAINFO(26..29)`。原 POST 路径虽然会
合并调用方上下文，却先构建完整自动上下文，冷启动约需 29.8 秒。服务端现允许
请求以 `context.automatic_market_context=false` 明确关闭自动解析，但只接受
`FINANCE/FINVALUE/DYNAINFO` 三种稳定标量组，并逐项校验
`context_bindings_required` 的有限数值；不完整请求返回 HTTP 400。完整真实公式
冷执行为 258 毫秒，正式合约执行为 121 毫秒，响应绑定为
`FINANCE#3`、`DYNAINFO#26/#27/#28/#29`。

## 固定合约

新增：

- `formula-cpbs-text-live`：真实 `CPBS` 的 bar-price 文字事件；
- `formula-fscage-number-live`：真实 `FSCAGE` 的 bar-price 数字事件与显式公开行情
  上下文。

原 `formula-tjcjl-stickline-live` 同时加强为必须存在 pane-fraction 固定文字、
多行、坐标和对齐语义。full 合约由 184 项增加至 186 项。

## 验收

- `tdx-formula-engine-tests`：通过；
- `tdx-recon-contract-tests`：通过；
- CTest：101/101；
- `npm run check`：0 errors、0 warnings；
- `npm run build`：成功，入口 `assets/index-DMe5KAd0.js`；
- 临时服务专项：3/3；全量：186/186；
- 最终正式服务专项（含 OpenAPI）：4/4，报告
  `output/api-contracts-annotations-final-selected.json`；
- 正式服务全量：186/186，报告
  `output/api-contracts-annotations-final-full.json`。

正式服务为 PID `44148`，监听 `127.0.0.1:8765`。发行 EXE SHA-256：
`520A53E11FA44A004463DA20A90D0AECE6910749AAEADDD46CE69C1B38533B63`。

第一次临时全量执行为 185/186，唯一失败是既有港股全库审计在 30 秒处发生
WinHTTP 12002；该项独立重试 1/1 通过，随后临时与正式全量均单次 186/186，
故记录为外部冷请求瞬时超时，不归因于本批公式语义变更。

后续字体表、`user.ini`、透明背景、测量/输出 API 和网页字体消费已继续闭合，
见[公式字体/GDI 记录](2026-08-09-native-formula-font-gdi.md)。该后续记录取代本文
“字体度量尚未审计”的旧状态，但仍不宣称浏览器与 GDI 字形逐像素相同。

`DRAWCFRAME` 的适用函数、HIGH/LOW 锚点、点状 leader、圆角与 `0x50` Alpha
又见[DRAWCFRAME 原生记录](2026-08-09-native-formula-drawcframe.md)；该记录取代
本文的通用 panel 近似。
