# tdx-l1stream

独立纯 C（C11）实现的通达信 L1 行情客户端与常驻推流服务。与仓库里的 C++
研究工具 `../native` **互不依赖**：C++ 版本保留为协议证据与逐字段对照物，
这个目录是要做成生产服务的实现。

一句话定位：**上游只能轮询（协议没有增量接口），增量在本进程里算出来。**

## 为什么单独做一个 C 版本

- 目标形态是长期常驻的行情流服务，只需要 L1 那一小块；
- 与 C++ 版本逐字段对照，任何一方解码错了都能立刻发现；
- 不引入 C++ 运行时，部署就是单个 exe + zlib。

## 目录

```
c/
  CMakeLists.txt          project(... LANGUAGES C)
  include/
    tdx_l1.h              umbrella header
    tdx_error.h           状态码 + 错误消息
    tdx_bytes.h           可增长缓冲、小端读写、printf 式追加
    tdx_thread.h          Win32 CRT / pthreads 线程、互斥、条件变量
    tdx_endpoint.h        端点解析 + connect.cfg HQHOST 发现
    tdx_frame.h           7709 请求组帧 / 响应解帧（16 字节头 + zlib）
    tdx_quote.h           证券身份、varint/wire 数值、0x0547 请求与解析
    tdx_directory.h       0x044D/0x044E 证券目录、品种与板块分类
    tdx_pool.h            持久连接池 + 并行批调度
    tdx_state.h           变更掩码 + 上一值哈希表
    tdx_format.h          唯一的 JSON 渲染路径（CLI 与 Hub 共用）
    tdx_hub.h             常驻 Hub：裁剪 + 分层节拍 + 订阅者队列
    tdx_serve.h           只读 HTTP / SSE 服务
    tdx_md5.h             RFC 1321 MD5（下载校验用）
    tdx_download.h        0x02C5/0x06B9 资源传输
    tdx_trades.h          0x0FC5/0x0FC6 L1 成交明细
    tdx_trades_json.h     成交明细的 JSONL 渲染
    tdx_kline.h           0x052D 多周期 K 线
    tdx_kline_json.h      K 线的 JSONL 渲染
    tdx_timeline.h        0x0537 当日分时（逆向所得）
    tdx_timeline_json.h    分时的 JSONL 渲染
    tdx_auction.h        0x056A 集合竞价序列
    tdx_auction_json.h    竞价序列的 JSONL 渲染
    tdx_snapshot.h        0x054C 全量快照
    tdx_finance.h         0x0010 批量财务基础信息
    tdx_snapshot_json.h   全量快照的 JSONL 渲染与累加器
    tdx_finance_json.h    财务记录的 JSONL 渲染与累加器
    tdx_capital.h         0x000F 股本变迁与除权
    tdx_capital_json.h    股本事件的 JSONL 渲染与累加器
    tdx_gbbq.h            本地加密 GBBQ 权息文件
    tdx_limits.h          0x0452 特殊涨跌停表
    tdx_limits_json.h     涨跌停记录的 JSONL 渲染
    tdx_json.h            有界 JSON 解析器
    tdx_jsn.h             JSN 表格格式与 GBK 转换
    tdx_bonds.h           债券字段映射与单位语义
    tdx_bonds_json.h      债券行的 JSONL 渲染
    tdx_convertible.h     可转债概览字段映射
    tdx_convertible_json.h 可转债行的 JSONL 渲染
    tdx_convertible_join.h 可转债六文档连接
    tdx_zst.h             zst_cache .img 容器 + tag 流解码
    tdx_zst_replay.h      增量重放：把变化流折成完整快照
    tdx_zst_json.h        快照的 JSON 渲染
    tdx_zst_day.h         「一只票 + 一个日期」的取数编排
  src/
    tdx_bytes.c  tdx_text.c  tdx_thread.c  tdx_endpoint.c  tdx_frame.c
    tdx_quote.c  tdx_directory.c  tdx_pool.c  tdx_state.c  tdx_format.c
    tdx_hub.c  tdx_serve.c  tdx_md5.c  tdx_download.c  tdx_trades.c
    tdx_trades_json.c  tdx_kline.c  tdx_kline_json.c  tdx_timeline.c
    tdx_timeline_json.c  tdx_zst.c  tdx_zst_replay.c  tdx_zst_json.c
    tdx_auction.c  tdx_auction_json.c  tdx_snapshot.c  tdx_snapshot_json.c
    tdx_finance.c  tdx_finance_json.c  tdx_capital.c  tdx_capital_json.c
    tdx_gbbq.c  gbbq_cipher_state.h (generated)
    tdx_limits.c  tdx_limits_json.c  tdx_json.c  tdx_jsn.c
    tdx_bonds.c  tdx_bonds_json.c  tdx_convertible.c  tdx_convertible_json.c
    tdx_convertible_join.c
    tdx_zst_day.c  main.c
  tests/
    test_frame.c  test_quote.c  test_directory.c  test_endpoint.c
    test_pool.c   test_state.c  test_hub.c  test_zst.c  test_download.c
    test_trades.c  test_kline.c  kline_fixtures.h (generated)
    test_timeline.c  timeline_fixtures.h (generated)
    test_auction.c   auction_fixtures.h (generated)
    test_snapshot.c  snapshot_fixtures.h (generated)
    test_finance.c   finance_fixtures.h (generated)
    test_capital.c   capital_fixtures.h (generated)
    test_gbbq.c      gbbq_fixtures.h (generated, real ciphertext)
    test_limits.c    limits_fixtures.h (generated)
    test_json.c      (grammar vectors, no capture)
    test_jsn.c       jsn_fixtures.h (generated, whole raw GBK payload)
    test_bonds.c     (captured + synthetic cases, labelled)
    test_convertible.c  convertible_fixtures.h (generated, six reduced captures)
    test_convertible_join.c  (the same six fixtures, joined)
```

## 构建

需要 CMake ≥ 3.20、C11 编译器、zlib。

```powershell
# 关键：必须把编译器自己的 bin 目录放进 PATH，否则 gcc 会静默失败（exit 1、零输出）
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH

cmake -S c -B build/l1stream-gcc -G Ninja `
      -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe `
      -DCMAKE_BUILD_TYPE=Release
cmake --build build/l1stream-gcc
ctest --test-dir build/l1stream-gcc --output-on-failure
```

已验证环境：MSYS2 UCRT64 GCC 15.1.0 + zlib 1.3.1 + Ninja（VS 自带），
23/23 测试通过、0 warning。

`test_zst` 与 `test_endpoint` 会用真实文件：前者默认读
`C:/new_tdx/T0002/zst_cache`（可用 `TDX_ZST_SAMPLE_DIR` 或 argv[1] 改指向），
文件不在时打印 `skip:` 并以 0 退出；后者在构建目录里自建一个 `connect.cfg` 夹具。

### 踩过的工具链坑

- **`gcc.exe` 会静默失败**：PATH 里没有编译器自己的 `bin` 时，它 exit 1 且
  零输出，看起来像"编译通过但没产出"。
- PATH 里出现 `C:\msys64\usr\bin`（`sh.exe`）会让 CMake 无法创建
  `MinGW Makefiles` 生成器；用 Ninja 绕开，Ninja 可以直接用 Visual Studio
  自带的那份。
- MinGW 在 `-std=c11` 下默认链接旧 MSVCRT 的 `printf`（不支持 `%zu`）；
  CMakeLists 对 MINGW 加了 `__USE_MINGW_ANSI_STDIO=1`。
- `FindZLIB` 找不到时 CMakeLists 退回 `-lz`，走编译器 sysroot。

## 命令

```powershell
# 单批探测
tdx-l1stream probe --security sz000001 sh600000 --root C:\new_tdx

# 枚举服务端证券目录
tdx-l1stream securities --market sz,sh,bj --category all --limit 20 --root C:\new_tdx

# 全市场扫描一轮
tdx-l1stream sweep --market sz,sh,bj --category a_share -j 6 `
                   --root C:\new_tdx --output output\l1.json

# 持续推流到 stdout（JSONL，只输出变化）
tdx-l1stream watch --market sz,sh,bj --category a_share -j 6 `
                   --interval-ms 1000 --iterations 0 --root C:\new_tdx

# 常驻服务：Hub + HTTP/SSE
tdx-l1stream serve --market sz,sh,bj --category a_share -j 6 `
                   --interval-ms 1000 --tier-warm-ms 3000 `
                   --idle-interval-ms 15000 --idle-rounds 30 `
                   --port 8790 --root C:\new_tdx

# 历史某天的完整 L1 盘口：优先读客户端自己的 zst_cache，缺了才去下载
tdx-l1stream day --security sz000623 --date 20260612 `
                 --root C:\new_tdx --output output\day-000623-20260612.jsonl

# 只要变化，附带原始 tag 表；--no-cache 强制走传输
tdx-l1stream day --security sz000623 --date 20260612 --cache-dir C:\new_tdx\T0002\zst_cache `
                 --changed-only --raw --max-records 20 --quiet

# 当日成交明细（0x0FC5）
tdx-l1stream trades --security sz000623 --root C:\new_tdx --output output\trades-today.jsonl

# 历史某天的成交明细（0x0FC6），分页自动走完再按时间正序输出
tdx-l1stream trades --security sz000623 --date 20260612 --root C:\new_tdx `
                    --page-size 2000 --max-pages 100 --output output\trades-000623-20260612.jsonl

# 日线（最近 6 天）；K 线按时间正序输出，最后一行是汇总
tdx-l1stream kline --security sz000623 --period day --page-size 6 --max-pages 1 --root C:\new_tdx

# 1 分钟线，往回翻到某一天只要那一日；分页走多一点再把该日留下来
tdx-l1stream kline --security sz000623 --period 1m --start 16520 --page-size 800 --max-pages 2 `
                   --date 20260612 --root C:\new_tdx --output output\kline-000623-1m-20260612.jsonl

# 指数要 breadth 字段：880xxx 自动识别，普通指数代码显式 --index
tdx-l1stream kline --security sh000001 --period day --index --page-size 3 --root C:\new_tdx

# 当日分时：240 点，价格 + 累计均价 + 每分钟成交量（手）
tdx-l1stream timeline --security sz000623 --root C:\new_tdx `
                     --output output\timeline-000623-today.jsonl

# 集合竞价过程：selector 0 只开盘，非 0 开盘+收盘
tdx-l1stream auction --security sz000623 --root C:\new_tdx
tdx-l1stream auction --security sz000623 --selector 1 --root C:\new_tdx `
                     --output output\auction-000623-both.jsonl

