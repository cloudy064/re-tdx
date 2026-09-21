# tdx-l1stream

独立纯 C（C11）实现的通达信 L1 行情客户端与常驻推流服务。与仓库里的 C++
研究工具 `../native` **互不依赖**：C++ 版本保留为协议证据与逐字段对照物。

进度：协议层、目录/连接池、状态差分、常驻 Hub 与 HTTP/SSE 服务均已完成。
上游仍是公开 7709 的有界轮询 —— 协议没有增量接口，增量是在本进程里算出来的。

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
    tdx_hub.h             常驻轮询 Hub、订阅者队列、快照重放
    tdx_serve.h           只读 HTTP / SSE 服务
  src/                    tdx_bytes text thread endpoint frame quote
                          directory pool state format hub serve main
  tests/                  frame quote directory pool state hub
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

已验证：MSYS2 UCRT64 GCC 15.1.0 + zlib 1.3.1 + Ninja（VS 自带），6/6 测试通过、0 warning。

注意：
- PATH 里出现 `C:\msys64\usr\bin`（`sh.exe`）会让 CMake 无法创建 `MinGW Makefiles`
  生成器；用 Ninja 绕开。
- `FindZLIB` 找不到时 CMakeLists 退回 `-lz`，走编译器 sysroot。
- MinGW 在 `-std=c11` 下默认链接旧 MSVCRT `printf`（不支持 `%zu`）；CMakeLists 对
  MINGW 加了 `__USE_MINGW_ANSI_STDIO=1`。

## 用法

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

    tdx-l1stream serve --market sz,sh,bj --category a_share -j 6 `
                       --interval-ms 1000 --idle-interval-ms 15000 `
                       --idle-rounds 30 --port 8790 --root C:\new_tdx
tdx-l1stream serve --market sz,sh,bj --category a_share -j 6 `
                   --interval-ms 1000 --port 8790 --root C:\new_tdx
```

### 服务端路由

| 路由 | 说明 |
|---|---|
| `GET /` | 用法页 |
| `GET /health` | 存活检查 |
| `GET /status` | Hub 计数：轮次、记录、事件、订阅者、队列、丢弃、最近错误 |
| `GET /api/v1/market/snapshot?codes=a,b` | 当前存量值，一次性 |
| `GET /api/v1/market/stream[?codes=a,b]` | **SSE**；省略 `codes` 即全 universe |

SSE 参数：`max_events=N`（取够即断）、`wait_timeout_ms=N`（空闲多久断开）。

服务参数：`--idle-interval-ms N` + `--idle-rounds N`：连续 N 轮无变化后把节拍
降到 N 毫秒（0 表示关闭），任何一次变化立即回到 `--interval-ms`。

事件类型：

```json
{"type":"snapshot","sequence":1,"security_id":"SZ000001","changed":["new"],"updates":2,"record":{...}}
{"type":"change","sequence":2,"security_id":"SZ000001","changed":["amount","volume","dishes","book","status"],"record":{...}}
{"type":"heartbeat","sequence":3,"subscribed":5574,"total_events":1128}
```

迟到订阅者**立即收到已订阅证券的快照**，之后只收变化。

## 实测（2026-09-21，节点 110.41.147.114:7709）

### 全市场扫描吞吐（5,574 只 A 股，一轮 `0x0547`）

| 并行连接 | 一轮 | 吞吐 | 每轮请求 |
|---:|---:|---:|---:|
| 1 | 1,992 ms | 2,798 条/秒 | 56 |
| 3 | 744 ms | 7,492 条/秒 | 56 |
| 6 | **440 ms** | 12,668 条/秒 | 56 |
| 12 | **313 ms** | 17,808 条/秒 | 56 |

请求数恒为 56 —— 连接数只改延迟。全部 `5574/5574`，0 重试、0 截断。

### 13:00 复盘实测（全市场 5,574 只，6 连接，1 s 节拍）

连续采集 24 轮，跨过午间休市到下午开盘：

| 轮次 | 变化比例 |
|---|---:|
| 1（首轮快照） | 100% |
| 2–6（午休） | 0% |
| 7（13:00 复盘瞬间） | 39% |
| 8–11（开盘冲击） | 54% / 18% / 91% / **93%** |
| 12–24（回稳） | 41% – 70% |

