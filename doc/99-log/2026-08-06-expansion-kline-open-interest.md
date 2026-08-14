# 7727 扩展行情 K 线与期货持仓量

## 结论

通达信扩展市场不是普通 `7709/0x052D` K 线的另一组市场号，而是独立的
`7727` 会话和 `0x23FF` 命令。纯 C++ 统一工具现已直接实现该链路，可取得
期货等扩展品种的分时、1/5/15/30/60 分及日/周/月 K 线，并保留普通 LC1
格式没有的两个字段：

- `position`：持仓量，绑定 TCalc 的 `VOLINSTK`，公式别名 `CCL` 使用同一序列；
- 记录末尾 `float`：期货日线表现为结算价；港股扩展市场中由 TCalc 的
  `HKSHORTVOL` 读取，接口按市场标为 `settlement_price` 或
  `hk_short_volume`，同时保留中性的 `auxiliary_price`。

这条链路不依赖 Python、客户端进程、DLL 注入或 L2 账号。

## TCalc 原生证据

离线探针 `output/ida_probe_tcalc_special_functions.py` 对已知求值器做了精确反编译：

| 名称 | TCalc VA | 行为 |
| --- | --- | --- |
| `VOLINSTK` | `0x10015B80` | 从 35 字节内部 K 线记录 `+23` 读取 `uint32` |
| `HKSHORTVOL` | `0x10015710` | 从同一记录 `+31` 读取 `float` |
| `IVOLAT` | `0x1000D0F0` | 组装 46 字节头和 K 线记录，调用宿主 type/selector 35 |

反编译日志为 `output/ida-tcalc-special-evaluators.log`。扩展行情的 32 字节线上
记录中，`+20` 的 `position` 和 `+28` 的末尾浮点数，在 TCalc 内部加上 3
字节记录前缀后恰好落到 `+23/+31`，字段布局完全闭合。

后续已经继续恢复宿主 selector 35、`TQQCalc.dll` 四个导出及期权元数据映射。
`IVOLAT(N,M)` 的历史波动率、BSM/Black-76 与欧式/美式隐含波动率算法均已
闭合到纯 C++ CLI/API；证据、边界和真实样本见
[期权目录与波动率闭环](2026-08-06-option-volatility.md)。

## 7727 协议

- 请求首字节：`0x01`（普通 7709 为 `0x0C`）；
- 建链初始化：命令 `0x2454`，80 字节固定 setup 数据；
- K 线命令：`0x23FF`；
- 请求体：市场 `u8`、代码 `char[9]`、周期 `u16`、标志 `u16=1`、
  起始偏移 `u32`、数量 `u16`，共 20 字节；
- 响应体：18 字节头、记录数 `u16`，随后每条 32 字节；记录包含时间、
  OHLC、`position u32`、`trade u32` 和末尾 `float`；
- 响应仍使用 `B1 CB 74 00` 帧头和 zlib 压缩。

安装目录 `dsmarket.dat/ds_mrk.dat` 给出了扩展市场号；当前常用别名为
`qz=28`（郑州）、`qd=29`（大连）、`qs=30`（上海期货）、`cz=47`
（中金所）、`qg=66`（广州）。服务地址来自 `connect.cfg [DSHOST]`，工具内置
多个 7727 节点并自动故障切换。

## 实现与使用

```powershell
tdx-tool market kline --security 47:IFL9 --period 30m `
  --pages 2 --page-size 800 --output output\IFL9-30m.json

tdx-tool formulas evaluate --root C:\new_tdx --formula CCL `
  --market 47 --code IFL9 --period day --page-size 100

tdx-tool market instruments --count 1000 --market 47 --query IF

# 全目录扫描后筛选大商所；显式 --all 当前约需 30 秒
tdx-tool market instruments --all --market 29 `
  --output output\tdx-expansion-instruments-market29.json