# 全市场 L1 快照（0x054C，批上限 80；--market/--category 与 sweep 同样的 universe 口径）
tdx-l1stream snapshot --market sz,sh,bj --category a_share --root C:\new_tdx `
                      --output output\snapshot-ashare.jsonl

# 全市场财务（0x0010，一次请求带一串证券，记录定长）
tdx-l1stream finance --market sz,sh,bj --category a_share --root C:\new_tdx `
                     --output output\finance-ashare.jsonl

# 股本变迁与除权（0x000F，一只一请求；不带 --security 就是整个 universe）
tdx-l1stream capital --security sz000001 --root C:\new_tdx `
                     --output output\capital-000001.jsonl

# 同一批事件的离线来源（本地加密文件），两者的输出可以逐行 diff
tdx-l1stream capital --local --security sz000001 --root C:\new_tdx `
                     --output output\capital-local-000001.jsonl

# 特殊涨跌停表（0x0452，每行一次请求，全表约 780 行）
tdx-l1stream limits --root C:\new_tdx --output output\limits.jsonl

# JSN 资源（GBK 编码的 JSON 表格，走 0x02C5/0x06B9 传输）
tdx-l1stream jsn --resource list/zq_tx201.jsn --root C:\new_tdx `
                --output output\jsn-zq_tx201.jsonl

# 同一份资源按债券字段映射（规模列的单位随资源而定）
tdx-l1stream jsn --resource list/zq_gz201_1.jsn --bonds --root C:\new_tdx `
                --output output\bonds-zq_gz201_1.jsonl

# 可转债概览（只映射概览文档，不做参考实现的六文档连接）
tdx-l1stream jsn --resource list/kzz_kzzsy201_1.jsn --convertible `
                --root C:\new_tdx --output output\convertible.jsonl

# 六份文档一次取回并连接（这才是参考实现的完整视图）
tdx-l1stream convertible --root C:\new_tdx `
                       --output output\convertible-join.jsonl
```

通用参数：`--security`（可重复）、`--market sz,sh,bj`、`--category`、`--limit`、
`--host`、`--root`、`--timeout-ms`、`--endpoints`、`-j/--connections`、
`--batch-size`、`--iterations`、`--interval-ms`、`--output`。
Hub 相关：`--tier-warm-ms`、`--idle-interval-ms`、`--idle-rounds`、
`--heartbeat-ms`、`--max-subscribers`、`--port`。
`day` 相关：`--date YYYYMMDD`、`--cache-dir`、`--refresh`、`--no-cache`、
`--raw`、`--changed-only`、`--max-records`、`--quiet`。
`trades` 相关：`--date YYYYMMDD`（省略＝当日 `0x0FC5`，给出＝历史 `0x0FC6`）、
`--page-size`、`--max-pages`、`--quiet`。
`kline` 相关：`--period NAME`、`--start N`、`--page-size`（默认 800）、
`--max-pages`（默认 20）、`--index`/`--stock`、`--date YYYYMMDD`（只筛分钟级周期）、
`--quiet`。

## 历史 L1：把 zst_cache 的变化流重放成完整快照

`day` 回答的问题和前面的命令完全相反。轮询那边上游只会整条重发，增量是**本进程
算出来的**；历史这边上游只发**变化**，完整记录得**重新拼回来**。

### 资源与容器

```
远端   hishf/date/<YYYYMMDD>/<sz|sh|bj><代码>.img
本地   <root>\T0002\zst_cache\<sz|sh|bj><代码>_<YYYYMMDD>.img
```

即客户端自己的缓存名就是远端名把日期从目录挪进文件名。

容器 24 字节头 + zlib 流：`[8]` 压缩长度 = 文件大小-24，`[16]` 解压后长度。
载荷是 tag 流：

```
tag 3 <8 字符证券键>      记录开始（2 位市场数字 ASCII + 6 位代码 ASCII）
tag 2 <2 字符字段号><值>   值一直读到下一个 <0x20 的字节（就是下一个 tag）
tag 4                    记录结束
```

注意**字段之间没有分隔符**：值靠下一个 tag 字节终止，整个记录只有一个 `04`。
字段号是**两个字符**、共 71 个；同一字段的值宽度不固定（同一个价格会写成
`17.39` / `17.3900` / `17.390000`）。

### 字段表（4 个真实样本、18904 条记录标定）

| tag | 含义 | 证据 |
|---|---|---|
| `0T` | 时间 HHMMSS | 每条必有；`83436.000` 这种带小数 |
| `04` | 昨收 | 全天恒定；且 `1E/1F` 正好是它的 ±10% 取整 |
| `05` | 开盘 | 只在 09:25 竞价撮合那一刻第一次出现 |
| `06`/`07` | 最高/最低 | 逐笔刷新日内极值 |
| `08` | 最新价 | |
| `09` | 累计成交笔数 | 单调不减；且**与 0x0FC6 成交明细里 `order_count` 之和 4/4 精确相等**（23175 / 22723 / 66690 / 38070），见 `output/trades_crosscheck_evidence.txt` |
| `10` | 累计成交量（股） | 单调不减；`1A/10` 恒在 `[最低, 最高]` 内 |
| `1A` | 累计成交额（元） | 竞价那一笔上 `1A == 10 × 价格` |
| `1C` | 市盈率 | `1C/08` 每只票恒定（离散度 < 5e-4），同一只票两天同一个比值，亏损票为 0 |
| `1E`/`1F` | 涨停/跌停 | `== round(1.1×04, 2)` / `round(0.9×04, 2)`，4/4 样本 |
| `1G`/`1H` | 委买均价 / 总买量 | native 侧恢复的 TdxW 界面标签 `0xCA1330..0xCA1348`；且 `1G ≤ 最新价 ≤ 1I`、`1H ≥ 五档买量和` |
| `1I`/`1J` | 委卖均价 / 总卖量 | 同上 |
| `0D` | 阶段文本 `S0 O0 B0 T0 C0 E0 A0` | 一天 9 次迁移 |
| `20..29`/`30..39` | 买 1..10 价 / 量 | |
| `40..49`/`50..59` | 卖 1..10 价 / 量 | |
| `25..29 35..39 45..49 55..59` | 第 6..10 档 | **4 个样本里全部是「字段在、值为空」**，本实现不会替它们编数据 |
| `1i..1m` | 阶段相关的辅助块 | 每个文件约 2 条 |
| `0a..0c 1B 1D 1v 1w Z3 Z4` | 从未非空 | 保留为原始 tag，不给名字 |

`09` 在 native 侧被命名为 `open_interest`（期货口径）。现在不用争了：把它和
`0x0FC6` 成交明细的 `order_count` 逐日对账，4 个样本**分毫不差**，所以它是
**累计成交笔数**。native 那个名字是期货消费者的残留。

### 重放模型

每只证券维护一张 `tag -> 值` 表；每条记录只覆盖它带的 tag，其余保持不动，
得到的就是那一刻的完整盘口。**「带了」和「变了」是两件事**，分别上报：

- `changed_tags`：原始文本确实不同的 tag（保真的线上视图）；
- `changed`：类型化投影不同的逻辑组（`new/time/last/ohlc/limits/trades/volume/
  amount/book/aggregate/valuation/phase/other`，与 `tdx_diff_mask` 同一风格）。

比较规则只有一条：两边都能整串解析成有限小数就按数值比，否则按去掉尾部填充的
文本比。所以 `17.3900` 与 `17.390000` 不算变化，`0D` 的尾部空格也不算。
真实样本里 4647 条有 1 条没带任何变化（首两条 header 记录完全相同）。

JSONL 每行一只票的完整快照，键**永远在**、没收到过的值给 `null`，这样下游不用猜
「0 是零还是未知」：

```json
{"type":"zst_snapshot","security_id":"SZ000623","record":4646,"sequence":4646,
 "time":"15:32:36","time_hhmmss":153236,"changed":["time"],"changed_tags":["0T"],
 "merged_tags":71,"record_tags":1,"dropped_tags":0,
 "last_price":17.390000,"pre_close_price":16.720000,"open_price":16.790000,
 "high_price":17.490000,"low_price":16.730000,"limit_up_price":18.390000,
 "limit_down_price":15.050000,"trade_count":23175,"volume":16826061,
 "amount":290028487.9000,"average_price":17.236862,"pe_ratio":8.680000,
 "aggregate_bid_price":16.910000,"aggregate_bid_volume":839685,
 "aggregate_ask_price":17.920000,"aggregate_ask_volume":2193166,"phase":"E0",
 "buy_levels":[{"price":17.380000,"volume":39800}, ...],
 "sell_levels":[{"price":17.390000,"volume":34700}, ...]}
```

`volume`/`aggregate_*_volume`/`buy_levels[].volume` 是**股**，`amount` 是**元**，
`trade_count` 是**笔**。`--raw` 追加 `fields` 对象给出合并后的原始 tag 表。

`record` 是文件内下标，`sequence` 是该证券第几次快照。

### 下载：0x02C5 / 0x06B9 已移植，但公开节点不提供这些文件

两条命令与 JSN 下载是同一对：`0x02C5`(709) 查长度+MD5（请求体 = 路径 NUL 补齐到
40 字节），`0x06B9`(1721) 分块取（请求体 = `u32 偏移 + u32 长度 + 路径` 补齐到
308 字节，单块上限 30000）。`day` 会先看本地缓存，没有再走这两个命令，并按服务端
公布的 MD5 校验。

**实测结论（证据见 `output/zst_transfer_evidence.txt`）**：

- 传输链路本身是对的：用同一套代码从公开节点拉 `bi/list/zq_aaa201.jsn`，
  **33,414,573 字节、1114 个分块、MD5 `52d2752e…` 逐字节吻合**。
- `hishf/date/...` 在**全部 48 个 HQHOST** 上都返回
  `00 00 00 00 01` + 33 个 0，即「长度 0」＝服务端没有这个资源。
- 把请求头改写成 TdxW 自己发的字节（前缀 0、message id 0、control 0，与抓包
  `output/tap-tdxw.jsonl` seq 47 逐字节相同）**结果一样**，所以不是组帧差异。
- 抓包里客户端确实用这个路径请求过（seq 47 是 52 字节的 `0x02C5`，seq 48..52 是
  5 个 320 字节的 `0x06B9`，正好够 141227 字节），说明这些文件是**在带权益的
  TdxW 会话里**取到的；无名 7709 会话拿不到。

因此 `day` 现在的用法是：**离线重放客户端已经缓存的 `.img`**（完全可用、已验证），
需要新日期时要么让 TdxW 自己下下来，要么等权益/口令链路打通。

### 顺带修掉的一个老问题

`connect.cfg` 的键是 `HostNum` / `IPAddress01` / `Port01` 这种大小写混写，而端点
发现的 INI 查表用 `strcmp` 对小写键做精确比较，于是**每一条都没匹配上**，
`tdx_endpoint_pool_load` 一直悄悄退化成单个编译内置节点。现在查表折叠大小写，
48 个节点按 `PrimaryHost` 轮转排好，并补了 `test_endpoint` 把 INI 契约钉住。

### `.zsm` 伴随文件：结构已量出，语义部分标定

`0#<代码>.zsm` 每天追加**固定 39028 字节**（000078 一天 = 39028，000534 两天 =
78056），说明它是一张预分配的定长表。实测结构（`output/zst_zsm_stride.txt`、
`output/zst_zsm_crosscheck.txt`）：

