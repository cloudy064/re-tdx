# DRAWNUMBER_DIF 原生活动序列与 STYLE=2 几何

## 目标与纠错

早期解释器把 `DRAWNUMBER_DIF(COND,STYLE,START,NUM)` 近似为“每个命中点都展开
一段连续标记”，网页再把 `STYLE=2` 画成通用圆角 panel。这会在连续条件命中时
生成重复事件，也没有复现原客户端的字体选择、引线、尺寸与上下翻转。

本轮沿 `TdxW.exe!sub_977560` 的 type 23 分派进入 `sub_9626E0`，恢复为一个有
状态的原生 renderer：同一图元同时只有一段活动序列，活动期间的新触发会被忽略；
STYLE 则在每个实际绘制 bar 重新求值。

## 参数和状态机

type 23 传给 `sub_9626E0` 的四条值流顺序为：

1. `COND`；
2. `STYLE`；
3. `START`；
4. `NUM`。

条件不是通用非零判断，而是 `abs(COND-1)<0.0001`。空闲状态下第一个真条件启动
序列，源 bar 为 `source`，活动结束 bar 为 `source+NUM-1`；活动期间即使 COND
再次为真也不启动嵌套序列。每个绘制 bar 的值为
`START[source] + current-source`，因此 START 和 NUM 在触发 bar 锁定，STYLE 在
当前 bar 求值。NUM 的安全实现保留官方上限 250；非正数不产生事件。

这项修正对系统 `SQJZ` 很重要：连续为真的触发区现在只产生彼此不重叠的九转
序列，不再按每根命中 K 线重复展开。

## STYLE、文字和字体

精确浮点 STYLE 的行为为：

| STYLE | 锚点偏移 | 点状引线 | 矩形 |
|---|---:|---|---|
| `0` | 0px | 无 | 无 |
| `1` | 10px | 10px | 无 |
| `2` | 10px | 10px | 有 |
| 其他非零值 | 10px | 无 | 无 |

数字标签的绘制盒宽 8px、右对齐，`DrawTextA` flags 为 `0x826`；11—36 映射为
`A`—`Z`，字母盒宽 14px、居中，flags 为 `0x825`。两类盒高均为 14px，公共
flags 为 `0x824`。引线由 `sub_695150` 绘制，普通路径点距 4px，内部加速路径
点距 5px。

renderer 先选择字体表 index 1；若当前绘制窗口首根 bar 的 STYLE 精确等于 2，
则在进入事件循环前把整层字体切到 index 13。index 13 不是 `user.ini` 的另一项
用户配置，而由 `sub_690350` 固定创建为 Arial、logical height `+15`、weight
400。C++ IR 发布完整逐 bar STYLE 序列；Svelte 在时间轴缩放或平移后寻找首个
可见 bar，重新选择 index 1/13，并把该选择应用到整个图元，而不是按单个事件
切换字体。后端的 `annotation_effective_font_table_index` 只作为无可见坐标时的
求值窗口回退值。

## STYLE=2 几何与颜色

水平位置以 bar x 为中心。默认方向在 LOW 下方：先留 2px 锚点间距，再应用非零
STYLE 的 10px 偏移。若盒底到达或越过 `paneBottom-40`，翻到 HIGH 上方。
`DRAWABOVE` 优先放在 HIGH 上方；若盒顶到达或越过 `paneTop+25`，翻到 LOW 下方。
网页每次 TradingView 的时间轴、价格轴、尺寸或可见区变化时用当前 HIGH/LOW
坐标重算，而不是固定使用首次求值时的像素位置。

STYLE=2 的填充和边框都使用图元 COLORREF 对应的视觉 RGB。填充通过 GDI+
solid brush 使用 Alpha `0x50`；边框为 GDI 不透明、宽 1px 的闭合五点
`Polyline`。它是 8×14 或 14×14 的直角矩形，不是 `DRAWCFRAME` 的圆角路径，
也不使用主题 panel 配色。Svelte 因而移除了通用 marker/panel 路径，以同色
`#RRGGBB50` 填充、同色 1px 直角边框和 4px 点距引线独立呈现。

为避免缩放后标注把 K 线价格范围裁掉，序列图层同时放入不可见 HIGH/LOW extent
series。同期也纠正了 `DRAWCFRAME` 的 autoscale：有效 frame 只使用 HIGH/LOW，
被原生忽略的公式 PRICE 不再进入隐藏价格范围。

## 机器证据

- `output/probes/ida-tdxw-drawnumber-dif-helpers.json`
- `output/probes/ida-tdxw-drawnumber-dif-disasm.json`
- `output/probes/ida-tdxw-font-table-constructor.json`
- `output/probes/ida-tdxw-font-selector-exact.json`
- `output/probes/ida-tdxw-formula-text-helpers.json`
- `output/probes/ida-tdxw-render-types-22-23.json`
- `output/probes/ida-tcalc-drawnumber-dif-xrefs.json`

## C++、网页和契约

C++ IR 新增参数顺序、精确真值规则、活动/重叠规则、逐 bar STYLE、四类样式、
字体选择依据、宽高/对齐/flags、锚点间距、偏移、翻转阈值、填充/边框合成及
逐事件 source/offset/value/label。事件生成已改为单活动序列状态机。

Svelte 不再用 TradingView generic markers 显示连续数字，而以原生尺寸覆盖层
绘制。新增 `formula-drawnumber-dif-inline-post` 契约会验证 STYLE 2/1/0、三段
活动序列、10 个不重复 STYLE=2 事件、字体表 index 13 和全部关键几何字段。

验收结果：

- CTest 101/101；
- Svelte check 0 errors、0 warnings；生产构建成功；
- 联合专项 3/3，报告
  `output/api-contracts-drawnumber-dif-final-selected.json`；
- quick 8/8，报告
  `output/api-contracts-drawnumber-dif-final-quick.json`；
- full 198/198、199 次网络请求，报告
  `output/api-contracts-drawnumber-dif-full.json`；
- 网页入口 `assets/index-DgbdVxQJ.js`，样式
  `assets/index-DebQOLhz.css`；首页、公式工作台深链接及两项静态资源均返回 200。

发行 EXE SHA-256 为
`A7998B7492823C2DBB73092D603811E20E1894B7A85D3B5B82016FF325AFF532`。全量巡检
后正式服务已重启为 PID `11608`，只监听 `127.0.0.1:8765`，命令行为纯 C++
`tdx-tool serve --root C:\new_tdx --port 8765 --cache-root ...`；健康检查确认
379 条公式、100 个图标单元、JSN 可用且 `python_runtime=false`。

## 边界与下一步

浏览器的字体 hinting、fallback、抗锯齿、CSS 文本度量和亚像素坐标仍不等价于
Win32 GDI，故 `pixel_font_equivalent=false` 与
`pixel_renderer_equivalent=false` 保持不变。当前高价值的下一步不再是继续猜
STYLE=2，而是沿各自 renderer 审计普通 `DRAWTEXT/DRAWNUMBER` 的无框整数取整、
文字宽高偏移、边缘翻转和重叠行为；只把有 IDA 证据且系统公式真实使用的差异
加入 IR 与网页。