- **盘中变化率：最小 18%，最大 93%，平均 59%**
- 一轮耗时：最小 490 ms，最大 1,382 ms，**平均 834 ms**（5,574 只 / 6 连接）
- 下游输出：**62,072 条事件** vs 全量重发的 133,776 条 → **少推 53.6%**

休市场景 0% 变化是重要的正确性证据：差分没有误报。

### 与 C++ 实现的对账

| 字段 | probe 路径 | sweep 路径 |
|---|---|---|
| `pre_close_price` | 5/5 一致 | 5/5 一致 |
| `open_price` | 5/5 一致 | — |
| `high_price` / `low_price` | 5/5 一致 | 5/5 一致 |
| `total_hand` | 5/5 一致 | 5/5 一致 |
| 买卖五档阶梯 | 顺序正确、不交叉 | — |

目录口径也逐项一致（52,579 条；a_share 5,574 / bond 4,276 / index 1,678 /
etf 1,645 / fund 497 / convertible_bond 327 / b_share 79 / repo 18 / unknown 38,485）。

`sweep --limit 200 -j 4` 的记录顺序与 `securities` 的目录顺序逐条相同（0 mismatch）。

## 协议要点

- 请求帧：`0x0C` 前缀 + `u32 message_id` + 控制字节 `1` + 两份 `u16` 长度 +
  `u16` 命令号 + 业务体；`message_id` 从 `0x01640801` 递增。
- 响应帧：16 字节头 `B1 CB 74 00` + 控制 + `u32 message_id` + 保留字节 +
  `u16` 命令号 + `u16` 线上长度 + `u16` 解码长度；两者不等时用 zlib 解压。
- 握手 `0x000D`，体 `0x01`；服务器名在响应体 `[68,152)`，GB18030。
- `0x0547`：请求 `u16 数量` + 每条 `market_id` + 6 位代码 + 4 个零字节；响应整体
  按 `0x93` 异或，`u16 数量` 后是变长记录流（服务端每批上限 100）。
- `0x044E` 取市场证券数，`0x044D` 按 `start/limit` 分页取 37 字节记录。
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
- 每轮结束都会广播条件变量，`tdx_hub_next` 不忙等。

### 持久连接池（2026-09-21 14:2x）

worker 线程与它们的 7709 会话只建立一次，跨轮复用：

| 场景 | 每轮新建会话 |
|---|---:|
| 每轮重连（sweep 命令） | 6 |
| 持久池（watch / serve） | 首轮 6，之后 0 |

隔离测量（1 只证券、20 轮、同一节点），只保留连接与握手的差别：

| 方式 | 平均一轮 | 会话 |
|---|---:|---|
| 每轮重连 | 117 ms | 每轮 1 条 |
| 持久会话 | 48 ms | 首轮 1 条，之后 0 |

差的约 70 ms 就是 TCP 连接加 0x000D 握手。全市场 6 条会话并行，省下的是每轮一次
连接建立的等待；更重要的是不再每秒新建/销毁 6 条 TCP —— 按 1 s 节拍跑一天原本约
52 万次连接，会耗尽临时端口并堆积 TIME_WAIT。

全市场实测（10 轮，14:40 前后）：sessions per round = 6,0,0,0,0,0,0,0,0,0；
一轮 778–2140 ms（盘中活跃度波动大，该窗口偏高）。

### 自适应节拍

连续 N 轮没有变化就把节拍降到 idle 档，任何一次变化立即回到活跃档：

```powershell
tdx-l1stream serve ... --interval-ms 1000 --idle-interval-ms 15000 --idle-rounds 30
```

午间休市与盘后全市场变化率为 0，此时每 1 秒轮询 56 个请求纯属浪费；退到 15 秒后
上游压力降到约 3.7 req/s。状态接口的 `effective_interval_ms` 与 `quiet_rounds`
可以直接观察当前处于哪一档。

## 尚未完成

1. 持久化工作线程池（现在每轮创建/销毁 worker 线程，约 0.5 ms/轮）；
2. 分层节拍（自选 1 s / 其余 3–5 s）与自适应升降频；
3. 优雅退出的信号处理（当前由进程终止结束）；
4. B 方案（FastHQ.Subscribe 真服务端推送）需要的登录与推送帧格式仍未恢复。