```
每天一块 39028 字节 = 28 字节头 + 1500 个 26 字节槽位
  头：u32 日期，float32 昨收，float32 开盘，其余为零
  槽：u8 序号，0x02，float32 A，float32 B，u32 量(手)，12 字节零
```

证据：

- 三个文件的块头都对得上：`date` 就是 `.img` 文件名里的日期；`昨收`/`开盘`
  与同一天 `.img` 里 `04`/`05` 的终值一致（4/4 块）。
- 每天**恰好 240 个槽位非零**，正好是一个交易日的分钟桶数
  （09:30–11:30 加 13:00–15:00，各 120 分钟）。
- **每个槽的 `u32` 相加 == 该日 `.img` 的成交量（手），比值 1.0000（4/4 块）**：
  000623 168261 vs 168260.61、000078 1651282 vs 1651281.78、
  000534 0911 146077 vs 146076.76、000534 0713 210299 vs 210299.14。
  所以这个 `u32` 是**那一分钟的成交量（手）**。
- 最后一个槽的 A 等于当日收盘价（4/4 块）；B 收敛到当日 VWAP 附近
  （000623 17.2415 vs 17.2369，000534 0713 27.3286 vs 27.3409），
  所以 B 是**均价**。
- 整块只有前 ~6.3 KB 被写过，后面全是零填充。

A 到底是「分钟收盘价」还是别的分钟统计量还没定：它和 `.img` 在该分钟键上的
`08` 不完全相等。这一点留待后续，不要当成已知。

## L1 成交明细：0x0FC5 / 0x0FC6

先说清楚名字，因为「逐笔成交」在本仓库里指两个不同的东西：

| | 命令 | 精度 | 权限 |
|---|---|---|---|
| **L1 成交明细**（本节） | `0x0FC5` 当日 / `0x0FC6` 历史 | **分钟** | **公开** |
| L2 逐笔成交 | 内置 `1364` / SDK `4655`·`1801`，52 字节源记录 | 秒级逐条 | 待授权 |

native 侧自己的措辞是「公开 L1 成交明细，时间精度为分钟；不是 Level2 秒级逐笔
成交或委托」，`doc/04-current-state.md` 也把「逐笔成交」列在 L2 那一段。

### 线格式

```
请求  0x0FC5  u8 市场, 0x00, 代码[6], u16 start, u16 count            (12 字节)
      0x0FC6  u32 日期, u16 市场, 代码[6], u16 start, u16 count        (16 字节)
应答  0x0FC5  u16 条数, 随后记录
      0x0FC6  u16 条数, f32 价格基, 随后记录
记录  u16 日内分钟数, 然后 5 个 varint：价格增量, 成交量(手), 笔数, status, tail
```

- **价格在页内累加**，累加器从 0 起，而第一条的增量本身就是绝对价格，所以每一页
  自洽：`start=0` 和 `start=10` 两页的第一条都解出 17.39。跨页不要把累加器接下去。
- 刻度：代码前缀 `10/11/12/15/16/50/51/52/53/56/58` → 1000，其余 100。
- `status`：0 买 / 1 卖 / 2 中性（15:00 集合竞价就是 2），其余按 `status_N` 原样报出，
  5 是 15:00 之后的**盘后固定价格**成交。
- 分页游标向前走、页是**倒序**回来的（第一页是当天最后一批），所以取完要整体反转。
  默认每页 1800（当日）/ 2000（历史），上限 100 页；当日模式的日期由 `0x0004`
  应答的第 6 字节给出（`0x0FC5` 自己的应答不带日期）。
- 解析要求**刚好消费完**整个应答体，多一个字节就报 trailing，短读不会被当成干净的一页。

### 交叉印证（4/4 精确）

拿 `trades` 和同一天的 `.img` 对账（`output/trades_crosscheck_evidence.txt`）：

| 样本 | 明细 `order_count` 之和（≤15:00） | `.img` 的 `09` | 明细 `volume_hand` 之和 | `.img` 的 `10`/100 |
|---|---:|---:|---:|---:|
| sz000623 20260612 | 23175 | 23175 | 168261 | 168260.61 |
| sz000078 20260907 | 22723 | 22723 | 1651282 | 1651281.78 |
| sz000534 20260713 | 66690 | 66690 | 210299 | 210299.14 |
| sz000534 20260911 | 38070 | 38070 | 146077 | 146076.76 |

两条结论：

1. **`09` 就是累计成交笔数**——不再是统计推断，是逐日精确对账。
2. `.img` 的累计量在 **15:00 收盘截止**，而成交明细还带盘后固定价格那几笔
   （`time>15:00`、`status=5`）。不按时间切分时明细恒 ≥ `.img`，差值正好是这部分的
   量，4/4 样本都对得上。

`amount` 有约 0.006% 的对不上，原因是**一个 tick 的价格是该 tick 最后一笔的价格，
而成交量是整个 tick 的**；跨价 tick 上 `价格 × 量` 不等于真实成交额。这不是解码错。

## 多周期 K 线：0x052D

一条命令服务所有周期，周期是请求里的一个 u16，不是不同命令。

```
请求 42 字节
  [0..1]   u16 市场号
  [2..7]   6 个 ASCII 代码字符，不足 6 位补 0（后面的字段不移动）
  [8..9]   u16 周期号
  [10..11] u16 周期参数（恒为 1）
  [12..13] u16 起始记录
  [14..15] u16 条数，1..800
  [16..41] 0
应答
  u16 条数，随后每条
    分钟级周期： u16 LC1 日期字, u16 日内分钟数
    日线及更慢： u32 日期 YYYYMMDD
    varint 开 相对【上一条的收】的增量
    varint 收 相对本条开的增量
    varint 高 相对本条开的增量
    varint 低 相对本条开的增量
    u32 量、u32 额（wire 浮点）
    仅指数模式： u16 涨家数, u16 跌家数
```

| 周期 | id | 分钟级 |
|---|---:|---|
| `time` / `1m` | 7 | 是（共用一个 id，`--period` 决定回报哪个名字） |
| `5m` / `15m` / `30m` / `60m`(`1h`) | 0 / 1 / 2 / 3 | 是 |
| `day` / `week` / `month` | 4 / 5 / 6 | 否 |

三个容易搞错、所以被测试钉住的点：

1. **价格是千分位**（`milli / 1000`），不是报价和成交明细那套 100/1000 刻度。
2. **增量沿「到达顺序」串接，而服务端先给最新的**，所以时间排序只能在整轮走完之后做。
3. **LC1 日期字不是天数**：`年 = 字 / 2048 + 2004`，余数是 `月*100+日`；只覆盖 2004 年以后。
   所以字 101 是 2004-01-01，字 309 是 2004-03-09。

指数模式的自动识别只看 `880xxx`/`881xxx`（通达信板块指数，挂在上交所市场号上）。
像 `sh000001` 这种普通指数代码要显式给 `--index`——这条规则与 native 侧一致。

### 交叉印证（五方一致）

`output/kline_crosscheck_evidence.txt`：

- **日线 vs `.img`（sz000623 20260612）**：日线 `open 16.790 / high 17.490 / low 16.730 /
  close 17.390`，`.img` 的 `05/06/07/08` 逐项相同；`volume 16826060` vs `.img` 的
  `10 = 16826061`（差 1 股）、`amount 290028480` vs `1A = 290028487.9`（差 8 元），
  都是 wire 浮点精度量级。
- **1m vs `.zsm` 分钟槽**：20260612 有 **240 根 1m bar**，`.zsm` 有 **240 个槽**；
  逐行 `1m close == zsm A`、`1m volume == zsm C × 100`，6/6 行精确。
- **1m vs 成交明细**：20260921 的 15:00 bar `close 17.750 / volume 109700 股`，成交明细
  同一分钟 `price 17.75 / volume 1097 手 / status 2`，完全相等。
- 分钟 240 根求和 `16826100` 与 `.zsm` 240 槽求和 `168261 手` 完全相等；与日线的
  `16826060` 差 40 股（2.4e-6），是两条聚合路径的舍入残差，不是解码错误。

### 顺带闭合：`.zsm` 的 A 字段

上一轮把 `.zsm` 槽位里的 A 标为「未知的分钟统计量」。现在不用猜了：**A 就是分钟收盘价**，
C 是该分钟成交量（手），B 是累计均价。K 线的 1m bar 与 `.zsm` 槽位一一对应。

## 分时：0x0537（当日）

**这一块不是移植，是原始逆向。** `0x0537`/`0x0FB4` 在
`doc/02-engine/07-useful-live-features.md` 里被记为「已确认」，但**全树没有实现**，
也没有留下当时的探测脚本。所以请求与应答格式是从活体服务器复原的，过程与 1173
字节应答原文见 `output/timeline_probe_evidence.txt`。

```
请求 12 字节
  u8 市场, u8 0, 代码[6], [8..11] 服务端不解释
应答
  u16 点数（一个交易日 240 点）
  u16 保留（观测为 0）
  每点三个 varint：
    价格偏移   第 0 点放本段基点（1/100 元）；之后每点是【相对该基点的偏移】
    均价偏移   同形，1/10000 元
    成交量     该分钟成交量（手）
  所以 price[i] = (基点 + 偏移[i]) / 100，第 0 点的偏移读作 0
```

三个要点：

1. **`a[i]` 不是逐点增量，是相对第 0 点基点的偏移。** 第一版按增量读，价格从 17.60
   一路漂到 30.35；改成偏移后 **240/240 精确**。注释里专门写了这个坑。
2. **后 4 字节服务端不解释**：发 `00 00 00 00` 和发 `04 27 35 01`（一个日期）返回
   **逐字节相同**的应答。所以没有分页、没有起始偏移，一次就是整段。
3. **应答不带时间**，只有顺序；点位标签是 `09:31..11:30` 然后 `13:01..15:00`
   （前 120 点上午，跳过午休），与 0x052D 的 1m 标签一致。

### 对账（240/240）

`output/timeline_crosscheck_evidence.txt`：

