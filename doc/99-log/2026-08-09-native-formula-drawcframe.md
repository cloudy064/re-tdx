# DRAWCFRAME 原生适用范围、几何与 Alpha

## 目标与纠错

早期 IR 只把 `DRAWCFRAME` 当成通用 `framed=true`，Svelte 再给价格文字、固定
文字和数字统一套主题 panel。这能表达“有框”意图，但不是 TdxW 原生语义。

本轮直接沿 `TdxW.exe!sub_977560` 的分派和四个标注 renderer 恢复真实边界：

- `DRAWCFRAME` 只对 `DRAWTEXT` 生效；
- `DRAWTEXT_FIX` 虽收到 frame 布尔参数，函数体完全不读取；
- `DRAWNUMBER/DRAWNUMBER_FIX` 的末参数是 `IndexInfo`，frame 根本没有转发；
- 有效 frame 不锚定公式的 PRICE，而锚定当前 bar 的 HIGH/LOW；
- 填充不是主题 panel，而是文字颜色的 GDI+ ARGB `0x50` Alpha。

当前 379 条系统公式只有“主力密码(适用于券商)”使用 `DRAWCFRAME`，共五个
`DRAWTEXT`，文字都是单行；其条件依赖私有 `SIGNALS_QS`。固定契约因此使用公开
日线和内联等价语句复现结构，不用零值冒充私有信号。

## 原生分派与适用范围

`sub_977560` 对绘图 type 4 调用 `sub_961290`：最后两个布尔参数分别来自样式
`DRAWABOVE`（内部值 9）与 `DRAWCFRAME`（内部值 8）。

type 7 的 `sub_958F90` 同样收到两个布尔参数，但反汇编只读取 `arg_14`
（`DRAWABOVE`），没有任何 `arg_18` 引用。type 6 的 `sub_9593C0` 与 type 8 的
`sub_959620` 则把 `arg_18` 保存为 `IndexInfo`，用于数字格式，而不是 frame。

因此 IR 保留“指令出现过”与“原生是否生效”两层：

- `DRAWTEXT`：`native-price-text-frame`；
- `DRAWTEXT_FIX`：`ignored-by-native-renderer`；
- `DRAWNUMBER/DRAWNUMBER_FIX`：`not-forwarded-to-native-renderer`。

事件中的 `annotation_frame` 现在只在第一种情况下为 true。样式中的
`draw_cframe` 仍保留源码事实，不能再直接驱动网页加框。

## 几何

`sub_961290` 先用 `GetTextExtentPoint32A` 得到每行文字宽高。有效 frame 每行都：

1. 取当前 bar 的 HIGH 与 LOW，转换成 `highY/lowY`；公式 PRICE 坐标虽然在进入
   frame 分支前算过，但后续不使用；`DRAWABOVE` 对 frame 也不再产生效果。
2. 比较 `paneBottom-lowY` 与 `highY-paneTop`。若下方空间小于等于上方空间，
   frame 放到 HIGH 上方；否则放到 LOW 下方。
3. 从 HIGH 向上或 LOW 向下画 20px 垂直 leader。调用 `sub_695150(...,4)` 时，
   普通 GDI 路径每 4px 写一个点；启用内部加速绘制标志时步长变为 5px。
4. 框左边为 `barX-textWidth/2`；宽 `textWidth+5`，高 `textHeight+4`。上方框的
   bottom 为 `highY-20`，下方框的 top 为 `lowY+20`。
5. `sub_695050` 用四次 `GdipAddPathArcI` 建立圆角路径；传入半径 4，对应直径
   8，并受框宽高钳制。
6. 文字由 `TextOutA` 画在 `(frameLeft+3, frameTop+3)`。

`&` 拆出的多行会逐行进入上述分支，但 frame 分支不使用普通文字路径累加的 y；
因此多行在同一锚点重叠。这看起来像原客户端限制，但属于真实行为，网页没有擅自
改成一个多行大框。唯一系统用例均为单行，所以不会触发该边界。

