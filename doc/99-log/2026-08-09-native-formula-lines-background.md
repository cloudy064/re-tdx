# 2026-08-09 公式条件线段、蜡烛与分区背景语义

## 目标

继续收敛 `tdx-formula-render-ir-v1` 中仍会丢失连接关系或只保留最后一根的绘图
函数。本轮覆盖 `PLOYLINE`、`DRAWLINE`、`DRAWKLINE` 和 `DRAWGBK_DIV`；全部
求值和结构化 IR 仍由 C++ 完成，Svelte 只消费结果。

## 证据

通达信公式系统帮助确认：

- `PLOYLINE(COND,PRICE)` 在条件成立处形成顶点，并依次连接这些顶点；
- `DRAWLINE(COND1,PRICE1,COND2,PRICE2,EXPAND)` 连接两次条件锚点，`EXPAND=1`
  向右延长；
- `DRAWKLINE(HIGH,OPEN,LOW,CLOSE)` 的参数顺序不是常见的 O/H/L/C；
- `DRAWGBK_DIV(COND,COLOR1,COLOR2,MODE,RANGE)` 的官方模式 `0..3` 分别为纵向
  渐变、横向渐变、边框、边框加填充，范围 `0..2` 分别为全窗格、高低价、开收价。

官方函数页：<https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html>。

`TCalc.dll` 的独立字符串/xref 探针保存在
`output/probes/ida-tcalc-lines-background-xrefs.json` 与
`output/probes/ida-tcalc-lines-background.log`：命中 20 处、2 个引用函数；
`sub_1006F720` 把 `DRAWLINE` 识别为未来函数。系统公式语料中 `DRAWLINE` 2 次、
`PLOYLINE` 2 次、`DRAWKLINE` 4 次、`DRAWGBK_DIV` 2 次。

## C++ 解释器与 IR

- `PLOYLINE` 事件逐个保存顶点及上一顶点到当前顶点的 bar/price，不再依赖普通
  输出线跨越空值；
- `DRAWLINE` 保存起点、真实终点、斜率和渲染终点；`EXPAND=1` 的渲染终点延至
  当前数据右边界，匿名绘图语句也不会从 IR 消失；
- 两者继续标记为未来/只读展示函数，不能进入扫描和回测；
- `DRAWKLINE` 图元发布 `candle_argument_order=[high,open,low,close]` 和阴阳线规则；
- `DRAWGBK_DIV` 从“仅最后一根命中”改为逐根事件，每根保留两个 Windows
  `COLORREF`、原始模式、原始范围和范围对应的价格边界。随后 IDA 已进一步定位
  原生类型 21：模式 `10..20` 是 GDI+ Alpha 实心填充，系统模式 `17` 的精确
  Alpha 为 `178/255`；详见后续
  [专项记录](2026-08-09-native-formula-drawgbk-alpha.md)。

## Svelte/TradingView

网页增加随缩放、平移、尺寸和价格刻度变化重算的 SVG 线段覆盖层：

- `PLOYLINE` 只画相邻真实顶点之间的线段；
- `DRAWLINE` 按 IR 的真实锚点和延长终点绘制；
- `DRAWGBK_DIV` 将相邻命中 bar 合并为一个原生背景区域，支持纵向/横向渐变、
  边框、边框加填充、`10..20` Alpha 实心模式以及三种价格范围；
- 隐形 scale series 只用于取得 TradingView 的价格坐标，不替代后端求值。

背景模式和 Alpha 已由后续 IDA 证据闭合；GDI 覆盖顺序和像素取整仍由
`pixel_renderer_equivalent=false` 明确隔离。

## 真实样本与固定契约

- `CYX`：两个 `DRAWLINE` 图元。真实样本的第一条线从 bar 172、11.60000038
  到 bar 199、11.39000034，斜率 -0.007777779，并向右延到 bar 239、
  11.07888917；第二条线同样保留真实锚点和延长端点；
- `ICHIMOKU`：两个 `PLOYLINE` 图元，分别产生 197、175 个真实顶点事件；
- `FKX`：120 个 `DRAWKLINE` 事件，固定 H/O/L/C 参数契约；
- 内联背景公式：69 个命中事件，双颜色为 197121/394500，模式为纵向渐变，
  范围为全窗格。

新增 `formula-background-inline-post`、`formula-cyx-drawline-live`、
`formula-fkx-candles-live` 三项契约；既有 `formula-ichimoku-stickline-live` 同时加强
为必须存在真实 `PLOYLINE` 顶点段。full 契约由 186 增至 189 项。

## 验收

- `tdx-formula-engine-tests`：通过；
- `tdx-recon-contract-tests`：通过；
- CTest：101/101；
- `npm run check`：0 errors、0 warnings；
- `npm run build`：成功，入口 `assets/index-xwg-CV3u.js`；
- 临时服务专项：4/4；报告 `output/api-contracts-lines-background-selected.json`；
- 临时服务全量首次为 188/189；唯一失败是既有
  `holder-cross-stock-live` 冷缓存访问 TQLEX 时连续三次 WinHTTP 12030。该项在旧
  正式进程的完整陈旧缓存上仍返回 200，因此没有删除契约或把外部断线伪装为通过。

为避免重启使这类完整缓存消失，`InstitutionService` 增加原子磁盘缓存：历史与详情
分文件、内容有 schema/kind/key 校验、单文件上限 64 MiB，损坏文件按冷缓存处理；
只有瞬时错误可回退，稳定业务/解析错误仍失败。纯 C++ `market institution`
新增可重复 `--cache-import`，会验证分页无缺口、无重叠、声明总数及详情一致性后
才写入缓存。本次从旧进程迁移 8 页，严格覆盖 3,503/3,503 条和 35 个报告期；
新进程首次请求显示历史/详情均 `persistent_restored=true`、上游请求数为 0。随后
TTL=0 强制刷新时上游恢复，两段均一次成功，证明持久缓存没有遮蔽实时恢复。

最终正式服务专项为 5/5，报告
`output/api-contracts-lines-background-final-selected.json`；全量为 189/189、190 次
网络请求，报告 `output/api-contracts-lines-background-final-full.json`。全量验收后
重启以释放一次性巡检缓存，最终正式服务 PID `44044`，仅监听
`127.0.0.1:8765`；重启后历史/详情都从磁盘恢复且零上游请求。网页入口为
`assets/index-xwg-CV3u.js`，发行
EXE SHA-256 为
`3DC22887D86102AF3405429ADF7B470B4EE5DE77D2C685B224F03F28A3F9D8D4`。