| 对照 | 结果 |
|---|---|
| 分时 `price` vs 同日 1m K 线 `close` | **240/240 精确相等** |
| 分时 `volume_hand` vs 同日 1m `volume/100` | **240/240 精确相等** |
| 分时量合计 | 87389 手 = 日线 bar 的 8738903 股/100 = 成交明细 ≤15:00 的合计 |
| 分时末日 `average_price` | 17.65500 vs 日线 VWAP 17.65498 |

这同时给了 0x052D 的 1m 分页一个**独立验证**：两条互不依赖的命令给出同一串分钟量。

`0x0FB4`（指定日期分时）的**请求格式仍未复原**。这一轮把范围收窄了，过程全部记在
`output/history_timeline_recovery_log.txt`，要点：

- **请求体长度是关键区分**：12 字节（照 `0x0537` 的形状再加一个 `u32 日期`）服务端**完全不回**
  （裸读 `recv` 直接失败）；**11 字节**（去掉 `0x0537` 的那个保留字节）**会被正常应答**——
  header `command=0x0FB4`、`wire=decoded=2`、体 `00 00`。同一次会话里 `0x0537` 用 12 字节体
  返回 `wire=900/decoded=1173` + zlib，所以这不是我的读法问题，是服务端的差异行为。
- **应答头线索**：普通股票回 2 字节 `00 00`，板块指数 `880471` 回 **6 字节**
  `00 00 00 00 00 00` —— 多出的 4 字节与 `0x0FC6` 的应答头一致（`u16 条数 + f32 价格基`）。
  成功时的应答很可能与 `0x0FC6` 同头。
- **那 4 个字节不是 YYYYMMDD 日期**：从 20260921 往回 24 个交易日（含当天）全试了，
  再加 LE/BE、`u16 年+月+日`、两位年、全零、LC1 日期字，**全部 0 点**。
  起初的"服务端只留近期窗口"假设（文档那次是 2026-07-31 查 2026-07-30）被"当天也是 0 点"推翻。
- 已排除的假设与下一步值得试的四条（相对天数偏移 / 选择器 / 字段顺序 / **直接读 TdxW 的调用点**）
  都写在日志里。最后一条最有可能一次到位。

`timeline --date` 会明确报「未复原」，不会发一个猜测的请求再把空序列当成功。要复盘历史某天的
分时，现在用 `kline --period 1m`（已验证与 `.zsm` 逐分钟吻合）。

## 集合竞价序列：0x056A

这是**别处都没有的数据**：快照和 K 线只给你竞价的结果，只有这条命令给竞价**过程**——
每一笔虚拟撮合价、已撮合量和买卖未匹配的倾斜。

```
请求 28 字节
  u8 市场, u8 0, 代码[6]
  u32 0           常数
  u32 selector    0 = 只开盘；非 0 = 开盘 + 收盘
  u32 0           常数
  u32 start
  u32 limit       1..5000
应答
  u16 条数，随后每条 16 字节定长记录
    [0..1]   u16 日内分钟数
    [2..5]   f32 虚拟撮合价
    [6..9]   u32 已撮合量（手）
    [10..13] i32 未匹配量，**带符号**：正=买方剩，负=卖方剩
    [14]     保留字节，4 份样本里恒为 0
    [15]     u8 秒
```

应答长度必须**恰好** `2 + 条数 × 16`，否则整条拒绝——半读一条竞价序列比报错更糟。

### 实测（sz000623，20260921）

| | selector=0 | selector=1 |
|---|---|---|
| 点数 | 42（只开盘） | 61（开盘 42 + 收盘 19） |
| 区间 | 09:15:00 – 09:24:57 | 收盘段 14:57:09 – 14:59:51 |

- 开盘段虚拟价区间 `[17.50, 17.59]`，**含当日实际开盘 17.55**；收盘段末虚拟价 17.74，
  当日收盘 17.75（差 1 分，撮合发生在 15:00:00）。
- 两段都**止于撮合前 3 秒**：这条命令给的是"竞价过程中的虚拟价"，最终撮合价不在序列里。
- 已撮合量会出现**下修**（本例开盘段 1 次），这是竞价中的正常现象。汇总里如实报
  `matched_volume_monotonic_violations` 而不是抹平它。
- 未匹配量的符号就是买卖倾斜，会翻转（开盘 3 次、收盘 4 次）；`max_unmatched_volume_hand`
  与出现时刻一起给出。

证据：`output/auction_probe_evidence.txt`（两份应答原文）、
`output/auction_crosscheck_evidence.txt`。

### 顺带修掉的一个宏陷阱

`APPEND_LITERAL` 用 `sizeof` 取长度，我一度给它传了三元表达式
`has_points ? "true" : "false"`——表达式退化成指针，于是拷了 `sizeof(char*) - 1` 字节，
JSON 里出现 `true\0fa`（`fa` 是 `false` 的尾巴）。这种错误在按 `len` 逐字节扫的调试里
看不出来，只有按 C 字符串读才会暴露。现在 5 个 JSON 模块的宏上方都写了"只传字面量"，
并且 `braces_balanced` 这类检查会把它拦住。

## 全量快照：0x054C

```
请求 10 + 7×N 字节
  [0] 5        固定标记
  [1..7] 0
  [8..9] u16 条数
  每只： u8 市场 + 代码[6]
应答
  [0..1] 本实现不解释（原样报出）
  [2..3] u16 条数
  [4..]  记录，首尾相接，没有长度前缀
```

**记录没有长度前缀**，边界靠扫描恢复：某个字节是市场号（0..2）且其后 6 字节都是 ASCII 数字。
这个扫描理论上会在记录的载荷里撞到假边界，所以只在"恰好得到声明的条数**且第一条在偏移 0**"
时才接受；否则报错，而不是给出一份悄悄切错的序列。记录本身与 0x0547 同形（少五档），
价格同样是增量+刻度，所以直接复用了项目里的 varint、wire 浮点和 `tdx_price_divisor`。

### 与 0x0547 逐字段对账（同一批证券、两次独立往返）

| 字段 | 结果 |
|---|---|
| last / previous / open / high / low | **一致** |
| amount / total_hand / current_hand | **一致** |
| inside / outside / imbalance | **一致** |
| `open_amount` | **差**，见下 |
| 未建模尾部字节数 | 0x0547 = 46，0x054C = 61–62 |

`open_amount` 的差异**不是解码错误**：native 自己在两条路径上就用了不同的刻度——0x0547 路径
`×10`（`market_protocol.cpp:289`），0x054C 路径 `×100`（同文件 `:163`）。实测 0x054C 的原始值
是 0x0547 原始值的 1/10 量级（19358 对 193577），即这条命令该字段的分辨率粗 10 倍。

**两条命令都留下大量未建模字节**（0x0547 46 字节 / 0x054C 61 字节），本实现按 c/ 的既有约定
**只报数量、不编语义**（`tail_bytes` / `tdx_depth.tail_size`）。这是一个明确记录的缺口。

### 顺带修正的一个判断

我原先以为 0x054C 是"更省的 0x0547"，**实测不是**：

| | 记录大小 | 每请求上限 | 全市场 A 股（5574 只） |
|---|---:|---:|---|
| `0x0547`（`sweep`） | 117.7 B | 100 | 56 请求 / 6 连接 / **0.44 s** |
| `0x054C`（`snapshot`） | 103.0 B | **80** | 70 请求 / 单连接 / 4.78 s |

记录只小约 12%，而服务端对这个命令的批上限是 **80**（请求 100 只、服务端只回 80 —— 这是活体
实测，也正好解释了 `tdx_quote.h` 里那个一直没人用的 `TDX_SNAPSHOT_BATCH_MAX 80` 是对的）。
所以全市场轮询仍然该用 `sweep`；`snapshot` 的价值在于**协议覆盖**、更小的记录，以及
"只要 L1 标量、不要五档"的调用方不必解析阶梯。`snapshot` 是单连接的，与 `sweep` 的多连接
池不是同一件事，这个对比不是同口径，上面的数字都标了连接数。

## 公开基础数据：0x0010 财务

行情之外的第一个数据族，也是形状最省事的一个：**一次请求带一整串证券，应答是定长记录数组**——
没有分页、没有增量、没有五档，完整性问题就只剩长度一条，所以解析器严格判 `2 + 条数 × 143`。

```
请求 2 + 7×N 字节
  u16 条数
  每只： u8 市场 + 代码[6]
应答
  u16 条数，随后 count × 143 字节
    [0]     u8 市场
    [1..6]  代码[6]
    [7..142] 136 字节信息块 = 34 个四字节小端槽
```

信息块是混合宽度，槽序号本身不够用：`+0` 是 f32 流通股本（单位万股），`+4` 是两个 u16
（省/行业），`+8` 是两个 u32（更新日/上市日，YYYYMMDD），`+16` 起是 30 个 f32。
**两种刻度是这里最容易搞错的地方**：股本是 1 万倍、金额是 1 千倍，所以模块在出口就归一化，
不把原始 float 交出去——否则调用方拿"万股"去乘价格会差 10⁴。

### 验证到什么程度（全市场 5574 只 A 股一次遍历）

| 检验 | 结果 |
|---|---|
| `province_id` / `industry_id` 非零 | **5574/5574** |
| 上市日期存在 | 5564/5574（缺 10 条=新股/退市） |
| 上市日期与公开事实对照（抽查 6 家） | **6/6 正确**（浦发 1999-11-10、平安 1991-04-03、工行 2006-10-27、万科A 1991-01-29、茅台 2001-08-27、宁德 2018-06-11） |
| 股本刻度（茅台 12.5 亿股、工行 867.9 亿 H 股） | **精确命中** |
| `每股净资产 × 总股本 ≈ 净资产` | **91.8% 成立**（5107/5564；例外是银行与负净资产，属会计口径） |

若有槽位错位或刻度差一个 10 的幂，不可能九成以上精确吻合。证据：
`output/finance_verification_evidence.txt`。

### 明确【未】验证的部分（不做断言）

`national` / `promoter_legal_person` / `legal_person` 这三个股本类别槽位**站不住**：

- 工商银行 `legal_person` 解出 4,270,920,000,000 股，是总股本 356,406,240,000 的 **12 倍**；
- 贵州茅台解出 893,893,520,000 股，是总股本 1,250,081,562 的 **715 倍**；
- 三家大行的 `national` 都是 1966 万量级——不像股本。

本移植**逐字段照搬 C++ 参考实现的绑定与刻度**（参考实现就是基准，擅自改绑只会掩盖问题），
测试里因此不对这三个字段做任何断言。同一条记录里的 `circulating`/`total`/`b_share`/`h_share`
都是对的，所以不是整段错位，而是这两三个槽位的语义/刻度尚未标定。

