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
  src/
    tdx_bytes.c  tdx_text.c  tdx_thread.c  tdx_endpoint.c  tdx_frame.c
    tdx_quote.c  tdx_directory.c  tdx_pool.c  tdx_state.c  tdx_format.c
    tdx_hub.c  tdx_serve.c  main.c
  tests/
    test_frame.c  test_quote.c  test_directory.c
    test_pool.c   test_state.c  test_hub.c
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
6/6 测试通过、0 warning。

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
```

通用参数：`--security`（可重复）、`--market sz,sh,bj`、`--category`、`--limit`、
`--host`、`--root`、`--timeout-ms`、`--endpoints`、`-j/--connections`、
`--batch-size`、`--iterations`、`--interval-ms`、`--output`。
Hub 相关：`--tier-warm-ms`、`--idle-interval-ms`、`--idle-rounds`、
`--heartbeat-ms`、`--max-subscribers`、`--port`。

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
| `test_pool` | **回环假 7709 服务器**上的批切分、顺序、截断、重连 |
| `test_state` | 八组差分、多组并集、哈希表扩容零丢失、市场不碰撞 |
| `test_hub` | 首轮快照、相同轮静默、变化掩码、过滤订阅、未知代码、队列丢弃、失败计数、快照 JSON、分层梯子 |

`test_pool` 与 `test_hub` 都不碰公网：前者自建回环 7709 服务器，后者注入
确定性 feed。

## 尚未完成

1. **真服务端推送（B 方案）**：`FastHQ.Subscribe` 需要已登录的 tpbus/TaApi
   会话，且推送帧格式尚未恢复。连"L1 有没有服务端推送"都还没证实——需要一次
   客户端被动观察（Step 0）。
2. **Ctrl+C 的真实验证**：优雅退出代码路径完整，但只在脱离控制台的进程上测过
   （Windows 走的是控制台关闭事件 `0xC000013A`，不经过本处理器）。需要在前台
   控制台手按一次。
3. 按订阅者集合进一步裁剪**批次数**（现在是按下标集合重排，批次边界可以更紧凑）。