## 颜色与合成

frame 路径使用 `GdipSetSmoothingMode(...,4)`。填充 ARGB 由 TCalc COLORREF
交换为视觉 RGB 后添加 Alpha `0x50`，即 `80/255≈0.314`；合成为
`GdipCreateSolidFill/GdipFillPath`。边框使用同一视觉 RGB、Alpha `0xFF`、宽 1 的
`GdipCreatePen1/GdipDrawPath`。文字和点状 leader 也使用原文字颜色。

这与旧 CSS 的 `color-mix(... var(--bg-panel) 82% ...)` 无关；暗/亮主题不再改变
frame 的原生颜色关系。

机器证据：

- `output/probes/ida-tdxw-formula-dispatch-callers.json`
- `output/probes/ida-tdxw-formula-text-helpers.json`
- `output/probes/ida-tdxw-drawcframe-renderers-disasm.json`
- `output/probes/ida-tdxw-drawcframe-helpers.json`

## C++ IR 与 Svelte

C++ 图元新增适用范围、HIGH/LOW 锚点、空间选择规则、20px leader、4/5px 点距、
圆角 4、宽高 padding `5/4`、文字 inset `(3,3)`、填充/边框 Alpha `80/255`、
1px 边框、smoothing mode 4 和多行重叠规则。

Svelte 在每次 TradingView 缩放、平移、价格轴或尺寸变化时重新取 HIGH/LOW 的
屏幕坐标并执行同一空间判断；frame 用同色 `#RRGGBB50` 填充、同色 1px 边框、
4px 圆角和 1-on/3-off 的 20px leader。框水平平移额外保留原生宽度 padding
造成的 2.5px 中心偏移。固定文字和数字不再因源码中出现 `DRAWCFRAME` 而被误加框。

CSS 字体度量、抗锯齿和亚像素坐标仍不同于 GDI，故
`pixel_font_equivalent=false` 与 `pixel_renderer_equivalent=false` 保持不变。

## 固定契约与验收

新增 `formula-drawcframe-inline-post`，同时要求：

- `DRAWTEXT` 发布全部几何/Alpha 字段且事件 frame=true；
- `DRAWTEXT_FIX` 明确 ignored 且事件 frame=false；
- `DRAWNUMBER_FIX` 明确 not-forwarded 且事件 frame=false；
- 顶层真实字体环境存在。

验收结果：

- CTest 101/101；
- Svelte check 0 errors、0 warnings；
- Svelte build 成功，入口 `assets/index-DVEzrNX-.js`，样式
  `assets/index-Dmk6TJh5.css`；
- DRAWCFRAME 专项 1/1，报告
  `output/api-contracts-drawcframe-selected.json`；
- 最终 DRAWCFRAME/CPBS/TJCJL 联合专项 3/3，报告
  `output/api-contracts-drawcframe-final-selected.json`；
- quick 8/8，报告 `output/api-contracts-drawcframe-quick.json`；
- full 197/197、198 次请求，报告
  `output/api-contracts-drawcframe-full.json`。
- 最终重启后 quick 8/8、联合专项 3/3，报告分别为
  `output/api-contracts-drawcframe-final-quick.json` 与
  `output/api-contracts-drawcframe-final-restart-selected.json`。

发行 EXE SHA-256 为
`9B1C1792B20A310EB52084364D0B52F1F53D778F57FE0392F4E244EC3331F862`。
最终静态资源复核后正式服务已重启为 PID `33312`，只监听 `127.0.0.1:8765`。

## 下一步

该后续项现已完成：`sub_9626E0` 的单活动序列、逐 bar STYLE、10px 点状 leader、
8/14×14px 直角框、HIGH/LOW 翻转、同色 Alpha `0x50` 填充、不透明闭合边线及
首个可见 bar 的字体 index 1/13 选择均已进入 C++ IR、Svelte 与固定契约。详见
[DRAWNUMBER_DIF 专项记录](2026-08-09-native-formula-drawnumber-dif.md)。