除上表列出的字段外，资产负债表与利润表的其余槽位（应收、存货、资本公积等）**尚无独立校验**，只是按参考实现的绑定原样输出；它们具体的量纲标定留待后续。

`reserved_2_raw` 保持原值：**5574 条里只有两个取值（6 和 9）**，说明它是标记位而不是个股数值
——C++ 参考把它留名为 `reserved_2` 是对的。

### 一个工程发现：长遍历会遇到服务端超时

单连接、每批 100 只、连发时，服务端会在第 49 批左右超时（实测 4800/5574 后失败）。
命令因此对**传输失败**做每批最多 3 次重试（新连接），解码失败不重试——那是 bug，重试只会掩盖
错误信息。加重试后全市场 5574/5574、4.1 秒跑完。

## 公开基础数据：0x000F 股本变迁与除权

和 `0x0010` 共用同一个请求形状，但**应答头只点名一只证券**，所以这条命令是"一只一请求"：

```
请求 9 字节： u16 条数(=1) + u8 市场 + 代码[6]
应答 11 字节头： u16 块数, u8 市场, 代码[6], u16 条数
      随后 count × 29 字节
        [0]      u8 市场
        [1..6]   代码[6]
        [7]      u8 保留
        [8..11]  u32 日期 YYYYMMDD
        [12]     u8 类别
        [13..28] 四个 u32
```

因为头里只有一只证券，多只就没法归属，所以模块只收一只，并**校验回显的市场/代码与请求一致**——
回错票的应答无法归因，宁可报错。长度严格判 `11 + 条数 × 29`。

### 类别与口径

| 类别 | 含义 | 四个槽怎么读 |
|---|---|---|
| 1 | 除权除息 | 每 10 股派息 / 配股价 / 每 10 股送转 / 每 10 股配股 |
| 2 | 送配股上市 | |
| 3 | 非流通股上市 | |
| 4 | 国家股配售 | |
| 5 | 股本变化 | 非事件类，槽是**变更前流通 / 变更前总 / 变更后流通 / 变更后总**（×10000 为股） |
| 6 | 增发新股 | |
| 7 | 股份回购 | |
| 8 | 增发新股上市 | |
| 9 | 转配股上市 | |
| 10 | 可转债上市 | |
| 11 | 扩缩股 | float[2] = 缩股比例 |
| 12 | 非流通股缩股 | float[2] = 缩股比例 |
| 13 | 送认购权证 | float[0] = 行权价，float[2] = 权证份数 |
| 14 | 送认沽权证 | float[0] = 行权价，float[2] = 权证份数 |
| 15 | 重整调整 | 四个 float，无命名 |

**类别 1 是"每 10 股"**，不是每股——把首字段当每股用会把现金红利放大十倍，所以 JSON 里
同时给出 `dividend_per_10_shares_yuan` 和派生出的 `dividend_per_share_yuan`。C 侧对外只给
ASCII key（如 `ex_rights_dividend`）+ 数字类别，中文名留在本表里，避免把中文写进编译单元。

### 验证到什么程度

| 检验 | 结果 |
|---|---|
| 平安银行 2024-06-14 那条 = **10 派 7.19** | **精确复现**（参考实现记录的活体事实） |
| 应答长度 = 11 + 81 × 29 | **逐字节吻合** |
| 股本链：第 N 条 `after` == 第 N+1 条 `before` | **26/26 对全部精确相符**（1991 年上市 2650 万股 → 2007 年 22.93 亿股） |
| float32 视图 vs wire 视图 | **324/324 槽全部吻合（<0.01%）** |
| 200 只 A 股遍历 | 11,213 条 / 4.6 秒，**15 个类别里出现 13 个** |

第三条钉住了槽的次序与 ×10000 刻度，第一条钉住了类别 1 的口径。

**一个澄清**：参考实现对每个槽同时按 float32 和按 wire 数解码，看起来像"一个槽装两种量"。
实测 324/324 槽两种读法给出同一个数（浮点 7.19 对 wire 71900/10000），所以**槽就是一个
float32**，wire 读法是同一个数的第二种解码路径，而不是另一个字段。本移植仍同时保留两种视图
（忠于参考实现），但 JSON 里两者并列，让调用方自己看到它们一致。

证据：`output/capital_verification_evidence.txt`。

## 本地加密 GBBQ 权息文件

`0x000F` 的离线替代：TdxW 把全市场公司行为历史放在本地
`<root>\T0002\hq_cache\gbbq`，内容是同一批事件。**这是本项目里唯一能拿两个完全独立
的数据源互相 diff 的地方**，所以值得为它做那个密码。

```
4 字节头： u32 条数
条数 × 29 字节：前 24 字节加密，后 5 字节明文
           记录布局与 0x000F 完全一致
```

实测该文件 5,607,734 字节，`(5607734 - 4) / 29 = 193370` 整除。解析器要求
`size == 4 + 条数 × 29` 精确成立（否则拒绝），因为"尽力读"在这里只会读出垃圾。

### 密码：16 轮 Feistel + 4168 字节状态表

参考实现把状态表以 base64 内嵌；本移植用 `output/make_gbbq_cipher_state.py` **机械提取**，
没有手抄一个字符（5560 字符里错一个，整份数据的解码都会错而没有别的症状）。四张 256 字表
恰好平铺 `[0x48, 0x1048)`：

```
0x48 + 256*4 = 0x448   0x448 + 256*4 = 0x848
0x848 + 256*4 = 0xC48  0xC48 + 256*4 = 0x1048 = 表长
```

这个平铺关系本身就是"表偏移写对了"的判据，测试里逐条断言。

### 决定性验证：本地文件 vs 网络 0x000F

两条路径没有任何共享代码——一个是服务端应答，一个是 5.6 MB 本地加密缓存。对同一只票
逐条比对（键为日期+类别，值为四个 float 与四个 share）：

| 证券 | 文件内条数 | 网络条数 | 精确一致 | 值不同 | 仅网络 | 仅本地 |
|---|---:|---:|---:|---:|---:|---:|
| sz000001 | 81 | 81 | **81** | 0 | 0 | 0 |
| sh600000 | 88 | 88 | **88** | 0 | 0 | 0 |
| sz000002 | 112 | 112 | **112** | 0 | 0 | 0 |

**281 条记录，零差异。** 更强的一步：把两个来源都渲染成 JSONL 再逐行比对，
sz000001 的 **81 行逐字节完全相同**（只差汇总里的 source 字段）。这同时证明了 base64 提取、
base64 解码、Feistel 解密、记录布局四件事。

`0x000F` 与本地文件共用同一个记录解析器，所以两种来源的输出可以直接 diff——上面做的就是这件事。

### 一个必须说清的限制

**这个容器没有认证码**（没有 MAC、没有摘要，只有长度校验和逐条形状校验）。所以
"改一个密文字节一定被拒绝"是**不成立**的：改动可能解出另一个仍然合法的记录。测试因此断言的是
**损坏永不隐形**——24 个加密字节逐个翻转，要么解析被拒绝，要么解出的值必然与原件不同。
这也说明该文件必须来自可信本地来源（它本来就是 TdxW 自己的缓存），不能当作有完整性保护的
传输通道使用。

证据：`output/gbbq_verification_evidence.txt`。

## 特殊涨跌停表：0x0452

```
请求 14 字节： u16 起始下标 + 12 个零
应答 2 字节头： u16 条数
      条数 × 13 字节：
        [0]     市场
        [1..4]  代码【数字】，显示时补零到 6 位
        [5..8]  f32 涨停价
        [9..12] f32 跌停价
```

**这个命令每次只回一行。** 从下标 0 请求，服务端只回 1 条（实测 15 字节 = 2 + 1×13）
——`start_index` 是"表的第几行"，不是"从这里开始最多给 N 行"，客户端必须按下标逐行推进，
**只有空页才是结束**。我最初把"不足一页"当成结束，结果走完第一行就停了；实测纠正后才走完整表。

`limits` 是本项目最"话多"的命令：782 行 = 782 次往返。

### 它是什么（这里我改过一次判断）

第一版判断是"特别处理 = ST = ±5%"。走完整表后，band（由 `(涨停-跌停)/(涨停+跌停)` 得出，
对称时即为 band 本身）的分布是：

| band | 行数 | 说明 |
|---:|---:|---|
| 0% | 1 | 涨停 == 跌停 |
| 5% | 1 | **ST 股确实在名单里**，只是全表只有一条 |
| 10% | 144 | 主板上需要显式给出的情形 |
| 10.5% | 4 | 两位数取整的边界 |
| **20%** | **626** | 创业板 / 科创板 |
| 20.5% | 1 | 同上，取整边界 |
| 100% | 5 | 新股首日等无涨跌幅限制 |

所以这张表是**各类非标准 band 的覆盖层**（参考实现注册表原话：
「特殊涨跌停表……作为普通交易规则的覆盖层」）。本条验证的是"字段就是涨跌停价"；
**没有**验证"某只票为什么在名单里"——字段本身看不出原因，不猜。

### 验证

| 检验 | 结果 |
|---|---|
| `(涨停 + 跌停) / 2 == 昨收`（0x054C 独立给出昨收） | 13 条连续记录：**11 条完全相等**，2 条差 +0.0050（奇数分位取整） |
| 隐含 band 落在 9.87%–10.16%（即 ±10% 取整后的散布） | 该抽样全部吻合 |
| 全表 782 行、市场分布 | 深 384 / 沪 394 / 北 4 |
| `涨停 == 跌停` 的行 | 实测存在 1 条 → 解析器**不能**要求严格大于，否则真实数据会被误判为损坏 |
| `涨停 < 跌停` 的行 | 0 条（这种一定说明解码错位，解析器直接报错拒绝） |

证据：`output/limits_verification_evidence.txt`。

## JSN 资源：GBK 编码的 JSON 表格

`bi/list/*.jsn` 这类资源是**债券参考名单**（按评级、利率类型、品种分类）。它们不是行情，
是"名单 + 属性"，而且格式与前面所有命令都不同——**它是 GBK 编码的 JSON**：

```json
[ { "colheader": ["$ZQDM", "ZSC", ...],
    "data": [ ["020820", "1", ...], ... ] } ]
```

根是数组，每项有 `colheader`（列名数组）与 `data`（行数组），**每行宽度必须等于列数**
——这是该格式唯一真正的约束。一行短了，它之后的每一格都会被贴上错误的列名，比直接报错更糟，
所以本实现拒绝它，报错时说明原因。

