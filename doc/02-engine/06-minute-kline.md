# 板块指数 1 分钟 K 线

> 目标是取得“银行 `880471`”这类板块指数的 1 分钟 OHLCV，而不是
> MACD/KDJ 指标输出。结论基于 2026-07-31 的本地安装和 IDA 数据库。

## 已确认的数据入口

通达信把沪深京普通证券和 `880xxx` 板块指数的历史 1 分钟线保存为：

```text
<通达信>\vipdoc\<市场>\minline\<市场><代码>.lc1
```

银行板块的实际文件是：

```text
C:\new_tdx\vipdoc\sh\minline\sh880471.lc1
```

`T0002\hq_cache\tdxzsbase.cfg` 中的 `1|880471|...` 把该板块映射到市场
`1`，本地前缀为 `sh`。扩展市场使用
`vipdoc\ds\minline\<市场号>#<代码>.lc1`。

### 32 字节记录

`TdxW.exe` 的 `sub_532E40` 对 `period=7` 明确选择 `.lc1`，按 32 字节
读取。每条记录的小端布局为：

| 偏移 | 类型 | 字段 |
| ---: | --- | --- |
| 0 | `uint16` | 日期编码 |
| 2 | `uint16` | 从 00:00 起的分钟数 |
| 4 | `float32` | Open |
| 8 | `float32` | High |
| 12 | `float32` | Low |
| 16 | `float32` | Close |
| 20 | `float32` | Amount |
| 24 | `int32` | Volume |
| 28 | `uint16` | 扩展字段 1 |
| 30 | `uint16` | 扩展字段 2 |

日期的解码为：

```python
year = encoded_date // 2048 + 2004
month = encoded_date % 2048 // 100
day = encoded_date % 100
```

`880471` 的两个扩展字段是每分钟上涨/下跌家数。当前读取器为保持既有
JSON/CSV 字段兼容，暂时仍命名为 `extra_1`、`extra_2`。

## 本地样本验证

当前 `sh880471.lc1`：

- 514,560 字节，即 16,080 条记录；
- 2025-08-06 至 2025-11-14，共 67 个交易日；
- 每日恰好 240 根，时间为 09:31—11:30、13:01—15:00；
- 所有记录都满足 `low <= open/close <= high`；
- 2025-11-14 最后一根收盘为 `3690.3899`，与同日 `.day` 的
  `3690.39` 一致。

这证明格式和市场映射可直接用于板块指数。它也暴露了当前机器的缓存时效
问题：分钟文件最后更新于 2025-11-14，不是当前行情。

## 读取和画图工具

仓库提供只读工具：

```powershell
python doc/90-scripts/extract_tdx_minute.py `
  --root C:\new_tdx `
  --code 880471 `
  --date latest `
  --format html `
  --output output\tdx-880471-1m.html
```

HTML 不引用网络资源，可以直接双击打开，并提供 K 线、成交量、盘中悬浮
明细和缓存日期。也可导出一个交易日或全部历史：

```powershell
python doc/90-scripts/extract_tdx_minute.py `
  --root C:\new_tdx --code 880471 --date 20251114 `
  --format csv --output C:\tmp\bank-1m.csv

python doc/90-scripts/extract_tdx_minute.py `
  --root C:\new_tdx --code 880471 --date all `
  --format json --output C:\tmp\bank-1m.json