```

HTTP 等价调用：

```text
/api/v1/kline?market=47&code=IFL9&period=30m&date=all
/api/v1/formulas/evaluate?market=47&code=IFL9&formula=CCL&period=day
/api/v1/market/instruments?start=0&count=1000&market=47&query=IF
```

`minute download` 仍专门输出普通市场 LC1。由于 LC1 没有持仓量和辅助字段，
该命令会明确拒绝扩展市场，避免静默丢数据；扩展品种应使用 `market kline`。

## 真实验收

2026-08-06 使用 `116.205.143.214:7727` 验证 `47:IFL9`：

- 日线 10 条成功，返回的日期为 2026-07-23 至 2026-08-05；
- 2026-08-05 收盘 4599.6、成交量 125,259、持仓量 279,700、结算价 4595.8；
- 30 分钟线正确跨 2026-08-04/05，时间序列含 10:00、10:30、11:00、
  11:30、13:30、14:00、14:30、15:00；
- CCL 真实执行：2026-08-05 持仓量 279,700，仓差 +8,524；
- CLI、`/api/v1/kline` 和 `/api/v1/formulas/evaluate` 三条路径均已通过。

样本输出为 `output/tdx-expansion-47-IFL9-day.json`、
`output/tdx-expansion-47-IFL9-30m.json` 和
`output/tdx-expansion-47-IFL9-ccl.json`。

## 扩展合约目录

同一 7727 会话的 `0x23F0` 返回目录总数，`0x23F5` 按全局偏移每页返回最多
100 条 64 字节记录。记录中已恢复 `category`、市场号、9 字节代码、17 字节
GBK 名称和 9 字节说明。统一工具新增 `market instruments` 和只读 API，目录
中的 `security` 可直接传给 `market kline`。

2026-08-06 服务器报告 142,611 条，实际分页在 142,591 条处返回空页，工具会
明确输出 `reported_total_shortfall=20` 和 `directory_exhausted=true`，不会制造
一个永远可继续的假游标。可读取记录覆盖 49 个市场号，其中：

- 中金所市场 47：89 条；`IF2608/IF2609/IF2612/IF2703` 与
  `IFL0/IFL1/IFL2/IFL3/IFL7/IFL8/IFL9` 均可直接发现；
- 大连市场 29：328 条；例如目录选出的 `29:A2609` 已继续成功取得日线，
  2026-08-06 收盘 4724、成交量 119,938、持仓量 161,162；
- 郑州 28、上海期货 30、广州 66 分别有 303、377、63 条。

全目录筛选实测约 29.4 秒；普通分页或首 1000 条内的中金所检索低于 1 秒。
目录计数是服务端动态状态，不应固化为协议常量。

## 扩展市场实时行情

继续实现 `7727/0x23FA`。请求体为市场 `u8` 加 9 字节代码，响应固定部分为
150 字节，现已结构化输出：

- 昨结/昨收参考价、开高低现价、涨跌额和涨跌幅；
- 总成交、现量、内盘、外盘、持仓量；
- 买卖各五档价格和数量（只提供一档的品种其余档位按上游返回 0）；
- 三个尚未命名的 `u32` 保留在 `unknown_u32`，不猜测业务含义。

期货响应第一个参考价格实测等于前一日结算价，因此同时输出
`pre_settlement`；保留 `pre_close` 是为了与原协议实现兼容，不把字段名差异
伪装成两份数据。

```powershell
tdx-tool market expansion-quote --security 29:A2609
```

同一能力也复用统一快照 API：

```text
/api/v1/market/snapshot?market=29&code=A2609
```

`29:A2609` 的真实样本返回现价 4724、昨结 4716、成交 119,938、现量 1、
内盘 56,325、外盘 63,613、持仓 161,162，买一 4723/5、卖一 4725/1；
与同日 K 线的 OHLC、成交量和持仓量一致。

## 扩展市场分时与逐笔

继续恢复四个公开 7727 命令，并全部纳入纯 C++ CLI 与只读 HTTP API：

| 数据 | 当日 | 历史 | 记录布局 |
| --- | --- | --- | --- |
| 分时 | `0x240B` | `0x240C` | 18 字节：时间、价格、均价、成交量、持仓量 |
| 逐笔 | `0x23FC` | `0x2406` | 16 字节：时间、原始价、成交量、增仓、性质码 |

请求和响应布局与 [pytdx 扩展行情解析器](https://github.com/rainx/pytdx/tree/master/pytdx/parser)
以及 [injoyai/tdx 的扩展协议实现](https://github.com/injoyai/tdx/blob/v0.0.84/protocol/model_ex.go)
交叉核对。运行时没有引入这些项目或 Python/Go 依赖。

```powershell
tdx-tool market expansion-timeline --security 29:A2609
tdx-tool market expansion-timeline --security 29:A2609 --date 20260805
tdx-tool market expansion-trades --security 29:A2609 --date 20260805 `
  --page-size 1800 --pages 2
```

HTTP 等价调用：

```text
/api/v1/market/expansion-timeline?market=29&code=A2609&date=20260805
/api/v1/market/expansion-trades?market=29&code=A2609&date=20260805&page_size=20&pages=2
```

真实服务验证得到以下协议细节：

- `29:A2609` 当日分时返回 121 点，21:00—23:00；末点现价 4724、持仓
  161,162，与 `0x23FA` 快照完全一致；
- 2026-08-05 历史分时返回 345 点。夜盘结束后的日盘时间在线上使用连续分钟，
  例如原始值 `1981 = 1440 + 541`，实际为下一会话段 09:01；接口保留
  `wire_minute/session_day_offset` 并输出归一化时间，不把 33:01 当成异常时间；
- 逐笔价格原始值 `4,724,000` 对应 4724，确认除数为 1000；
- `nature` 低四位承载秒，万位承载方向；结合成交量与增仓可区分多开、空开、
  双开、多平、空平、双平、多换、空换等性质；港股市场 31/48 使用 B/S 分支；
- 服务端 `start=0` 从最新记录向历史回溯，每页内部时间正序。多页结果会反转
  页顺序后输出整体时间正序，同时保留 `absolute_index` 原始回溯偏移。

真实样本保存在 `output/probes/expansion-timeline-current.json`、
`output/probes/expansion-timeline-20260805.json`、
`output/probes/expansion-trades-current.json` 和
`output/probes/expansion-trades-20260805-2pages.json`。

## 公式覆盖变化

全库 379 条的静态覆盖更新为：

- 直接可执行：217 → **220**；
- 带市场上下文可执行：319 → **322**；
- 另有 18 条显式未来函数只读公式；按适用市场取并集为 **340** 条。

新增的 `SHORTVOL/CCL/CCYD` 只在相应扩展字段存在时运行。运行体检会把不适用
当前市场的公式计入 `market_inapplicable`，不会把全空序列伪装成成功结果。