子节点用**兄弟链表**而不是连续区间：成员本身是容器时，它会把自己的子节点插在中间，
"区间"就会指到孙节点上。JSN 的形状（对象的成员是"数组的数组"）恰好触发这个 bug——
扁平数组时完全看不出来，是单元测试抓出来的。

### 分三层，各自可单独验证

| 层 | 文件 | 说明 |
|---|---|---|
| JSON | `tdx_json.c` | 有界解析器：转义、surrogate 对、64 层深度上限、拒绝尾随数据与前导零 |
| 表格 | `tdx_jsn.c` | 组/行/列展平 + 宽度不变式 |
| 编码 | `tdx_jsn.c` | GBK → UTF-8 走**系统代码页**（936），不内嵌码表 |

GBK 用 `MultiByteToWideChar(936)` + `WideCharToMultiByte(CP_UTF8)`：映射精确，且不需要
一张约 100 KB 的码表。非 Windows 平台会明确报"需要平台代码页"，而不是猜一个转换。

### 实测

| 检验 | 结果 |
|---|---|
| 小资源 `bi/list/zq_tx201.jsn`（贴现债券） | 6,969 字节 GBK → **7,221 字节 UTF-8**，1 组 42 行 20 列，MD5 校验通过 |
| 中文列值 | **正确**（证券简称"26贴债39"、利率类型"贴现"、债券类型"国债"） |
| 大资源 `bi/list/zq_zqqb201.jsn`（全市场债券） | **40,728,611 字节** / 1358 分块 / **42,957 行** / 约 73 秒，MD5 校验通过 |

GBK→UTF-8 的长度变化本身就是证据：中文字符两字节变三字节，所以 UTF-8 一定更长；
若只是原样透传，长度会相等。而中文能正确显示，说明 GBK 转换与 JSON 解析**两层都对**——
任何一层错，中文就会是乱码。

### 限制（诚实说明）

- 本层**不解释列的含义**。每列是什么属于调用方的领域知识；这一层只保证"某格就是它表头
  所说的那一格"。参考实现里债券目录、可转债、权息等各自的字段映射是另一件事，本次没有移植。
- 磁盘上**没有本地 JSN 缓存**，所以这一项没有"本地 vs 网络"的对账（GBBQ 那一项有）。
  验证依据是结构、内容合理性与端到端一致性。
- 40 MB 的资源要 73 秒，瓶颈在传输（1358 个分块）而非解析；若需频繁读全市场债券名单，
  应当缓存而不是每次重取。

证据：`output/jsn_verification_evidence.txt`。

## JSN 之上的债券字段映射（`jsn --bonds`）

JSN 层保证"某格就是它表头所说的那一格"；这一层是**领域知识**：哪一列是哪个字段、市场怎么写，
以及最微妙的一点——**发行规模那一列到底是什么单位**。

### 单位取决于来自哪个资源（这是本层存在的理由）

同一列 `GM`，三个资源读出三种含义，都是活体实测：

| 资源 | scale | `GM` 原始值 | 换算 |
|---|---|---:|---|
| `list/zqgz201.jsn`（客户端合并表） | `client-master-hidden-unit` | 56,000,000,000 | **不换算**（单位不可复原） |
| `list/zq_gz201_1.jsn`（沪市投影） | `outstanding-balance-100m-yuan` | 260 | **260 亿元**（存量） |
| `list/zq_jrz201_1.jsn`（政策性金融债） | `issue-size-100m-yuan` | 100 | **100 亿元**（发行量） |

所以只报一个 "size" 数字，在三种情况里至少两种是错的。归一化后的行因此同时给出：
**原始值**、**读出它时采用的语义**、以及**只有该语义才支持的那个换算字段**；
单位不可复原时两个换算字段都留空，不猜。

### profile 表：20 条，逐条来自参考实现

- **7 个客户端合并表**（`reference_master = true`）：`zqjrz201` → `issue-100m-yuan`，
  其余 6 个 → `client-master-hidden-unit`
- **12 个交易所投影**（`_1` 沪 / `_2` 深）：`zq_jrz201_1/2` → `issue-100m-yuan`，
  其余 → `outstanding-100m-yuan`
- 表外资源：默认 `issue-yuan`（与参考实现一致）

**合并表会交换身份列**：客户端合并表用自己的 `$ZQDM1`/`$SC1` 作身份、把 `$ZQDM`
报为 `client_instrument_id`；投影表相反。映射按 profile 走，不假定一种顺序。

### 票息表

`FXRQXL` 与 `FXLLXL` 是逗号分隔的列表，按下标配对。利率归一化（照参考实现）：
**`|值| ≤ 1` 视为分数，乘 100 变百分比**；否则视为已是百分比。例：`0.035` → 3.5%、
`3.5` → 3.5%。利率比日期少时，多出的日期保留但利率为空（不是填错值）。
调度表长于调用方缓冲区时**拒绝**——截断会静默丢掉票息。

### 验证与限制（诚实说明）

仓库里只有贴现名单这一份债券资源抓包，所以分工是：

| 覆盖 | 依据 |
|---|---|
| 默认 profile、身份取自 `$ZQDM`/`$SC`、字段映射、渲染 | **真实抓包**（`list/zq_tx201.jsn`，42 行，中文精确比对） |
| 合并表的身份交换、投影的标的关系、三种单位语义、票息表 | **合成文档**（测试里明确标注 `SYNTHETIC`） |
| 规模端到端 | **全市场债券 42,957 行**，约 70 秒 |

两个被测试抓出的错误都在测试侧：我把含 UTF-8 中文的合成 JSON 送进了 GBK 解码器
（只有线上载荷是 GBK），解码器正确地拒绝了它，而测试随后又无条件访问 `groups[0]`
于是段错误而不是报错；以及归一化行的文本字段是**零拷贝**指向 JSN 文档 arena 的，
我第一版却经过 `tdx_buf` 取文本再 free 缓冲区——那是悬空指针，因为缓冲区持有的是副本
（为此给 JSN 层加了真正的零拷贝视图 `tdx_jsn_cell_view`）。

证据：`output/bonds_verification_evidence.txt`。

## 可转债概览（`jsn --convertible`）

`bi/list/kzz_kzzsy201_1.jsn`：**314 行 × 55 列 / 200 KB**（活体实测），首行是南方航空转债。

### 这一层【只是概览】——范围声明

参考实现的可转债视图是**六份文档的连接**：概览 / 进度 / 票息 / 回售 / 赎回 / 下修，
按债券的市场+代码做键，外加最多两份换股投影，再挂三份触发条款文档。

本模块**只做概览**：单份资源、没有连接语义、可以独立端到端验证，而字段知识
（二十来个条款、评级、日期、比例）就在这一份里。**连接、触发条款、票息利率数组、
投影优先级都没有移植**——汇总里用 `documents_joined:1` 和 `join_note` 明说，
而不是留给调用方去发现。

### 字段

| 组 | 字段 |
|---|---|
| 身份 | `bond.{market,security_id,code,name}`；`instrument_type`（**代码以 132 开头 = 可交换债**，否则可转债）；`underlying.*` |
| 条款 | 面值、发行价、转股价、发行规模（亿）、剩余规模（亿）、剩余比例、剩余年限、到期赎回价、回售触发比例、赎回触发比例 |
| 日期 | 上市、发行、转股起、转股止、到期 |
| 评级/状态 | 债项评级、主体评级、当前状态 |
| 派生 | `core_terms_complete`——参考实现自己的完整性判据：**面值、转股价、到期日三者齐备** |

### 一个与参考实现矛盾的列：`ZGDM`

参考实现把 `ZGDM` 绑成**"正股名称"**（`security_document` 的第三个参数是 fallback name）。
全量 314 行实测：

| 内容 | 行数 |
|---|---:|
| 六位数字（代码） | **217** |
| 空 | 97 |
| 其他（含名称） | **0** |

样本：`190075, 190076, 190077, 190081, 190084`。**它从来不是名称。**

本模块因此把它放在 `underlying.reference_code` 名下（它实际装的东西），并在测试里对抓包行
断言"要么空、要么六位数字"。这与财务那三个股本类别槽位是同一类处理：照抄参考实现的绑定
会给出误导性标签，所以**改标签、留实测、写清楚**。

### 验证

| 检验 | 结果 |
|---|---|
| 314 行全部映射并渲染，**0 行不可解析** | ✅ |
| `core_terms_complete` | 314/314 |
| 有标的 | 314/314 |
| 抓包 fixture（55 列 + 前 3 行，逐值真实） | 字段逐个断言，中文精确比对 |

另有一个诚实的说明：参考实现在概览资源里读 `LLZH`（未付利息合计），但**该列不在本资源中**，
实测全部为空（不是 0）——这正是 `has_*` 标志存在的意义。

证据：`output/convertible_verification_evidence.txt`。

## 可转债六文档连接（`convertible`）

参考实现的完整视图：**六份文档按"市场:代码"连接**，再挂三份触发条款。

| 文档 | 资源 | 实测规模 |
|---|---|---|
| 概览 | `list/kzz_kzzsy201_1.jsn` | 200,764 字节 / 314 行 × 55 列 |
| 进度 | `list/func_kzz_tkjd201.jsn` | 97,764 字节 / 316 行 × 21 列 |
| 票息 | `list/func_kzz_lltk201.jsn` | 34,046 字节 / 316 行 × 13 列 |
| 回售 | `list/func_kzz_hstk201.jsn` | 51,745 字节 / 320 行 × 20 列 |
| 赎回 | `list/func_kzz_shtk201.jsn` | 45,021 字节 / 320 行 × 19 列 |
| 下修 | `list/func_kzz_xztk201.jsn` | 72,313 字节 / 320 行 × 22 列 |

一次取回并连接，约 1.6 秒。另外两份文档做的事**不一样**，必须分清：

| 文档 | 资源 | 规模 | 行为 |
|---|---|---|---|
| 可交换债 | `list/kjhz_kjhzsy201_1.jsn` | 1,449 字节 / 2 行 × 43 列 | **替换**它点名债券的概览行 |
| 换股投影 | `list/func_kzz103_1.jsn` | 375 字节 / 2 行 × 15 列 | **兜底**：只填参考实现允许的 10 个字段，且只在主源为空时填，**绝不覆盖** |

把两者当成同一种东西处理，会产出与参考实现不一致、但看起来合理的结果。实测全市场换股债
2 只、投影覆盖 2 只、投影实际只填了 **3 个字段**。

测试用一个能**同时判出两个方向**的用例：SH132024 的 `SYNX` 在换股文档是 `4.547945`、
在投影是 `4.548`（精度不同）→ 结果必须是 4.547945，证明主源没被覆盖；而它的
`DQSHJ=105` 与 `LLZH=0.04` **只存在于投影** → 必须被填上，证明兜底生效。
SH132026 的 `LLZH` 在投影里是**空** → 必须是"缺失"而不是 0。