```

市场前缀和板块名称默认从配置及现有文件自动判断。

## 如何补齐最新数据

### 1. 现有客户端的盘后下载

这是当前安装可立即使用、风险最低的刷新路径：

1. 在通达信中进入“系统 → 盘后数据下载”，或输入快捷入口 `.933`；
2. 选择“1分钟线”，设置日期并添加目标品种；
3. 下载完成后重新运行上面的 Python 工具。

主程序字符串和代码共同确认该功能会下载 1 分钟线。对于标准市场，请求
函数 `sub_6993D0` 组装 20 字节消息，操作码为 `4045`，并携带市场号、
代码、起止日期和周期 `7`；另一类市场由 `sub_6A2AF0` 使用操作码
`9239`。

#### 盘后下载使用的地址

这里没有单独的 HTTP 下载 URL。`4045` 请求进入标准行情连接
`dword_10FCD28`，目标是客户端当次选择的 `7709/TCP` 行情主站；
`880471` 的市场号为 `1`，因此走的正是这条标准通道。主站候选来自：

```text
C:\new_tdx\T0002\newhost.lst
```

当前配置含 43 个可轮换节点，客户端最近一次实际连接为：

```text
81.71.32.47:7709  广州双线主站3
```

例如 `110.41.147.114:7709`（深圳双线主站1）也是同类节点。程序不应只
硬编码一个 IP，而应读取 `newhost.lst`，连接失败时轮换。

#### 已验证的独立下载方式

直接发送老版的短 `0x052D` 请求会得到两字节 `20 03`，这是请求格式不再
匹配，而不是 `880471` 不存在。2026-07-31 已用当前长格式完成真实下载：

1. 连接任一可用的 `7709/TCP` 主站；
2. 用 `0x000D` 完成握手；
3. 发送 `0x052D` K 线请求，市场号 `1`、代码 `880471`、周期 `7/1`；
4. 按响应头的压缩长度读取，并在长度不同时使用 zlib 解压；
5. 该品种按指数记录解析，尾部两个 `uint16` 是上涨/下跌家数。

一次最多请求 800 根，`start=0, 800, 1600...` 可向历史分页。实测结果：

| `start` | 根数 | 第一根 | 最后一根 |
| ---: | ---: | --- | --- |
| 0 | 800 | 2026-07-28 10:11 | 2026-07-31 13:00 |
| 800 | 800 | 2026-07-22 14:21 | 2026-07-28 10:10 |
| 1600 | 800 | 2026-07-17 13:01 | 2026-07-22 14:20 |
| 2400 | 800 | 2026-07-14 10:11 | 2026-07-17 11:30 |

`81.71.32.47:7709`、`110.41.147.114:7709` 和
`116.205.183.150:7709` 均返回相同的最新 `880471` 数据。这样可以沿用
盘后下载的主站地址，但独立工具宜使用已验证的 `0x052D` 历史 K 线命令，
而不是依赖 `.933` 的 `4045` 任务状态机。对 `4045` 的裸调用目前只返回
周期号及 `0xFFFFFFFF` 哨兵，说明它还依赖客户端内的下载任务上下文。

当前长格式 `0x052D` 的请求布局为：

| 偏移 | 类型 | 值/字段 |
| ---: | --- | --- |
| 0 | `uint8` | 帧前缀 `0x0C` |
| 1 | `uint32` | 消息 ID |
| 5 | `uint8` | 控制字 `1` |
| 6 | `uint16 × 2` | 长度，两份均为 `44` |
| 10 | `uint16` | 命令 `0x052D` |
| 12 | `uint16` | 市场号 |
| 14 | `char[6]` | 代码 |
| 20 | `uint16 × 2` | 周期和周期参数 |
| 24 | `uint16 × 2` | `start` 和 `count` |
| 28 | `uint16` | 复权模式 |
| 30 | `uint32` | 复权锚点日期 |
| 34 | `byte[20]` | 保留字段 |

公开实现可作为交叉核对：

- <https://github.com/electkismet/eltdx/blob/main/src/eltdx/protocol/commands/klines.py>
- <https://github.com/electkismet/eltdx/blob/main/src/eltdx/protocol/frame.py>

#### 独立更新工具

仓库已经提供无第三方依赖的下载器：

```powershell
python doc/90-scripts/download_tdx_minute.py `
  --download `
  --root C:\new_tdx `
  --code 880471 `
  --pages 2 `
  --output output\tdx-880471-live-1m.html
```

它会：

1. 从板块配置自动判断 `880471 -> sh/市场 1/index`；
2. 读取 `T0002\newhost.lst` 并在连接失败时切换主站；
3. 握手后按每页 800 根向历史分页；
4. 解析价格差分、量额编码和上涨/下跌家数；
5. 把通达信原缓存、上次快照和本次数据按“日期+分钟”去重合并；
6. 原子写入 `output\minute\sh880471.lc1`，并生成可双击打开的 HTML。

