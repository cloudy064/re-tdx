# COLORSTICK/VOLSTICK 原生柱体语义闭合

## 结论

公式库 379/379 条源码与数值语法已经覆盖后，本轮转向实际使用最广的展示缺口。
库内有 9 条公式使用 `COLORSTICK`、6 条使用 `VOLSTICK`，包括 MACD、VOL、AMO
等常用指标。旧网页把二者统一近似为 72% bar 间距宽的实心直方图；TdxW 的真实
行为并非如此。

- `COLORSTICK` 是绘图类型 2，由 `TdxW.exe!sub_9555B0` 绘制从零轴到指标值的
  一像素竖线；值先转为 float、乘 10000，再按 renderer epsilon 选择上涨/下跌
  主题笔。
- `VOLSTICK` 是绘图类型 1，由 `sub_957030` 绘制零轴量柱。默认先比较收盘与
  开盘；开收在 epsilon 内相等时，首根按上涨，其余再和前收比较。
- `Other/VolKUseZT=1` 会切成只按前收着色；当前安装为 0。
- `Other/RealUPK=0` 时上涨量柱为空心，为 1 时实心；下跌量柱始终实心。当前
  安装为 0。
- 柱体宽度由 `sub_987080` 从 bar center spacing 推导，而不是固定百分比：间距
  不小于 3 时先扣除 `max(spacing*0.25,2)`，否则 body 最大为 1px，再取
  `half=floor(body/2)` 和 `2*half+1` 像素宽。

## 原生证据

本轮只用 Python 驱动 IDA 的离线导出，正式工具、服务和浏览器运行时仍是纯 C++/
Svelte，不启动或转发 Python。证据文件为：

- `output/probes/ida-tcalc-style-tables.json`：TCalc 样式表确认 VOLSTICK=1、
  COLORSTICK=2、STICK=4、LINESTICK=5；
- `output/probes/ida-tdxw-formula-dispatch-current.json`：`sub_977560` 的类型分派；
- `output/probes/ida-tdxw-color-vol-stick-renderers.json`：类型 1/2 renderer；
- `output/probes/ida-tdxw-volstick-helpers.json`：量柱颜色角色与偏移 helper；
- `output/probes/ida-tdxw-fill-helper-692520.json`：实心矩形及窄柱回退；
- `output/probes/ida-tdxw-stick-config-defaults.json`、
  `ida-tdxw-stick-config-xrefs.json`：RealUPK/VolKUseZT 键和默认值；
- `output/probes/ida-tdxw-bar-spacing-fields.json`：bar 间距到 body 宽度公式。

覆盖审计保存为 `output/formula-coverage-current.json` 和
`output/formula-render-gap-inventory.csv`。

## 实现

C++ 解释器对每个有限输出 bar 物化 `events`。`COLORSTICK` 事件携带
`series_stick_color_role`；`VOLSTICK` 事件同时携带
`series_stick_open_close_color_role` 和
`series_stick_previous_close_color_role`，浏览器不再重新解释 OHLC。IR 还发布原生
类型、renderer、零轴、float 转换、epsilon、几何、宽度和填充规则。

`tdx-formula-render-environment-v1` 新增 `series_sticks`，纯 C++ 只读解析
`T0002/user.ini` 的 `RealUPK/VolKUseZT`。Svelte 用透明 TradingView histogram
保持时间轴、价格轴和 autoscale，再按 IR 以 SVG 画原生细线、空心/实心量柱；
缩放和平移时重新按当前 bar spacing 计算宽度。

新增固定契约 `formula-series-sticks-inline-post`。它要求两类逐柱事件、原生类型/
renderer/几何、两个 user.ini 开关及填充规则全部存在；把 COLORSTICK 改回
`wide-filled-histogram` 的反向 fixture 必须失败。full 契约由 199 增至 200。

## 验证与部署

- 原生 CTest：101/101；
- Svelte 检查：0 错误、0 警告，生产构建成功；
- 临时服务专项：3/3；正式服务专项：4/4；quick：8/8；
- 正式 full：200/200，201 次网络请求，报告为
  `output/probes/api-contracts-full-current.json`；
- 平安银行真实内置 MACD 与 VOL 各取得 120 个原生 stick 事件，renderer 分别为
  `tdxw-sub_9555B0`、`tdxw-sub_957030`；
- 正式 EXE SHA-256：
  `991DF0BD920A252808A541B076FF87135D5F67C77CA7A61DA25C6319AF12C2C9`；
- 网页资源：`assets/index-DLTgxijU.js`、`assets/index-BBbdHCcI.css`；
- 正式服务 PID 43392，仅监听 `127.0.0.1:8765`；健康检查为 379 条公式、100 个
  图标单元、`native_cpp=true`、`python_runtime=false`。

浏览器 SVG 的抗锯齿和亚像素栅格仍不声称与 Win32 GDI 逐像素等价；颜色角色、
配置选择、柱宽公式、实心/空心分支和零轴几何已经不再是猜测。