副产物：`core_terms_complete` 从 314 升到 **316** —— 那两只换股债原本在概览里条款不全，
由换股文档补齐后达标。

### 键并集是关键设计

**实测 314 只债券在六份文档里全都有**，另有 6 只只在非概览文档里出现。连接因此取
**六份文档键的并集**（本次 320 只），而不是按概览遍历——只按概览会漏掉那 6 只，
而它们确实有进度/回售/赎回/下修数据。那 6 只的概览字段如实报缺失，
`core_terms_complete` 为 false（314/320 为 true），`sources` 块则说明每只债券
**究竟是哪几份文档提供的**。

### 跨文档印证（本层最有价值的验证）

概览的 `FXLLXL` 与票息文档的 `PMLL_1..6` 描述**同一份票息表**，但两者单位不同：
概览写分数，票息文档写百分比。312 个可比行实测：

| 关系 | 行数 |
|---|---:|
| 前 n-1 期：`payment_rates[i] × 100 == rates_pct[i]` | 312/312 |
| 尾期：`payment_rates[-1] × 100 == rates_pct[-1] + compensation_rate_pct` | **311** |
| 尾期就等于 `rates_pct[-1]`（未并入补偿） | **0** |

样本（天能转债）：`[0.4, 0.6, 1.0, 1.6, 2.5, 15.0]` 对 `[0.4, 0.6, 1.0, 1.6, 2.5, 3.0]`，补偿 12 —— **15 = 3 + 12**。

这说明两件事：**两份文档互相印证**（不是各自自洽），以及**尾期已并入补偿利率**，
所以只看某一列会低估最后一年的现金流。参考实现把两列原样并列，本移植照做并在此点明。

另外 314/314 行满足 `payment_dates 条数 == term_years`。

### 测试里最锋利的一条

三组触发条款来自三份不同文档，所以**同一只债券的三个触发条件必须互不相同**：
SH110076 的回售是 `30/30`/70%、赎回是 `15/30`/130%、下修是 `15/30`/80%，
起始日分别是 20241102 / 20210506 / 20201102。

**把一份文档读三遍的实现会产出三个完全相同的触发条款，而且看起来仍然合理**——
这条断言就是为它准备的。

同一批测试还抓出两个真 bug：把概览行替换成换股文档的行之后，`normalize` 仍然传
`documents->overview`，于是**按错误的文档读正确的行号**，读出了另一只债券的条款
（132024 读出了 110075 的转股价 6.17）；以及 `from_overview` 在替换之后才算，
把换股债误报成"概览提供了它"。

### 未移植

- 参考实现的**待发**（`list/dfkzz201_1.jsn` 等）、**申购**（`list/func_kkzss101_1.jsn`）、
  **定价**（`list/gxjty_zq_kzzsy101_1.jsn`）三个视图。

证据：`output/convertible_join_evidence.txt`。

## 服务端路由

| 路由 | 说明 |
|---|---|
| `GET /` | 用法页 |
| `GET /health` | 存活检查 |
| `GET /status` | Hub 计数，见下 |
| `GET /api/v1/market/snapshot?codes=a,b` | 当前存量值，一次性 |
| `GET /api/v1/market/stream[?codes=a,b]` | **SSE**；省略 `codes` 即全 universe |

SSE 参数：`max_events=N`（取够即断）、`wait_timeout_ms=N`（空闲多久断开）。

事件类型：

```json
{"type":"snapshot","sequence":1,"security_id":"SZ000001","changed":["new"],"updates":2,"record":{...}}
{"type":"change","sequence":2,"security_id":"SZ000001","changed":["amount","volume","dishes","book","status"],"record":{...}}
{"type":"heartbeat","sequence":3,"subscribed":5574,"total_events":1128}
```

迟到订阅者**立即收到已订阅证券的快照**（`replay`），之后只收变化。

`/status` 关键字段：`universe` / `polled`（本轮实际轮询数）/ `rounds` /
`failed_rounds` / `records` / `events` / `last_round_ms` / `interval_ms` /
`effective_interval_ms` / `tier_warm_ms` / `tier_cold_ms` / `demote_rounds` /
`tier_hot` / `tier_warm` / `tier_cold` / `subscribers` / `queued` / `dropped` /
`last_error`。

## 调度：裁剪 + 分层节拍

轮询线程不盲扫 universe。每轮它构造"**被需要**且**自己那一档到点**"的下标集合，
把这份清单交给 fetcher，只对这些证券发请求。

**裁剪**：没有任何订阅者要的证券完全不轮询。零订阅者时保留全 universe 让
`/snapshot` 仍有数据。实测：2,000 只的 universe，挂一个只订 1 只票的 SSE 读者，

```
无订阅者：universe=2000  polled=2000  tiers h/w/c=2000/0/0
1 只订阅：universe=2000  polled=1     effective=1000ms
```

上游请求数随之从 20 批降到 1 批。**这是省上游的主要手段**——全市场订阅时
每只票都在变（实测盘中平均 59% 的票每秒都有变化），分层救不了你，只有裁剪能。

**分层节拍**：每只票维护 `tier / quiet / polled`。

| 档位 | 默认来源 | 含义 |
|---|---|---|
| hot | `--interval-ms` | 最快档 |
| warm | `--tier-warm-ms`，0 = 自动 `3 × hot` | 中间档 |
| cold | `--idle-interval-ms`，未设则塌到 hot | 最慢档 |

有变化立刻拉回 hot；连续 `--idle-rounds` 次无变化降一档，到 cold 为止。
轮询线程按"最早到点时间"睡，但不超过一个 hot 间隔，所以新订阅者不会被冷档拖住。
梯子会夹紧（`hot ≤ warm ≤ cold`），越界值不会把顺序倒过来。

## 持久连接池

worker 线程与它们的 7709 会话只建立一次，跨轮复用：

| 场景 | 每轮新建会话 |
|---|---:|
| 每轮重连（`sweep` 命令） | 6 |
| 持久池（`watch` / `serve`） | 首轮 6，之后 0 |

隔离测量（1 只证券、20 轮、同一节点），只保留连接与握手的差别：

| 方式 | 平均一轮 | 会话 |
|---|---:|---|
| 每轮重连 | 117 ms | 每轮 1 条 |
| 持久会话 | 48 ms | 首轮 1 条，之后 0 |

差的约 70 ms 就是 TCP 连接加 `0x000D` 握手。全市场 6 条会话并行，省下的是每轮
一次连接建立的等待；**更重要的是不再每秒新建/销毁 6 条 TCP**——按 1 s 节拍跑
一天原本约 52 万次连接，会耗尽临时端口并堆积 TIME_WAIT。

## 实测数据

### 全市场扫描吞吐（5,574 只 A 股，一轮 `0x0547`）

| 并行连接 | 一轮 | 吞吐 | 每轮请求 |
|---:|---:|---:|---:|
| 1 | 1,992 ms | 2,798 条/秒 | 56 |
| 3 | 744 ms | 7,492 条/秒 | 56 |
| 6 | 440 ms | 12,668 条/秒 | 56 |
| 12 | 313 ms | 17,808 条/秒 | 56 |

请求数恒为 56 —— 连接数只改延迟。全部 `5574/5574`，0 重试、0 截断。

### 13:00 复盘（全市场，6 连接，1 s 节拍，连续 24 轮）

| 轮次 | 变化比例 |
|---|---:|
| 1（首轮快照） | 100% |
| 2–6（午休） | 0% |
| 7（13:00 复盘瞬间） | 39% |
| 8–11（开盘冲击） | 54% / 18% / 91% / **93%** |
| 12–24（回稳） | 41% – 70% |

- **盘中变化率：最小 18%、最大 93%、平均 59%**
- 一轮耗时：最小 490 ms、最大 1,382 ms、平均 834 ms
- 下游只推变化：**62,072 条事件** vs 全量重发的 133,776 条 → **少推 53.6%**

休市 5 轮 0% 变化是重要的正确性证据：差分没有误报。

### 目录口径与 C++ 实现逐项一致

52,579 条总计：`unknown` 38,485 / `a_share` 5,574 / `bond` 4,276 / `index`
1,678 / `etf` 1,645 / `fund` 497 / `convertible_bond` 327 / `b_share` 79 /
`repo` 18。

`sweep --limit 200 -j 4` 的记录顺序与 `securities` 的目录顺序逐条相同
（0 mismatch），并行取回后仍按 universe 顺序落位。

### 与 C++ 解码器的逐字段对账

| 字段 | probe 路径 | sweep 路径 |
|---|---|---|
| `pre_close_price` | 5/5 一致 | 5/5 一致 |
| `open_price` | 5/5 一致 | — |
| `high_price` / `low_price` | 5/5 一致 | 5/5 一致 |
| `total_hand` | 5/5 一致 | 5/5 一致 |
| 买卖五档阶梯 | 顺序正确、不交叉 | — |

## 协议要点

- 请求帧：`0x0C` 前缀 + `u32 message_id` + 控制字节 `1` + 两份 `u16` 长度 +
  `u16` 命令号 + 业务体；`message_id` 从 `0x01640801` 递增（扩展市场用 `0x01`
  前缀、从 0 起）。
- 响应帧：16 字节头 `B1 CB 74 00` + 控制 + `u32 message_id` + 保留字节 +
  `u16` 命令号 + `u16` 线上长度 + `u16` 解码长度；两者不等时用 zlib 解压。
- 握手 `0x000D`，体 `0x01`；服务器名在响应体 `[68,152)`，GB18030。
- `0x0547`：请求 `u16 数量` + 每条 `market_id` + 6 位代码 + 4 个零字节；
  响应整体按 `0x93` 异或，`u16 数量` 后是变长记录流（服务端每批上限 100）。
- `0x044E` 取市场证券数，`0x044D` 按 `start/limit` 分页取 37 字节记录
  （代码 6 + 合约乘数 2 + GBK 名称 16 + 精度/昨收等）。
- 记录体：`market_id` + 6 位代码 + `u16 active` + 5 个价格 varint（相对当前价）
  + `u32 更新时间` + `status` + `total_hand` + `current_hand` + `u32 成交额`
  （wire 浮点）+ `inside` + `outside` + `unknown_after_outer` + 开盘额 varint
  + 5 组 `买价增量/卖价增量/买量/卖量` varint + 未消费尾部。
- 价格刻度：`价格 = 增量 * 10 / (divisor * 1000)`；divisor 由品种前缀决定
  （债券/逆回购 100，场内基金 10，其余 1）。

## 变更检测

分八组上报，下游可只订阅关心的部分：