默认不覆盖通达信安装目录。省略 `--download` 时只打印计划，不建立网络
连接，也不写输出。2026-07-31 的完整端到端验证使用 `--pages 30`：
服务器在第 28 页 `start=21600` 返回 142 根短页并自动停止，共下载
21,742 根（2026-03-20—2026-07-31）；与原缓存合并后的快照为 37,822
根。最新交易日页面在 13:22 时包含 142 根，并通过 `.lc1` 重新解析及
单文件 HTML 自包含校验。另以不可连接地址作为第一候选，确认工具能够
自动切换到第二个 `7709` 主站继续下载。

### 2. 官方 TQ 量化接口

支持 TQ 的通达信版本提供：

- `tq.refresh_kline(stock_list=[...], period="1m")`；
- `tq.get_market_data(..., period="1m")`；
- 本机 HTTP `POST http://127.0.0.1:17709/`。

官方说明 TQ 支持板块指数和 1 分钟周期。可优先试
`880471.SH`，再根据返回的代码规则修正市场后缀。当前本机版本没有
`PYPlugins\tqcenter.py`，也没有监听 17709，因此暂时不能直接验证此路径。

官方资料：

- <https://help.tdx.com.cn/quant/docs/markdown/mindoc-1cfsjkbf8f3is/mindoc-1d00kk3jsibbc.html>
- <https://help.tdx.com.cn/quant/docs/markdown/mindoc-1ctuhthaq5qmg/mindoc-1h10g60jt68sc.html>
- <https://help.tdx.com.cn/quant/docs/markdown/mindoc-1hdhbmi50d038.html>

## 与当日分时接口的区别

主程序中的 `GetMinuteData`/`GetMinuteDataACK` 使用
`zst_cache`、`.tfz/.zsm` 和 26 字节点记录，字段更接近成交价、均价和
成交量，并预生成 240 个交易分钟。这是当日分时走势链路，不是历史
OHLC K 线。

因此：

- 历史和盘后 1 分钟 K 线：读取 `.lc1`；
- 当前分时走势：继续追 `GetMinuteData` 或 `FastHQ.Subscribe`；
- 要让当天形成完整 K 线，优先走盘后下载或 TQ `refresh_kline`。

## 纯 C++ 本地多周期入口

统一工具现可直接把安装目录缓存作为 `market kline` 的数据源：

```powershell
tdx-tool market kline --security sz:000001 --source local `
  --period 30m --pages 2 --page-size 80 --root C:\new_tdx `
  --output output\local-30m.json

tdx-tool market kline --security sz:000001 --source local `
  --period week --page-size 120 --root C:\new_tdx `
  --output output\local-week.json
```

`time/1m/5m/15m/30m/60m` 使用 `.lc1`，`day/week/month` 使用 `.day`。
`start` 是从最新端计算的历史偏移，输出保持时间升序，并返回
`next_start/has_more`。接口同时给出缓存的 `first_available_date` 和
`last_available_date`；本地文件可能落后于当前交易日，调用方不应把它当成实时源。

## 港股扩展市场复权

扩展市场 `31/48` 的港股日线支持 `none/qfq/hfq/fixed_qfq/fixed_hfq`。复权数据
不沿用 A 股 `0x000F`，而是解密本地 `hkqxinfo.dat/hkqxinfo2.dat`，以相邻累计
乘数和偏移执行 TdxW 原生仿射变换。响应的 `adjustment.method` 为
`tdx-hk-native-affine-v1`，每根柱显式携带比例与偏移；只调整 OHLC，不改成交量、
持仓量和港股沽空量。

真实 `31:00001` 的 700 根日线中，qfq 首根收盘由 `40.300003` 调整为
`35.612999`，hfq 末根由 `72.600006` 调整为 `77.287010`，窗口内四次现金事件
的合计偏移为 `4.687004`。CLI `market kline` 和 HTTP `/api/v1/kline` 共用该
实现；固定模式还要求 `anchor_date`。公式 `DIVFACTOR` 只复用股份乘数，不复用
价格偏移。