`new` / `last` / `ohlc` / `amount` / `volume` / `dishes` / `book` / `status`

比较用**精确相等**——解码器是确定性的，数值不同就意味着线上字节不同，因此不会
出现抖动误报。

## 可靠性约定

- **截断即失败**：目录下载核对"收到数 == 服务端报告数"；`0x0547` 每批核对
  "解析数 == 请求数"。少于请求数一律让整轮失败。
- 单批失败会在下一个节点重连重试（默认 3 次），失败批次数进统计。
- 任一帧错误即关闭该连接（流已错位，不复用）。
- 订阅者队列有界：容量为 `max(queue_limit, 订阅证券数)`，即至少能装下自己那份
  完整快照；之后的下行变化按"丢最旧"处理并累加 `dropped`。
- 每轮结束广播条件变量，`tdx_hub_next` 不忙等。
- **优雅退出**：`SIGINT`/`SIGTERM` 处理器只置标志并关闭监听 socket（这是把阻塞
  的 `accept()` 拉出来的唯一可靠办法）；运行器随后 `tdx_hub_stop()`（停轮询 +
  唤醒所有阻塞订阅者，但 hub 仍存活），最多等 5 s 让在途连接收尾，再返回让
  调用方依次释放 hub 与连接池。

## 测试

| 套件 | 覆盖 |
|---|---|
| `test_frame` | 组帧字节级断言、响应头、zlib 往返与损坏流 |
| `test_quote` | 证券解析、价格刻度、varint 往返、深度记录编解码、拒绝路径 |
| `test_directory` | 品种/板块分类 32 例、0x044D 页解析、市场号覆盖、截断 |
| `test_endpoint` | 端点解析、`connect.cfg` 大小写混写键、PrimaryHost 轮转、缺文件回退 |
| `test_pool` | **回环假 7709 服务器**上的批切分、顺序、截断、重连 |
| `test_state` | 八组差分、多组并集、哈希表扩容零丢失、市场不碰撞 |
| `test_hub` | 首轮快照、相同轮静默、变化掩码、过滤订阅、未知代码、队列丢弃、失败计数、快照 JSON、分层梯子 |
| `test_zst` | tag 流分帧与拒绝路径、值的数值/文本等价、状态累加、只带变化时的前向折叠、多证券隔离、逻辑组掩码、JSON 渲染（含 `null` 与原始 tag 表）、**真实样本 4647 条的不变量与末日终值** |
| `test_download` | RFC 1321 MD5 向量与分块喂入一致、路径校验、`0x02C5`/`0x06B9` 请求字节级断言、两种应答的解析与拒绝、`day` 路径拼装 |
| `test_trades` | 分钟/买卖方向/刻度/日历校验、两种请求的字节级断言、**活体应答原文**的五个 varint 记录解码、负数增量与累加、分钟越界与悬挂 varint 的拒绝、分页尾部未消费检测、汇总分桶、JSONL 括号平衡与 `null` |
| `test_kline` | 周期别名表（含 `time`/`1m` 共用 id 7）、LC1 日期字解码、`880xxx` 板块指数判定、42 字节请求字段布局与补零、**三份活体应答原文**（日线/分钟/指数）的两种日期编码与 breadth 字段、指数字节当股票解析必须失败、时间排序、JSONL 括号平衡与 `null` |
| `test_timeline` | 点位→时间标签映射（含午休跳段）、12 字节请求布局、**1173 字节活体应答**的 240 点解码、基点+偏移的读法（写成增量就会失败）、逐点成交量合计等于当日总量、截断/超帽/尾部未消费与负量的拒绝、JSONL 括号平衡与 `null` |
| `test_auction` | 时间标签与买卖方向文案、28 字节请求的常数/选择器/起点/上限布局、**两份活体应答**（42 点只开盘 / 61 点开+收）的定长记录解码、带符号未匹配量与方向、两段划分与段内不变量（翻转次数、撮合量下修）、长度不符/非法秒/非法分钟/负价/NaN 的拒绝、JSONL 括号平衡与空段 |
| `test_snapshot` | 基金净值判定（深 158/159、沪 17 个前缀 + 末位 0、北交所恒否）、10+7N 请求布局、**309 字节活体应答**的 3 条记录解码与另一条命令的字段对账（含 `open_amount` 刻度差异与未建模尾部）、记录边界扫描（数量不符/首条不偏移 0/零条）、市场号与代码非法、截断记录、累加器与 JSONL 括号平衡 |
| `test_finance` | 10+7N 请求布局、**860 字节活体应答**的 6 条 143 字节记录解码、6 个真实上市日期、茅台总股本与工行 H 股钉死 ×10000 刻度、字段包络（总资产≥净资产、流通≤总股本）、长度不符/市场号非法/非数字代码/非日期/NaN 的拒绝、累加器与 JSONL 括号平衡与 `null` |
| `test_capital` | 类别 key 表、9 字节请求布局、**2360 字节活体应答**的 81 条 29 字节记录解码、平安银行 2024 年 10 派 7.19 精确复现、26/26 连续股本链、324/324 双读法一致、回显证券不匹配/长度不符/类别越界/非日期/NaN 的拒绝、累加器与 JSONL 括号平衡 |
| `test_gbbq` | 状态表 base64 解码与长度、四张表平铺 0x48/0x448/0x848/0xC48/0x1048、**真实密文**的 5 条记录解密、明文尾部原样透传、1991 年股本链跨记录衔接、2024 年 10 派 7.19 与网络源一致、长度不符/条数超限/缺证券/短输出的拒绝、24 个加密字节逐字节翻转的"损坏永不隐形"统计 |
| `test_limits` | 14 字节请求布局与 16 位下标边界、**真实 15 字节单行应答**的解码、代码数字补零成 6 位、涨跌停中点=1.58 的取值、合成多行页的列表解码与页起始下标延续、长度不符/输出不足/市场非法/代码超 6 位/涨停低于跌停/NaN 的拒绝 |
| `test_json` | 标量/容器语法、全部转义与 surrogate 对（含孤立 surrogate 变 U+FFFD）、**多段拼接字符串必须连续**、**成员名与成员值各存一份**、嵌套数组的兄弟链表、深度上限、尾随数据/尾随逗号/缺冒号/缺逗号/前导零/未闭合/未知转义的拒绝、整数读取 |
| `test_jsn` | 资源路径前缀规范化（前导斜杠、已带前缀不重复）、**整份真实 GBK 载荷**的转换与解析（长度必须变长）、42 行 × 20 列、中文列值精确比对、空单元格保留为空串而非 null、根非数组/缺 colheader-data/**行宽与表头不符**/行非数组的拒绝 |
| `test_bonds` | 20 条 profile 的分类与前缀/斜杠写法、市场命名（含 44→bj 与未知市场的 `M<n>:`）、**抓包资源**的完整字段映射与中文精确比对、三种单位语义各自的换算与"不换算"、合并表/投影的身份列交换与标的、票息表配对与 `|值|≤1 乘 100` 规则、日期多于利率/空表/缓冲区不足（拒绝截断）、缺代码列/市场非数字的拒绝、渲染括号平衡 |
| `test_convertible` | 可交换债判定（132 前缀，含截断码）、**55 列 × 3 行真实抓包**的字段映射、中文精确比对、`core_terms_complete` 判据、**ZGDM"要么空要么六位数字"的实测断言**、概览缺失列回到"缺失而非 0"、缺代码/市场非数字的拒绝、**渲染括号平衡**（该检查抓出了漏掉的 overview 右括号） |
| `test_convertible_join` | 六份 fixture 的列数与源行数、**键并集为 3**（同一批债券在六份里，所以不是 18）与容量不足的拒绝、SH110076 六路全部命中、**三组触发条款取值必须互不相同**（一份文档读三遍会产出一模一样的触发条款）、票息列表条数 == 期数、只给一份文档/不给概览时仍能连接并把缺失报为缺失、**连接后渲染的括号平衡** |

`test_pool` 与 `test_hub` 都不碰公网：前者自建回环 7709 服务器，后者注入
确定性 feed。`test_zst` 在不存在的样本目录上会 `skip:` 并以 0 退出；
`test_download` 完全不联网（传输链路的活体验证放在
`output/zst_transfer_evidence.txt`）。

## 尚未完成

6. **可转债的待发/申购/定价三个视图**：六文档连接与换股替换/投影兜底已就位；
   注册表里剩下的 `dfkzz201`（待发）、`func_kkzss101`（申购）、`gxjty_zq_kzzsy101`（定价）
   尚未移植。
7. **`0x0010` 里三个未标定的股本类别槽位**（national / promoter_legal_person / legal_person）：实测在工行、茅台身上给出不可能是股本的数值，需要另找消费者证据。

1. **真服务端推送（B 方案）**：`FastHQ.Subscribe` 需要已登录的 tpbus/TaApi
   会话，且推送帧格式尚未恢复。连"L1 有没有服务端推送"都还没证实——需要一次
   客户端被动观察（Step 0）。
2. **Ctrl+C 的真实验证**：优雅退出代码路径完整，但只在脱离控制台的进程上测过
   （Windows 走的是控制台关闭事件 `0xC000013A`，不经过本处理器）。需要在前台
   控制台手按一次。
3. 按订阅者集合进一步裁剪**批次数**（现在是按下标集合重排，批次边界可以更紧凑）。
4. **指定日期分时 `0x0FB4`**：请求格式还没复原。范围已收窄到"11 字节体被识别、那 4 个
   尾部字节不是 YYYYMMDD 日期"，探测日志与下一步假设见 `output/history_timeline_recovery_log.txt`。
   最省事的收尾办法是从 TdxW 里直接读出它自己的调用点，而不是继续盲试。
5. **两条 L1 命令各留一大段未建模字节**：0x0547 的 46 字节、0x054C 的 61–62 字节尾部。
   本实现只报数量、不编语义；要标定它们需要另找消费者证据（TdxW 侧或对照物）。
5. **L2 秒级逐笔成交 / 逐笔委托**：交易所口径的那个，走内置 `1364`/`1374` 或
   SDK `4655`/`1801`/`1802`，需要授权业务事件。本仓库只做到结构确认与被动探针，
   `c/` 侧没做——公开会话拿不到，做了也无法验证。
6. `.img` 的累计字段止于 15:00，而成交明细含盘后固定价格成交（`status=5`）。
   两者口径不同是**已验证的事实**，不是缺口；用的时候记得按 `time` 或 `status` 切。
7. `1i..1m` 这个阶段辅助块还没有语义。
8. **历史 `.img` 的公网取数被挡**：见上文，公开节点一律报长度 0；需要恢复带权益
   会话的握手/口令链路，或继续依赖客户端自己的缓存。
