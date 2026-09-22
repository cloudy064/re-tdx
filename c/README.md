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
    tdx_pending.h          dfkzz201 待发计划表与集合对账
    tdx_pending_json.h     待发行的 JSONL 渲染
    tdx_subscription.h     申购事件与两个派生指标
    tdx_subscription_json.h 申购事件的 JSONL 渲染
    tdx_bond_math.h        应计利息/现金流/贴现/YTM 求解
    tdx_pricing.h          定价视图：条款 + 报价联接 + 估值
    tdx_pricing_json.h     定价行的 JSONL 渲染
    tdx_newbond.h          新债投影映射与对账
    tdx_newbond_json.h     投影行与对账的 JSONL 渲染
    tdx_professional.h     professional_data .dat 解析与字段表
    tdx_professional_json.h 该族的 JSONL 渲染
    tdx_professional_finance.h 季度财务包（定长表，零拷贝读取）
    tdx_zip.h              够用即止的 ZIP 读取（EOCD/中央目录/CRC）
    tdx_daily.h            本地 .day 日线（32 字节记录 + 按品种的价格口径）
    tdx_daily_json.h       日线 bar 与汇总的 JSONL 渲染
    tdx_minute.h           本地 .lc1 分钟线（OHLC 越界只计数不拒绝）
    tdx_minute_json.h      分钟 bar 与汇总的 JSONL 渲染
    tdx_industry.h         行业估值资源（同资源内两种成员计数互证）
    tdx_industry_json.h    行业行与股票-行业行的 JSONL 渲染
    tdx_limit.h            hqrule.dat 的涨跌停规则与限价计算
    tdx_valuation.h        指数估值：当前表 + PE/PB 历史合并
    tdx_valuation_json.h   指数/历史点/合并报告/基金的 JSONL 渲染
    tdx_ranking.h          0x054B 分类行情排名（价格是相对收盘的差值）
    tdx_ranking_json.h     排名行与汇总的 JSONL 渲染
    tdx_seal.h             封单量/封单比（三条路径，金额带符号）
    tdx_seal_json.h        封单结果与其输入的 JSONL 渲染
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
    tdx_convertible_join.c  tdx_pending.c  tdx_pending_json.c
    tdx_subscription.c  tdx_subscription_json.c  tdx_bond_math.c
    tdx_pricing.c  tdx_pricing_json.c  tdx_newbond.c  tdx_newbond_json.c
    tdx_professional.c  tdx_professional_json.c
    tdx_professional_finance.c  tdx_zip.c  tdx_daily.c  tdx_daily_json.c
    tdx_minute.c  tdx_minute_json.c  tdx_industry.c  tdx_industry_json.c
    tdx_limit.c  tdx_valuation.c  tdx_valuation_json.c
    tdx_ranking.c  tdx_ranking_json.c  tdx_seal.c  tdx_seal_json.c
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
    test_pending.c   pending_fixtures.h (generated, reduced capture)
    test_subscription.c  subscription_fixtures.h (generated, reduced capture)
    test_bond_math.c (calendar vs Python, closed form, fixed point, refusals)
    test_pricing.c   pricing_fixtures.h (generated, three shapes)
    test_newbond.c   (shares subscription_fixtures.h, which now also carries the
                      projection whole and the subscriptions it names)
    test_professional.c  professional_fixtures.h (generated, real .dat prefixes)
    test_daily.c  daily_fixtures.h (generated, real .day prefixes)
    test_minute.c  minute_fixtures.h (generated, a real .lc1 violation window)
    test_industry.c  industry_fixtures.h (generated, whole industries)
    test_limit.c (the real hqrule.dat, inline, and the rounding arithmetic)
    test_valuation.c  valuation_fixtures.h (generated, master whole)
    test_ranking.c (a response built by the test, its varints computed)
    test_seal.c (the three seal paths, built from inputs)
    test_professional_finance.c  professional_finance_fixtures.h
                      (ZIP archives written by Python, read by this code)
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
37/37 测试通过、0 warning。

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

# 本地日线：32 字节记录的 .day 文件，价格口径按品种（不联网）
tdx-l1stream daily --security sh600519 --root C:\new_tdx `
                  --output output\daily-sh600519.jsonl

# 本地分钟线：32 字节记录，价格已是元（不需要口径规则）
tdx-l1stream minute --security sh600519 --root C:\new_tdx `
                   --output output\minute-sh600519.jsonl

# 行业估值：每个行业一行（--bonds 再附上每只股票的行）
tdx-l1stream industry --output output\industry.jsonl

# 涨跌停规则（读本机 hqrule.dat），以及一只股票的限价
tdx-l1stream limit --root C:\new_tdx
tdx-l1stream limit --root C:\new_tdx --security sz000001 --prev 11.70 `
                   --date 20260922

# 指数估值：11 个指数的当前表
tdx-l1stream valuation --output output\valuation.jsonl

# 加上某个指数的 PE/PB 历史（按日期合并）与跟踪它的基金
tdx-l1stream valuation --security sh000001 `
                      --output output\valuation-000001.jsonl

# 分类行情排名（0x054B）：按涨幅降序，服务端分页
tdx-l1stream ranking --sort change-pct --limit 100 `
                    --output output\ranking.jsonl

# 封单：一只证券的涨跌停价与封单量/封单比
tdx-l1stream seal --security sz000504 --root C:\new_tdx `
                 --output output\seal-000504.jsonl

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

# 待发计划表 + 两份投影的集合对账
tdx-l1stream pending --root C:\new_tdx `
                     --output output\pending.jsonl

# 申购事件（含转股价值与溢价率两个派生指标）
tdx-l1stream subscription --root C:\new_tdx `
                          --output output\subscription.jsonl

# 定价：条款 + 实时报价联接 + 估值
tdx-l1stream pricing --root C:\new_tdx `
                     --output output\pricing.jsonl

# 新债投影与申购列表对账
tdx-l1stream newbond --root C:\new_tdx `
                     --output output\newbond.jsonl

# 解析一个本地 professional_data .dat（不联网，文件由外部取回）
tdx-l1stream professional --input gpsz000001.dat --kind stock `
                        --field 3 --from 20240101 --to 20241231 `
                        --output output\professional.jsonl

# 季度财务包：整个市场的营收/净利同比，或某一只的全部 584 列
tdx-l1stream professional --zip gpcw20260630.zip `
                        --output output\finance.jsonl
tdx-l1stream professional --zip gpcw20260630.zip --code 600519 --field 183 `
                        --output output\maotai.jsonl
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

## 待发可转债计划表（`pending`）

已经公告、尚未上市的转债计划。**待发债券还没有自己的代码**，所以这一层的身份是它将要转股的
**正股**——这也正是集合对账用的键。

| 资源 | 角色 | 实测规模 |
|---|---|---:|
| `list/dfkzz201_1.jsn` | 主资源 | 18,597 字节 / 153 行 × 16 列 |
| `list/func_kzz102_1.jsn` | 投影 A | 11,878 字节 / 145 行 × 9 列 |
| `list/gxjty_zq_dfkzz102_1.jsn` | 投影 B | 20,607 字节 / 154 行 × 18 列 |

### 这里的"投影"和上一节不是一回事

参考实现对三份资源用**同一个映射**归一化，然后对每份投影报一份**集合对账**：哪些标的证券
两边都有、哪些只在主资源、哪些只在投影。

所以投影在这里回答的是**"另一份视图是否同意谁在筹划发行"**，而**不是**"该用哪个值填空"——
后者是上一节换股投影回答的问题。两种机制不同，按同一种方式处理会产出没人要的报告。

实测差异：**投影 B 是主资源的超集**（多 `SZ300495`，0 只只在主资源）；
**投影 A 缺 9 只**（SH603339 / SH600456 / SH688210 / SZ300727 / SZ300491 / SZ300881 /
SH605589 / SZ000422 / SZ002997）。若把它们当成同一份数据的不同副本，会漏掉那 9 只或凭空多出 1 只。

### 两处与项目其他地方不同（有意的）

- **列名是小写的**（`zzlx`/`mzgm`/`fadj`/`date0`/`byhq`/`zgj`/`gdpsl`/`sgrq`/`fxrq`/`zql`/`zqr`/`sgdm`/`sgmc`/`fxjg`），
  这是项目里唯一一份小写列名的 JSN 资源。
- **读不出来的行是跳过而不是报错**：这是一份"计划"，一行挂不到股票上就没有用；
  而一只**已上市**债券认不出来，说明文档本身有问题（六文档连接那边就是直接报错）。
  测试对两半都做了断言。

### 验证

| 检验 | 结果 |
|---|---|
| 抓包前三行的字段映射 | 类型/规模/进度/进度日期/转股价逐个断言，中文精确比对 |
| `gdpsl` 列存在但为空 | 输出 `null` 而**不是 0**（这正是 `has_*` 的意义） |
| 集合对账逻辑 | 用测试内构造的数组覆盖：完全相同（含顺序不同）、两侧各有差异、**重复身份**、**空身份**、空投影、两个空集 |
| 153 行主资源 | 0 行被跳过 |

证据：`output/pending_verification_evidence.txt`。

## 可转债申购（`subscription`）

资源 `bi/list/func_kkzss101_1.jsn`：**50,783 字节 / 319 行 × 16 列**（活体实测）。

### 两个派生指标（资源本身不带，是本层算的）

```
转股价值 = 正股收盘价 × 100 / 转股价
溢价率   = (债券收盘价 − 转股价值) × 100 / 转股价值
```

`100` 是面值（元），所以转股价值就是"一张债券转成股票值多少钱"。**两处除法都有保护**：
转股价为 0 或转股价值为 0 时字段**缺失**，而不是产出无穷大或一个读起来像真数字的 0。
这正是 `has_*` 标志存在的意义，测试用 0 / −0 / 无穷 / NULL 四种输入逐条断言这两道保护。

### 验证

| 检验 | 结果 |
|---|---|
| 首行（南航转债）派生指标 | `4.950 × 100 / 6.170 = 80.226904`、`(106.368 − 80.226904) × 100 / 80.226904 = 32.583952%` |
| **独立重算**（Python 从同样输入再算一遍） | 转股价值 **319/319**、溢价率 **319/319**，最大偏差 **4.99e-07** |
| **跨视图对账**：两个视图都有转股价的债券 | **314 只，全部一致，0 只不一致** |
| 活体汇总 | 319 事件、0 行跳过、315 已上市、319 有转股价值、319 有溢价率 |

两条独立路径的**跨视图对账**是这里最有价值的一条：申购文档的 `zgj` 与概览文档的 `ZGJ`
是同一只债券的转股价，来自两份不同的资源，314 只全部读出同一个数——这同时检验了两个
字段映射，而不是各自自洽。

关于"独立重算"还有一个**方法上的教训**记在证据里：我的证据脚本第一版用 `1e-9` 容差去比一个
只打印了六位小数的结果，于是报出"319 行里只有 4 行一致"。**那是在量打印精度，不是在量算术**——
容差必须与证据的精度匹配。最大偏差 4.99e-07 正好是 `%.6f` 的半个最小位，说明差异纯粹来自舍入。

### 与待发表的另一处不同

申购行**必须**同时能读出债券与正股两套身份（代码 + 市场），缺一即跳过并计数——
"挂不到债券和标的上的事件不算事件"。待发表则只要求正股可读。

### 事件 id

形如 `convertible-subscription:<市场>:<债券代码>:<申购日>`，照参考实现的拼法，
便于识别"同一个事件出现两次"。申购日缺失时 id 以冒号结尾——参考实现也是这样。

### 未移植

- 参考实现的**新债投影**（`list/gxjty_zq_xkzz102_1.jsn`）与申购行的匹配报告
  （按申购代码优先、标的兜底匹配，报告申购日不一致与发行规模差额，并做 hybrid/stale 分类）；
- **定价视图**（`list/gxjty_zq_kzzsy101_1.jsn`）。

证据：`output/subscription_verification_evidence.txt`。

## 债券定价算术（`tdx_bond_math`）

定价视图读到行之后套用的招股书算术，参考实现逐条移植：

```
应计利息   IA = 面值 × 当期利率 × 已计息天数 / 365
剩余现金流 每个剩余付息日一笔，最后一笔同时偿还面值
贴现价值   Σ 金额 / (1 + 年利率)^年数
到期收益率 对上面的贴现做二分（区间 [-0.999999, 10]，160 次）
```

时间口径全程 `days/365`，**不是**债券市场的 ACT/365 结算惯例——这是该表自己的规则，
所以照抄而不去"改进"：改进就成了另一个数。

### 这一层难得地能被外部事物检验，所以用了四种独立方式

| 方式 | 内容 |
|---|---|
| **日历** | 日序数与 **Python 的 `datetime`**（不同算法）对照，含闰日与整百年 |
| **闭式解** | 单笔现金流的收益率有解析答案 `(金额/价格)^(1/年数) − 1`，用它检验二分 |
| **不动点** | 解出的收益率代回去贴现，必须还原给定价格——检验的是**答案**而非路径 |
| **拒绝** | 每一处不成立都必须返回"缺失"，而不是一个看起来像数据的数 |

### 两处最容易写错、因而单独断言的细节

- 应计利息用剩余利率列表的**第一项**；在付息日当天是 **0**（是值，不是缺失）。
- **最后一笔现金流同时偿还面值**：三笔 0.01/0.02/0.03 的票息 + 面值 100 → 最后一笔 **103.0**
  而不是 3.0。漏掉这条会得到一个**悄悄偏高**的收益率。

### 拒绝的边界是测出来的，不是假设的

贴现的年利率恰为 −100% 或更低、现金流条数为 0、价格为 0 或负、无现金流 → 一律缺失。
求解器的区间实测：高利率端 `100/11 = 9.0909`，低利率端（−99.9999%）可达 `1e8`，
所以价格 `1.0` 被拒而 `1000` 可解——这两条边界都写进了测试。

### 范围

本模块只交付**算术层**。定价**行映射**与**报价联接**（定价资源与 `0x054C` 快照按
市场+代码联接，再算全价 / 转股价值 / 溢价 / 纯债价值）尚未移植。
定价资源本身：`bi/list/gxjty_zq_kzzsy101_1.jsn`，**1,265,167 字节 / 312 行**（活体实测）。

证据：`output/bond_math_verification_evidence.txt`。

## 可转债定价视图（`pricing`）

把**条款**、**实时报价**与**估值**三层拼成一行。资源 `bi/list/gxjty_zq_kzzsy101_1.jsn`：
**1,265,172 字节 / 312 行 × 43 列**。

| 分组 | 内容 |
|---|---|
| `terms` | 面值、转股价、转股起止、上市/起息/到期、剩余年限、评级、**票息表**、剩余票息表、三档触发的**比例**与由比例算出的**触发价**、两点收益率曲线 |
| `quote` | 债券与正股的现价、**价格来源**、涨跌幅、成交额 |
| `valuation` | 应计利息、全价、转股价值、转股溢价率、到期收益率、纯债价值、纯债溢价率、双低 |

算术在 `tdx_bond_math`（上一轮已交付并有独立验证）；本层只负责**装配**，并决定每一项在什么
条件下"缺失"。`availability` 三个词说明这一行走到了哪：`complete` / `bond-only` / `terms-only`。

### 报价联接

312 行据此收集到 **621 只证券**（312 债券 + 309 正股），按 `0x054C` 的 80 只上限**分 8 批**
取回，**621/621 全部拿到报价**，312 行全部 `complete`、312 行有到期收益率。

每侧价格都带 `price_source`：`last-price` / `pre-close` / `unavailable`——**现价为正先用它，
否则退回昨收**。为什么必须随价格一起输出：**用昨收算出的溢价与用现价算出的不是同一个论断**。

### 活体全量对账（每个派生字段都对回它的输入）

| 检验项 | 通过/可查 | 最大偏差 |
|---|---:|---:|
| 三个触发价 = 转股价 × 比例 / 100 | 312/312 | 5.7e-14 |
| 全价 = 现价 + 应计利息 | 312/312 | 5.7e-14 |
| 转股价值 = 正股价 × 面值 / 转股价 | 312/312 | 5.0e-07 |
| 转股溢价率 = (全价/转股价值 − 1) × 100 | 310/312 | 5.5e-05 |
| 纯债溢价率 = (全价/纯债价值 − 1) × 100 | 311/312 | 2.7e-05 |
| 双低 = 现价 + 转股溢价率 | 312/312 | 3.6e-12 |
| **按收益率贴现现金流还原全价** | **307/312** | **1.2e-02** |

最后一条是最强的：那正是求解器声称要解的方程，而现在是在**活体数据**上由**另一个实现**验证。
不足 312 的行与 1.2e-02 的偏差**已查清不是 bug**：它们都是收益率逼近 −100% 的行，此时
`d(价值)/d(收益率) ≈ −3.5e5`，而收益率以"百分比 6 位小数"打印（分数精度 1e-8），
`3.5e5 × 5e-9 ≈ 0.0018` 与观测吻合——**残差恰好是打印精度被极大的导数放大**。

### 一个实测出来的数据缺陷（不是本移植引入的）

`SH132026`（G三峡EB2）的报价是 **13522.20**，而面值只有 100。用本机另一条独立数据路径核对：

| 来源 | 原值 | 换算 |
|---|---|---|
| `sh600519` 日线（茅台） | 127588 | /100 = **1275.88 元** ✓ 标定股票口径 |
| `sh110075` 日线**上市首日** | 1102300 | /10000 = **110.23 元** ✓ 债券上市首日就在 100 附近 |
| `sh132026` 日线 | 1342060 | /10000 = **134.206 元** |
| `sh132026` 线上报价 | — | 13522.20 / 100 = **135.222 元** |

两个来源在 1% 内一致 ⇒ **线上 `132xxx` 的价格大了 100 倍**。根因：共享价格除数表
**没有 `13` 规则**（只有 `1318`）——与参考实现 `price_divisor_rules` **逐字相同**，
所以这是**参考实现自带的缺陷**，本移植忠实复现了它。

#### 先标注、后测量、再收窄地修

发现时的处理是**标注**（定价视图的 `price_plausible`：债券价 > 10 倍面值即置 false，
**数字照原样输出**——隐藏数字只会让缺陷消失而不是被看见）。随后做了**逐族测量**：

方法分三步，且带**对照组**（`110`/`113`/`123`/`127`，它们的除数本来就是 100）：

| 族 | 可测代码数 | 修复前中位比值 | 修复后中位比值 |
|---|---:|---:|---:|
| **`132xxx`** | 2 | **102.152** | **1.022** |
| `110xxx`（对照） | 6 | 0.984 | 0.984 |
| `113xxx`（对照） | 6 | 0.969 | 0.969 |
| `123xxx`（对照） | 4 | 0.981 | 0.981 |
| `127xxx`（对照） | 6 | 0.994 | 0.994 |

比值 = 线上价格 / (本地日线收盘 / 10000)。**四个对照族都是 1 附近** ⇒ 方法可信；
**`132xxx` 是 102** ⇒ 线上价格确实大了 100 倍。

（日线口径本身也是标定出来的，不是假设的：茅台 `127588 / 100 = 1275.88` ✓ 股票是 /100；
而南航转债**上市首日** `1102300 / 10000 = 110.23` ✓ 债券是 /10000——首日必在面值附近。）

于是给除数表**加了一条 `{"132", 100}`**，并只加了这一条：

- **`132` 而不是 `13`**：本机 `13` 族 23 个代码**全是 `132xxx`**，其中只有 2 只仍在交易，
  `130`/`131`/`133..139` **一个可测代码都没有**。现有证据下放宽到 `13` 是**猜**而不是测量。
  测试里显式断言 `130001` 仍是 1（"未测量即不改"）。
- **只动目标族**：上表"修复后"一列里四个对照组**一个没变**——这既是修复有效的证明，
  也是**没有误伤**的证明。
- 端到端：`snapshot sh132026` 由 13522.2 → **135.222**；定价视图由 **2 行不可信 → 0 行**，
  两只 EB 的到期收益率由 −65.55%/−99.91% 变为 **−5.15%/−29.59%**。
  （仍为负是**真实的**：可转债价格远高于赎回价时年化收益本来就负；全市场有 54 行低于 −20%。
  修复只消除了"因单位错误而产生的"极端值。）

参考实现的 `price_divisor_rules` 与本移植**逐字相同**，差别只是本移植多了这一条——
即**参考实现自己把可交换债价格解成面值的 100 倍，本移植测量后比参考更正确**。

`price_plausible` 标注**保留**：它现在对 EB 已不再触发，但仍是一张通用的安全网，
任何未来的单位错误都会在那里露头。阈值取 10 倍面值而非 2 倍——可转债真的涨到过 3000 元，
实测 `SH113615` 报价 **738 元**（日线 671.657，同量级）**没有**被误标。

### 一个看起来像 bug 的正常值

`SH110075` 的 `maturity_yield_pct = −57.8%`：它全价 112.45，而 23 天后只拿回 106.5
（末期票息 6.5 + 面值 100），年化确实是负 57.8%。**债券高于赎回价且只剩一期时，
到期收益率为负是正确的**——测试里特意断言了这个符号，免得下一个人去"修"它。
另有两行的 `-99.9%` 则是上面那个价格缺陷的后果，已被 `price_plausible` 标注。

证据：`output/pricing_verification_evidence.txt`。

## 新债投影对账（`newbond`）

第二份独立的"即将发行"清单，与申购列表**互相对照**而不是单信一份：

| 资源 | 角色 | 实测规模 |
|---|---|---:|
| `list/func_kkzss101_1.jsn` | 主资源（申购） | 50,783 字节 / 319 行 / 16 列 |
| `list/gxjty_zq_xkzz102_1.jsn` | 投影 | 2,824 字节 / 13 行 / 20 列 |

匹配规则照参考实现：**申购代码优先，标的证券兜底**，并且报出究竟走了哪一种——
**代码匹配是较强证据，兜底匹配是较弱证据**，读者应当知道拿到的是哪一种。

### 实测对账结果

```
13 行投影，319 行申购
13/13 按申购代码匹配，0 只走兜底，0 只未匹配
0 处日期不一致
2 处发行规模不一致（+9.80 亿 / -4.22 亿）
```

**那 2 处不一致才是这个对账的意义**——否则它只是把两份一样的清单念了一遍。

### 一个差点被写进证据的假象

我的第一版分析脚本报出 **"13/13 全部日期不一致"**。这个数字太整齐，所以去看了原始值：

| 来源 | 原始值 |
|---|---|
| 投影 `sgrq` | `"2026-06-26 星期五"`（带中文星期） |
| 申购 `sgrq` | `"20260626"`（紧凑） |

参考实现的 `compact_date_prefix` 正是为消除这个差异而存在的。**我拿未压缩的投影日期去比申购的紧凑日期，
于是每一行都"不一致"——那个 13/13 量的是我的比较方式，不是债券的事实。** 修正后是 **0 处**。

测试把这一点固化成了断言：**如果谁把日期压缩去掉，这个计数会立刻变成 13，而断言正是期望 0**。

### 匹配的语义正确性（不只是字符串相等）

首行：投影 `sgdm=070422`（宜化发债，标的 SZ000422，计划 33 亿）→ 命中申购
`convertible-subscription:0:127114:20260626` → `primary_bond = SZ127114`，**其标的正是 SZ000422** ✓，
两边规模都是 33 亿、差额 0。测试对全部 13 行都断言了"匹配到的申购行的标的 == 投影点名的标的"。

规模差异的容忍度是 **0.01 亿（100 万元）**：栏目单位是亿元，低于 100 万的差异不当作分歧，
否则浮点噪声会被当成新闻。

证据：`output/newbond_verification_evidence.txt`。

## 公开数据族：professional_data（`professional`）

一族的公开统计/基本面数据，**已交付解析半边**。三张字段表：**44 个个股 + 42 个市场 +
15 个板块**（股东人数、龙虎榜、融资融券、大宗交易、陆股通、涨停板封单、市值、股息率、
质押、回购、期指净持仓、板块市盈率……）。

```
tdxgp/gpszsh.txt              清单：<文件名>,<md5>,<字节数>
tdxgp/gp<市场><代码>.dat       个股，例如 gpsz000001.dat
tdxgp/gpsh999999.dat          市场级
tdxgp/gpsh880471.dat          板块级
```

### 交易文件格式（记录是定长的 13 字节，无文件头）

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | `u8` | 字段 id |
| 1 | `u32` | 日期 YYYYMMDD（小端）；**0 表示"无日期"** |
| 5 | `f32` | 第一个值 |
| 9 | `f32` | 第二个值 |

**记录尺寸是被测出来的，不是猜的**：清单里 **8,647 个条目的字节数全部能被 13 整除**
（余数集合只有 `{0}`），其中三条正好 **13 字节**（一条记录）。这是发布方自己给的
8,647 个独立尺寸，比三个本机样本强得多。

### 字段表覆盖：有一批 id 没有名字

| 样本 | 记录 | 字段 | 有名字 | 无名字 | 表大小 |
|---|---:|---:|---:|---:|---:|
| 个股 `gpsz000001` | 30,569 | 46 | 41 | **5** | 44 |
| 市场 `gpsh999999` | 88,918 | 46 | 42 | **4** | 42 |
| 板块 `gpsh880471` | 29,748 | 15 | **15** | 0 | 15 |

超出的 id 是个股的 `45/47/48/49/50` 与市场的 `43/44/45/99`。处理方式**照参考实现返回空名字**，
本移植渲染成 `"name":null` 并同时给出 `named:false` 与文档级 `fields_unnamed` 计数。
**给未公布的 id 编一个名字，是唯一比承认不知道更糟的做法。**

### 验证

| 检验 | 结果 |
|---|---|
| 清单尺寸 × 13 字节记录 | **8,647/8,647 全部整除** |
| 现取文件端到端 | md5 与清单一致 ✓、大小一致 ✓、解析出 **27,903 条 / 46 字段**、日期到 20260924 |
| 三个本机样本 | 余数全 0，记录 30,569 / 88,918 / 29,748 |
| 筛选（字段 3 + 2024 全年） | **242 条**，日期区间**两端含**，名字来自字段表 |

**一处不匹配要正确解读**：本机缓存的 `gpsz000001.dat` 是 397,397 字节，今日清单是 362,739 字节
——**文件会每日更新**。拿旧缓存的 md5 比今日清单，比的不是"解码对不对"，而是"文件换了没有"。

### 两处比参考实现更严（有意为之）

- 参考只检查 `day <= 31`，于是 **20260231 会被接受**。本移植按**真实日历**校验（含闰年 2 月 29 日）：
  拒绝 2 月 31 日，但**接受**真实的闰日——把真实数据拒掉比放过一个不可能的日期更糟。
- 长度不是 13 的整数倍时**直接报错**，而不是忽略尾部残余：定长记录流一旦错位就是解码失败。

### 财务半边：季度 ZIP 包（`professional --zip`）

`tdxfin/gpcw.txt` 列出 **147 个季度包**，每个是一个 ZIP，内含**单个成员**例如
`gpcw20260630.dat`。成员是一张定长表：

```
头 20 字节   0 u16 版本(=1)   2 u32 报告期   6 u16 记录数
            10 u16 索引项大小(=11)   12 u32 每条记录的浮点字节数
索引          每条 11 字节：6 字节代码 + 1 字节(实测为 0) + u32 数据偏移
数据          每条记录「字节数/4」个 float
```

实测 `gpcw20260630.zip`：19,072,810 解压后 **5,570 条 × 584 字段**，且
`20 + 5570×11 + 5570×2336 = 13072810` **恰好等于成员大小、余 0**——布局的算术闭合是确认格式的依据。

| 检验 | 结果 |
|---|---|
| 归档与清单一致 | 5,750,037 字节、md5 `567b4136...` **均一致** |
| ZIP 读取 | EOCD + 中央目录 + 本地头，stored 与 deflate 两条路径，**CRC 校验** |
| **逐值对照**（Python 独立解压读取 vs 本实现） | 6 个抽样字段**全部一致，0 处不符** |
| 全市场路径 | 5,570 条营收/净利同比；`600519` 营收同比 **1.30%** |

**只命名两个字段**：参考实现只公布 `183 = 营收同比`、`184 = 净利同比`。
其余 **582 列按编号报告、不给名字**——编 582 个标签比留空更糟。

`c/tdx_zip.c` 是**够用即止**的 ZIP 读取：不加密、不分盘、只用 stored/deflate，
越界一律拒绝而不是猜；**CRC 真的校验**（"能解压出正确的长度但内容不对"正是那种会以
看似合理的数字抵达调用方、却最难发现的失败）。

### 故意不做的部分

- **HTTPS 取数**：该族走 `https://data.tdx.com.cn/`，要在 `c/` 引入 TLS，而本项目明确声明
  "部署就是单个 exe + zlib"。因此两半都保持**"外部取回、本实现解析"**，与 JSN 资源走既有传输层一致。

证据：`output/professional_verification_evidence.txt`、`output/professional_manifest_evidence.txt`、
`output/professional_finance_evidence.txt`、`output/professional_data_reconnaissance.txt`。

## 参考实现模块的范围判定（`market/` 等）

上一轮靠模块清单发现了 `professional_data`，但只处理了那一族。这一轮把 `market/` 与
`protocol/` 下其余模块**逐个按公开 API 判定**是否属于本项目声明的 L1 范围
（"只需要 L1 那一小块"），而不是按目录或文件数判断。

| 参考模块 | 内容 | 判定 |
|---|---|---|
| `market/daily.cpp` | 本地 `.day` 日线（32 字节记录） | **在范围内，待做** |
| `market/minute.cpp`、`local_kline.cpp`、`minute_download*.cpp` | 本地分钟线（`.lc1`/`.lc5`） | **在范围内，待做** |
| `market/hyzt.cpp` | 行业估值 JSN ← `command_hyzt_extract` | **在范围内，待做** |
| `market/panorama.cpp` | 全景 JSN 查询 | **在范围内，待做** |
| `market/ranking.cpp` | 分类行情/排名（走 7709 传输） | **在范围内，待做** |
| `market/seal_order.cpp` | 涨跌停规则 + 封单 | **在范围内，待做** |
| `market/valuation.cpp` | 估值 JSN 查询 | **在范围内，待做** |
| `market/level2_*.cpp`（约 50 个） | L2 SDK / tpbus | 范围外：需授权业务事件 |
| `protocol/professional_data_*` | 公开数据族 | **已交付**（交易 + 财务两半） |
| `research/`、`formula/`、`cloud/`、`trading/`、`recon/`、`institution/`、`funds/`、`industry/` 等 | 研究/机构/云/交易/扫描类工具 | 范围外：不是 L1 行情流的一部分 |

判定后的第一顺位是 **`market/daily.cpp`**：它小、可**完全离线验证**（本机 `vipdoc/` 里就是
真实的 `.day` 文件），而且第 7 轮测量除数表时**已经撞上它的致命细节**——
`.day` 的价格口径**股票是 /100、债券是 /10000**（茅台 127588 → 1275.88 元；南航转债上市首日
1102300 → 110.23 元）。这个差异如果不按品种处理，读出来的债券价格会差 100 倍，
而"看起来仍然是数字"。所以它值得做，也值得用真实文件做 fixture。

## 本地日线：`.day` 文件（`daily`）

终端自己的 `vipdoc/<市场>/lday/<市场><代码>.day`，**32 字节定长、无文件头**：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | `u32` | 日期 YYYYMMDD |
| 4 / 8 / 12 / 16 | `i32` | 开 / 高 / 低 / 收（**有符号**） |
| 20 | `f32` | 成交额 |
| 24 | `u32` | 成交量 |

### 价格口径不是常数——这是本层的核心

参考实现**无条件除以 100**：对股票对，对其他一切都错，而且**差 100 倍**。实测（与**同一只
证券的线上现价**逐类对照）：

| 品种 | 线上除数 | day 原值 | 线上价 | 比值 | `100 × 除数` |
|---|---:|---:|---:|---:|---:|
| 平安银行 | 1 | 1107 | 11.700 | 94.6 | **100** ✓ |
| 贵州茅台 | 1 | 125208 | 1255.600 | 99.7 | **100** ✓ |
| 510300 基金 | 10 | 4790 | 4.635 | **1033.4** | **1000** ✓ |
| 159915 基金 | 10 | 3925 | 3.452 | **1137.0** | **1000** ✓ |
| 南航转债 | 100 | 1086200 | 106.365 | **10212.0** | **10000** ✓ |
| 123071 转债 | 100 | 1159020 | 114.804 | **10095.6** | **10000** ✓ |
| 132026 可交换债 | 100 | 1340010 | 135.222 | **9909.7** | **10000** ✓ |
| 204001 逆回购 | 100 | 12500 | 1.455 | **8591.1** | **10000** ✓ |

（比值偏离整数倍几个百分点，是因为日线止于 20260610 而线上是当日；**量级差 10 倍的是除数类别**。）

所以口径是**线上除数表的函数**：`100 × tdx_price_divisor(code)`——**不是"股票和债券不一样"的特例**。
活体对照：`sh110075` 上市首日按此规则得 open **110.23**（债券首日就该在面值附近），
而固定 /100 会得 **11023**。测试对**同一份字节用两种口径**各解析一次，断言比值恰好 100。

### 拒绝规则是先测后定的

扫描本机 **1,130 个 `.day` 文件、4,053,117 条记录**：

- **非法日期 0 条** ⇒ 强制真实日历校验**不会拒掉任何真实数据**；而一条不是日期的数据就是
  解码错位，不是"数据有点脏"。（真实闰日 2 月 29 日**接受**，2 月 31 日拒绝。）
- **零/负价格 0 条** ⇒ 但**只计数、不拒绝**：样本里没有不等于不存在，**拒掉真实数据比如实
  报告更糟**。输出里的 `suspicious_prices` 就是这个计数。

### 验证

| 检验 | 结果 |
|---|---|
| 四类除数的口径规则 | 1/10/100 各自对上 100/1000/1000/10000 ✓ |
| 真实前缀 fixture（股票 + 债券） | 逐字段断言到小数点后 4 位，含首日那一条 |
| 同一份字节两种口径 | 比值恰好 100 ✓ |
| 日历校验 | 2 月 31 日拒绝、真实闰日接受，**且错误消息必须带上测试写的那个日期** |
| 路径定位 | 沪深北 + 扩展市场数字目录；短缓冲拒绝而非截断 |
| 渲染 | 行能被项目自己的 JSON 解析器解析（含带反斜杠的路径） |

证据：`output/daily_verification_evidence.txt`。

## 本地分钟线：`.lc1` 文件（`minute`）

终端自己的 `vipdoc/<市场>/minline/<市场><代码>.lc1`，**32 字节定长、无文件头**：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | `u16` | 日期字：`year = word/2048 + 2004`，余数给 MMDD |
| 2 | `u16` | 分钟字：`hour*60 + minute` |
| 4 / 8 / 12 / 16 | `f32` | 开 / 高 / 低 / 收 |
| 20 | `f32` | 成交额 |
| 24 | `u32` | 成交量 |
| 28 / 30 | `u16` | 两个**语义未确立**的字 |

### 与 `.day` 相邻却相反：这里**不需要**口径规则

`.day` 是缩放整数、除数按品种变（股票 100、基金 1000、债券 10000）。`.lc1` 是 `f32`，
实测与**同一只证券的线上现价**之比：股票 1.16 / 1.00、债券 1.27 / 1.10、
基金 1.02 / 0.90、指数 1.01、逆回购 0.86 —— **全部量级 1**，所以**不需要任何除数规则**。
两个模块相邻，**照搬上一轮的结论就会出错**，所以两边都单独测了。

### 参考实现那条 OHLC 校验会拒掉真实文件（本轮最重要的发现）

参考 refuse 掉 `high < close` 的记录。扫描本机 **884 个 `.lc1`、13,905,607 条记录**：

| 检查项 | 实测 |
|---|---|
| 尺寸非 32 倍数 / 日期字非法 / 分钟字非法 / 非有限值 | **各 0 条** ⇒ 这四项**强制校验** |
| **OHLC 越界** | **6 条（真实存在）** ⇒ **只计数、不拒绝** |

例：`sh000043` 的 `H 2608.770` 而 `C 2608.780`；`sh000689` 的 `L 1113.950` 而 `C 1113.960`。
**照抄参考会把终端自己写的文件判为错误。** 而且违规记录出现在 **15:00 收盘那一分钟**——
收盘集合竞价正好是它越界一个百分点的合理解释，这也正是"不该拒"的理由。

**两个独立实现对同一批文件的计数一致**：`sh000043` 1/1、`sh000689` 1/1、`sh600519` 0/0、
`sh000001` 0/0 ✓ 所以本实现的 `ohlc_violations` 不是自说自话。

### 尾部两个字：报告而**不命名**

实测 `sh600519` 全部 **16,080 条的两个字都是 0**；而 `sh000001`（上证指数）是
`731/1378`、`804/1311` 这样**逐分钟变化**的值。所以它们与品种有关，但**语义未确立**——
本实现按 `extra_1` / `extra_2` 原样输出。**给不知道的东西编一个名字，比留着数字更糟。**

### 验证

| 检验 | 结果 |
|---|---|
| 日期字解码 | `43814 → 20250806`、`44122 → 20251114`；**字 0 不是 2004-01-01**（余数 0 即月份 0），最小合法字是 `101` |
| 普通窗口（真实前缀） | 逐字段断言；股票的两个尾字均为 0 |
| **违规窗口（真实前缀）** | **仍解析成功**、`ohlc_violations == 1`、**违规记录本身被完整返回**（计数不是修正） |
| 拒绝 | 非 32 倍数、分钟字 ≥ 1440（1439 即 23:59 **接受**）、月份 0、NaN 价格、容量不足 |
| 路径定位 | 沪深北；扩展市场**主动拒绝**（分钟线不存在），短缓冲拒绝 |
| 渲染 | 行能被项目自己的 JSON 解析器解析（含带反斜杠路径与空的汇总） |

证据：`output/minute_verification_evidence.txt`。

## 行业估值：`func_gx_hyzt101_1.jsn`（`industry`）

**8,970,351 字节 / 5,567 行 / 9 列**。每行说"**一只股票属于一个行业**"，并带上该行业的估值：

| 列 | 含义 |
|---|---|
| `$ZQDM` / `$SC` | 股票 |
| `TDXHY` | 行业名 |
| `$ZQDM1` / `$SC1` | 行业指数（如 880471） |
| `hyPE` / `hyPB` | 行业市盈率 / 市净率（文本） |
| `$S_ZQDM` | 该行业的成员，**声明**为 `market\|code` 逗号表 |
| `sszt` | 逐行的题材列表 |

### 同一份资源里的两种成员描述，实测**完全一致**

`$S_ZQDM` 声明一个行业有哪些证券；行情行本身一行一只股票，**独立地实测**同一件事。
实测 **110 个行业全部一致**（880471 声明 42/实测 42、880483 26/26、880484 43/43）。

所以每行同时输出 `rows` 与 `declared` 以及 `counts_agree`——**有意思的是不一致的那一天**。
这不是自说自话：同一份文件的两个字段互相印证。

实测同时确认：**没有任何行业的行之间在 market/name/PE/PB 上不一致**（0 个）。参考实现在这种情况下
**抛错**；本实现**记录 `inconsistent` 而不拒绝**——一个行业有陈旧行不该让调用方丢掉另外 109 个。

### `sszt` 的分隔符是**顿号**，不是 ASCII 逗号

这条是 fixture 当场抓出来的：我第一版对 `$S_ZQDM` 与 `sszt` 用同一个计数函数，于是
**每一行的题材数都恰好是 1**——一个"看起来很合理"的数字。实测 **5,566/5,567 行含顿号（U+3001）**，
所以那个 bug 会**影响每一行**。现在两个列表各按自己的分隔符计数，测试里断言"**存在题材数 > 1 的行**"：
谁把顿号计数去掉，这条会立刻变成 0 并失败。

### 与参考实现的两处不同（都是有意为之）

- **不能命名两侧的行是跳过并计数，而不是抛错**（参考抛错，一行坏掉就丢掉整个文件）。
- **不一致记录而不拒绝**（同上）。测试用合成行覆盖了这两条路径。

### 未做

参考在这层之上还做**板块层级展开**（父子板块、成员并集、层级树），依赖 `tdx/blocks.hpp`
与一路 cloud 数据源。那是另一个子系统，**其范围归属尚未判定**，本轮只做行业估值这一层。

证据：`output/industry_verification_evidence.txt`。

## 涨跌停价规则：`hqrule.dat`（`limit`）

终端自己在 `T0002/hq_cache/hqrule.dat` 里放的规则（本机 **217 字节**）。费率按板块：
主板 10%、创业板 300/301 与科创板 688/689 为 20%、北交所 920 为 30%，
名字看起来像特别处理的另有一档——**但要看配置的切换日**。

### 真实规则文件与参考实现的默认值不同（这是本节最有价值的一条）

本机实测：**`SZST10Date=99991231`、`SHST10Date=99991231`**。切换日在未来 ⇒
**5% 那一档不触发，ST 股仍是 10%**。而参考实现的默认值是 `0`——用默认值会让**每只 ST 股都变成 5%**。
本实现读真实文件，所以不会。测试对同一只股票**两种规则各算一次**，把差别固定下来。

### 取整是三步，且北交所方向相反

```
增量 = trunc(费率 × 前收 × 100 + 0.503) / 100
涨停 = trunc((前收 + 增量)     × 100 + 0.503) / 100
跌停 = trunc((1 - 费率) × 前收 × 100 + 0.503) / 100
```

**增量先取整到分、再相加**——不是一次乘法。偏置 `0.503` 让截断表现为四舍五入。
中间结果**按 `float` 存储**（`store_float`）——这不是装饰：调用方拿它去比行情时必须与终端存的一致。

**北交所用的是另一套**：偏置 `0.003`（上）与 `0.997`（下），即**截断**而非四舍五入。
这是"另一个函数"，不是"同一个函数换个常数"。测试里两个取整在需要进位的值上**故意断言它们不相等**。

### 对真实行情的验证（1,200 个交易日）

涨跌停价是**硬天花板/地板**：真实数据里任何一天的最高价不得超过由前收算出的涨停价。
样本：**30 只数据延续至今的证券 × 最近 40 个交易日 = 1,200 个交易日**。

| 检验 | 结果 |
|---|---|
| 最高价突破涨停价 / 最低价跌破跌停价 | **1 例，且已查明**（见下） |
| 最高价**恰好等于**涨停价（涨停日） | **35 天（2.9%）** |
| 与**独立 Python 实现**逐值对照（同一套取整） | **30/30 一致** |

**唯一那例是除权日，不是算错**：`sz000034` 于 20260519 前收 41.57、次日开盘 29.60（**−29% 跳空**）。
29% 不是任何涨跌幅限制能产生的移动 ⇒ 交易所按**除权后参考价**计算涨跌停，
而日线文件里的"前收"是**未除权**的。**这是前提不成立，不是取整错误**；
而这也正是服务端 `0x0452` 特别限价表存在的原因——**它就是为这类日子提供官方价格的**。
所以上面那句"从不被突破"应读作：**在常规交易日上从不被突破**。

### 两次取样/容差的修正（都发生在验证过程中）

- **容差**：最初用 `1e-6`，报出 11 处"违规"，但首例的最高价与涨停价**打印值相同**。
  逐个量差值：最大超出 **1.221e-06**，**恰好等于 37.03 的 float 存储误差**
  （实测 `abs(float(37.03) − 37.03)` 同为 1.221e-06）。真正的取整错误至少 **1e-02**——
  **相差四个数量级**。容差改为 `1e-3`。
- **取样**：第一版按文件名排序取前 200 只，于是**退市股**的"最近 41 条记录"落在 **2001 年**，
  报出 9 处跌停价被跌破——那是当年 **PT 股**（无常规涨跌停）的真实成交。
  **"最后 N 条记录"不等于"最近 N 个交易日"**，加了"最后日期必须在 2026-06-01 之后"的筛选后消失。
  （与除数测量中"退市债没有报价"是**同一类陷阱**。）

### 解析教训：不要把显示当成文件

我最初用 `"[$_]"` 打印规则文件，**把方括号加到了每一行上**，于是按 `[key=value]` 写了 parser，
**在一个明明有这些键的文件里什么也没找到**。真实格式是普通 INI：`[节名]` 段落头 + **裸 `key=value`**。
读**字节**而不是读显示，才定下这件事。

### 未做

参考实现的**封单量/封单比**（`calculate_seal_order`：由买卖档位、成交单位、竞价不平衡量算封单金额与占比）
尚未移植——它需要五档盘口与竞价不平衡量，属于另一片数据。

证据：`output/limit_verification_evidence.txt`。

## 指数估值：`valuation`

四份资源，实测：

| 资源 | 字节 | 行 | 内容 |
|---|---:|---:|---|
| `list/func_zsgz101_1.jsn` | 2,407 | 11 | 当前表：指数 + 指标 + 收益 |
| `zsgz3/<detail>.jsn` | 101,898 | 3,093 | PE 历史 |
| `zsgz4/<detail>.jsn` | 98,524 | 3,093 | PB 历史 |
| `zsgz1/<detail>.jsn` | 497 | 6 | 跟踪该指数的基金 |

明细资源的路径是**主表自己的 `$ZQDM` 列**给的，且与 `bi` 根之间**没有 `list/`**：
`bi/zsgz3/000001.jsn`。逐一实测的主要指标：PE、PE 分位、PB、PB 分位、股息率、ROE、
盈利收益率、**服务端自己的估值判断（文本）**、以及 5/10/20/30 日收益。

### 两条历史按日期合并：**完全对齐**

```
points 3093，both_sides 3093，pe_only 0，pb_only 0，complete true
```

**3,093 个日期全部同时在两侧** ⇒ 这两份历史确实是**同一条序列**，而不是"行数相同所以大概对得上"。
合并报告把这件事**说出来**而不是假设：哪些日期两侧都有、哪一侧多出日期，都计数并输出。
测试另外用合成行覆盖了"只有一侧有"与"输入乱序"两种真实抓包不保证含有的情形。

### 跨资源一致性：主表的当前值 == 历史的最后一点

| 来源 | 日期 | PE | PB | PE 分位 | PB 分位 |
|---|---|---|---|---:|---:|
| 主表 | 20260921 | 15.9510 | 1.3583 | 79.1082 | 77.4566 |
| PE/PB 历史最后一点 | 20260921 | 15.9510 | 1.3583 | 79.1082 | 77.4566 |

**四值全同**——两份独立的资源在同一日期上给出同一组数。这是本层**最省事也最有说服力**的一条自洽检验。

### 活体规模

`11 指数 + 1 合并报告 + 3093 历史点 + 6 基金 + 1 汇总 = 3112 行`。

### 本轮我自己的三个错误

1. 明细资源路径**少了 `tdx_jsn_remote_path` 那一步**（`tdx_download_resource` 要的是已带前缀的路径，
   别的命令都先转换），服务器回"零长度"。
2. 路径我按参考的写法**加了 `list/`**，实际没有；三份都取回空。
3. **渲染器第 7、8 次漏右括号**：`format_point` 与 `format_fund` 都写完了成员却没关闭对象。
   第一个被**解析断言**抓到；第二个的教训更值得记——**断言只在被应用的地方起作用**，
   这个测试对 4 个渲染器里的 3 个做了断言，而漏掉的正是出错的那个。
   **活体运行的独立 JSON 解析**（证据脚本）才把它翻出来：**所以活体运行不是可选项。**

### 有意不做的部分

参考在这层还做了**缓存**（主表 300 秒、明细 900 秒 TTL）与多资源失败容忍。
本实现每次直取，与项目里其它命令一致；缓存属于服务形态，不属于解析层。

证据：`output/valuation_verification_evidence.txt`。

## 分类行情排名：`0x054B`（`ranking`）

一条**新的协议命令**：按某个排序键给一个分类的证券排名，服务端分页。

### 请求与记录形状

请求体 **18 字节 = 9 个 u16**：`category, sort, start, count, reverse, 5, filter_raw, 1, 0`，
其中 **`reverse = sort==0 ? 0 : (升序 ? 2 : 1)`**——**降序是默认，它不是一个布尔标志**。

记录**既不是定长也不是纯 varint**：

```
0        u8      市场 0..2
1..6     char[6] 六位数字代码
7        u16     active1
9        九个 varint：close，然后是【相对它的四个差值】、服务器时间、一个原始价、两个手数
之后      u32     成交额（wire 自身的缩放形式）
之后      八个 varint：内盘、外盘、一个原始值、开盘金额（×100 元）、买卖一价（**也是差值**）及其量
之后      56 字节定长尾：涨速、短换手、两个 f32、**两段未建模字节**、active2
```

### 五个价格是**相对收盘价的差值**

`close` 是绝对值，**前收/开/高/低四个都是 `close + Δ`**。把每个 varint 当绝对价格读，
会**得到一个对、四个错——而且看起来仍然像价格**。varint 是**符号-数值**编码
（首字节 `0x40` 位是符号，与参考实现逐字相同），所以前收高于现价（下跌）也能正确表示。

价格口径复用线上除数表：`raw / 100 / tdx_price_divisor(code)`。

### 排序自洽：整页的**推导涨幅必须单调不增**

记录**不带涨幅字段**，所以涨幅只能由两个价格推出。实测 **80 条整页，单调性 0 处破坏**：

| 排名 | 证券 | 现价 | 推导涨幅 |
|---:|---|---:|---:|
| 1 | SZ301686 | 288.00 | **+420.98%** |
| 2 | BJ920229 | 67.00 | **+327.57%** |
| 3 | BJ920526 | 26.13 | **+29.94%** |
| 4 | BJ920427 | 14.10 | **+27.83%** |
| 5 | SZ300110 | 3.95 | **+20.06%** |
| … | … | … | … |
| 79 | SZ002467 | 5.09 | **+9.94%** |
| 80 | SZ000607 | 4.98 | **+9.93%** |

**单调不增说明"排序"与"差值解码"同时对**：差值读错的话，推出的涨幅会变成一堆无意义的数，
不可能恰好递减。

而且**这张表本身是跨层印证**：尾部正好落在 **9.93–10.0%**、中间出现 **20.06%** 与 **29.94%**——
正是第 15 轮 `limit` 模块算出的**三档涨跌停**；顶部的 +421%/+327% 是**新股上市首日**
（创业板/北交所首日无涨跌幅限制，可以涨几倍）。**把它当 bug 的人，是默认了 10% 或 20% 的涨跌幅。**

### 有意保留原样

尾偏移 12 的 10 字节与偏移 30 的 24 字节**语义未确立**，按 hex 原样输出
（`extra_pair_hex` / `extra_meta_hex`）——与 L1 命令里那两段未建模尾部同样处理。

### 三个自己的错误

1. **includes 加错锚点**：`main.c` 不 include `tdx_quote.h`（它按模块逐个 include），
   于是头文件根本没加进去，整片编译失败。
2. **`--category` 已被 `securities` 占用**（`all|a_share|etf|index`，默认 `a_share`），
   我的分类解析一开始拒了这个默认值，于是命令直接不可用；改为接受该目录分类的拼写
   （`a_share` → 6），不再多出一个选项。
3. 测试里我一度想手写 varint 字节；改为**由辅助函数按数值生成**
   （连续三轮栽在手写二进制字段上之后的做法）。

证据：`output/ranking_verification_evidence.txt`。

## 封单：`seal`（`0x0547` 盘口 + 涨跌停规则）

一只证券是否**被封在板上**、封了多少。三条路径：

| 模式 | 情形 | 封单手数 |
|---|---|---|
| `auction-imbalance` | 开盘前**无现价**、两侧在同一价位 | **竞价不平衡量** |
| `auction-second-level` | 同样是交叉价，但**只有二档有量** | 二档量，且**一档量并入分母** |
| `continuous` | 盘中现价即限价，且**对手方一档不存在** | 一档量 |

### "封住"的定义是**对手方一档不存在**，不是价格接近限价

这条最值得写下来：**两侧都有挂单就不是封板，无论价格离限价多近**。测试对此**专门断言**——
把现价设成恰好等于涨停价、同时给卖一挂上量，结果必须是 `not-sealed`。

### 金额与封单比**带符号**

涨停为正、跌停为负，所以调用方可以**直接按封单额排序**而不必先问方向。
分母是成交量（`auction-second-level` 时再加上一档量）——那部分量也是"封单相对什么而言"的一部分。

### 对真实盘口的验证

走查 **96 只证券**的 0x0547 盘口：**1 只被封住**，95 只未封住。对每一个"被封住"的**重新独立推导**三件事：

| 检验 | 结果 |
|---|---|
| ① 现价是否真的等于它声称的那个限价 | **0 处不符** |
| ② 金额是否真的等于 价 × 封单手数 × 每手股数 | **0 处不符** |
| ③ 封单比是否真的等于 封单手数 / 分母 | **0 处不符** |

那一只是 **`000504`，continuous 涨停**：

| 现价 | 上限 | 封单手数 | 封单金额 | 封单比 |
|---:|---:|---:|---:|---:|
| 12.1100 | **12.1100** | 410,742 | **497,408,562 元** | **7.36** |

封单比 7.36 意味着封单量是当日成交量的 7.36 倍——强封板，数值合理；而现价与上限**完全相同**，
说明"在板上"这件事本身被独立确认了。

### 格式里一个反直觉之处（测试纠正了我的预期）

`auction-second-level` 分支的条件是**二档的【价格】不存在、而【量】存在**：

```c
!present(bid2.price) && !present(ask2.price)   /* 价格缺席 */
if (bid2.volume_hand && ...)                   /* 量却在 */
```

也就是说**竞价盘口会把"量有、价无"的二档报出来**。我第一版测试给二档设了价格，于是整条分支
不成立、10 条断言失败——**实现是对的，错的是我的预期**。修好之后我把"给二档设价格会跳出这条分支"
**变成了显式断言**。

同一轮里我另外两处预期也写错了：

- 封单比 `410742 / 55812 = 7.3594`，而我填的是**活体记录里另一个分母**算出的 7.3579；
- 拒绝状态掩码 `(flags & 0x3C) == 0x1C` **保留四位再比较**，所以 `0x3C & 0x3C = 0x3C` **不是**拒绝状态，
  而 `0x5C` 是——我原以为"任何拒绝标志都算"。

### 活体未覆盖的部分

活体取样只命中 `continuous` 与 `not-sealed`；`auction-imbalance` 与 `auction-second-level`
由测试用**构造输入**覆盖（它们只出现在开盘前那几分钟）。

### 未做的部分

参考实现另有 `all_sealed` 多页扫描（按封单额排序翻十页）与板块聚合，属于**服务形态**而非计算层。

证据：`output/seal_verification_evidence.txt`。

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

**批次数实测已是最小**（`/status` 的 `last_round_batches`）：

| 情形 | 被轮询 | 批次数 | 理论最小值 |
|---|---:|---:|---:|
| 全量（无订阅者） | 2320 | **24** | `ceil(2320/100)` = 24 ✓ |
| 订阅 3 只分散证券 | 3 | **1** | `ceil(3/100)` = 1 ✓ |

同时实测：`batches == upstream_requests`（没有批次外的重复请求），
且首轮之后 `sessions=0`（**连接池复用会话**，只有 1 个批次时也不会开 6 条）。
单轮耗时由全量首轮的 2335 ms 降到订阅 3 只时的 **26 ms**。
所以"批次划分"这一层**没有余量可压**，能省的只有"要不要轮询这只票"。

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
| `test_hub` | 首轮快照、相同轮静默、变化掩码、过滤订阅、未知代码、队列丢弃、失败计数、快照 JSON、分层梯子 |；**status 必须转述 fetcher 报告的批次数**（假 fetcher 按 `ceil(count/10)` 报告，断言 hub 不会自己编一个、也不会在一轮确实取过数之后报 0）
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
| `test_pending` | **16 列小写列名**的真实抓包前三行映射、两类中文取值精确比对、`gdpsl` 列存在但为空渲染为 `null` 而非 0、短代码/非数字市场**跳过并计数**（与上市视图"直接报错"相反）、容量不足的拒绝、集合对账的六种情形（顺序不同仍相等 / 两侧各有差异 / 重复身份折叠 / 空身份不算证券 / 空投影 / 两个空集）、渲染括号平衡 |
| `test_subscription` | 两个派生指标在抓包数字上的取值、**除法保护**（转股价 0 / −0 / 无穷、转股价值 0 / 无穷、NULL 输出）、**16 列**真实抓包前三行的映射、三类中文名精确比对、事件 id 拼法与缺日期时的尾冒号、债券或正股身份任一不可读即**跳过并计数**、身份齐全但输入全空时事件仍成立且两个指标**缺失而非 0**、渲染括号平衡 |
| `test_bond_math` | **日序数与 Python datetime 对照**（含闰日、整百年、跨年、带连字符）、日期非法/短/空/NULL 的拒绝、CSV 文本与数值解析（跳过非数字与空项、容量不足截断）、应计利息取**第一项**利率与付息日当天为 0、期外拒绝、**最后一笔同时偿还面值**（103 而非 3）、现金流按时间升序、**单笔现金流的闭式解**、**贴现回代的不动点**、利率 ≤ −100%、空现金流、零/负价格、**区间夹不住的价格**的拒绝 |
| `test_pricing` | **43 列**真实抓包按"每种形态取第一只"选取的三行（普通转债 / 仅剩一期 / 可交换债）、触发价 = 转股价 × 比例、票息表数组化、**应计利息与全价在抓包窗口上的精确取值**、转股价值与溢价率、**到期收益率为负时符号正确**（防后人"修"它）、报价**现价优先/昨收兜底**且来源随价格输出、无报价时退化为 terms-only 而条款仍在、只有正股报价时转股价值**仍然有值**（它不需要债券价）、**>10 倍面值被标注 price_plausible 而数字仍保留**、738 元真实高价**不被误标**、容量不足的拒绝、渲染括号平衡 |
| `test_newbond` | 日期压缩（带星期/带连字符/已紧凑/非八位/空/NULL）、**20 列**投影抓包的映射（代码 + 标的 + 原始与压缩日期并存）、**13/13 按代码匹配而 0 处日期不一致**（去掉压缩立刻变 13，断言期望 0）、**2 处规模不一致且较大者为 9.80 亿**、合成用例覆盖"同代码多行由标的择优"与"代码无果才兜底"、未匹配行的代码上报、完全一致时为 exact、**每一行都断言语义一致性**（匹配到的申购行标的 == 投影标的）、全部 13 行渲染括号平衡 |
| `test_professional` | 三张字段表的大小与**表外 id 返回空名字**、**真实 .dat 前缀**（个股 + 板块）逐字节解析并断言取值（含 f32 位精确比对）、**长度非 13 倍数 / 2 月 31 日 / 月 13 一律拒绝**而真实闰日接受、无日期记录在区间内被排除但不带区间时保留、筛选（按 id、含两端、单边界、无匹配、容量不足）、**未命名 id 渲染 `name:null`**、非有限值渲染 `null`、渲染括号平衡 |
| `test_professional_finance` | ZIP 由 **Python 的 zipfile 写、由本代码读**（stored 与 deflate 两条路径）、**破坏 CRC 必须被拒绝**、缺 EOCD 被拒绝、定长表头/索引/数据起点逐字段断言、**字段取值按 `index` 与 `index×2` 设计**使偏移错位必然暴露、越界字段为缺失而非 0、成员结构错误的四种拒绝、只命名两个字段、**渲染行必须能被项目自己的 JSON 解析器解析**（这条抓出了两个真 bug） |
| `test_daily` | **四类除数**的口径函数（股票/基金/债券/逆回购/未知码）、真实 `.day` 前缀（股票 + 债券）逐字段断言、**同一份字节两种口径比值恰好 100**（这就是参考实现会犯的那个 100 倍错）、月份 13 / 2 月 31 日拒绝而**真实闰日接受**、**拒绝时错误消息必须带上测试写的日期**（防"通过但没测到"）、零价格**只计数不拒绝**、非 32 倍数与容量不足拒绝、四种市场路径定位与短缓冲拒绝、渲染可被项目自己的解析器解析 |
| `test_minute` | 日期字解码（含**字 0 不是日期**、最小合法字 101）、真实 `.lc1` 普通窗口逐字段断言与**股票尾字为 0**、**真实违规窗口仍能解析且恰好计 1 条**（违规记录本身完整返回，计数不是修正）、分钟字 ≥ 1440 拒绝而 1439（23:59）接受、月份 0 拒绝**且错误消息带上测试写的字**、NaN 价格拒绝、容量不足拒绝、扩展市场主动拒绝、渲染可被项目自己的解析器解析 |
| `test_industry` | **保留整行业的 fixture**（前缀会把行业切半，让"声明==实测"在 fixture 里失败而在真实文件上成立）、三个行业的两种计数**逐一对上**、**存在题材数 > 1 的行**（顿号计数去掉即失败）、负市盈率**按数字解析而非当作缺失**、缺码的行跳过并计数、**同类不一致记录而不拒绝**（合成行）、渲染可被项目自己的解析器解析、缺行业名渲染 null |
| `test_limit` | **真实 hqrule.dat 的字节**（`[节名]` + 裸 `key=value`、注释、空行）、切换日 99991231 与默认值 0 的差别（**同一只 ST 股两种规则各算一次**）、三个板块的费率、**三步取整与 0.503 偏置**（需要进位的值上断言）、**北交所截断与主板四舍五入在同一个值上不相等**、非限价品种/无前收/`N` 开头新股的拒绝、规则表为 NULL 时回落到默认 |
| `test_valuation` | **整份主表**（11 行全断言）、标签按 **UTF-8 字节精确比对**（只查"非空"会放过乱码）、四个收益字段、每行都带明细 id、**真实 PE/PB 前缀按日期合并**（20 点全部两侧都有、值来自两个资源、升序）、合成行覆盖 **pe_only / pb_only / 输入乱序 / 单侧缺失**、基金字段（净值/溢价/规模/类型）、**四个渲染器全部断言可解析**（漏掉的那个正是出错的） |
| `test_ranking` | 请求体 9 个字段**逐个断言**（含 `reverse` 的 0/1/2 三态，**降序是默认而非标志**）、页大小 1..80 的拒绝、排序键表（名称/大小写/十进制/十六进制/未知名）、分类拼写（`a-shares`/`a_share`/数字）、**由测试构造的报文**（varint 由辅助函数按数值生成，不手写）断言**前收高于现价的下跌情形**、买卖一价同样是差值、负的涨速/开盘抢筹、两段未建模字节按 hex、拒绝：超 80 条、**尾部多余字节**、市场越界、代码非数字、尾部截断、容量不足、渲染可被项目自己的解析器解析 |
| `test_seal` | 三条路径**各自构造**（活体只在盘中命中其中一条）：**连续涨停**（现价==上限且卖一不存在 ⇒ 封单=买一量、金额 497,408,562、比值 7.359385）、**跌停方向**（金额与比值**为负**）、**两侧都有挂单即使现价恰在限价也不封**、竞价不平衡（无现价、两侧交叉，比值为 null 而非 0）、**竞价二档**（二档**价缺席而量存在**、一档量并入分母；**给二档设价格即跳出该分支**）、拒绝状态掩码 `0x3C`**不是**、`0x5C`**是**、限价不可用/每手为 0/无成交时各字段的可用性、渲染可被项目自己的解析器解析且输入随结果一起输出 |

**每个渲染测试都要求输出能被项目自己的 JSON 解析器解析**（`c/tests/render_check.h`），
而不只是括号平衡。这一条是财务包那一轮加的，**当场抓出两个真 bug**：Windows 绝对路径经
`%s` 进 JSON 产生 `\U`（根本不是 JSON 转义），以及 CRC 以 `%08x` 裸输出使
`"member_crc":9c4fb1bf` 无法解析（JSON 数字不能以字母开头）。两行的括号都是平衡的。
现已覆盖**全部 15 个渲染测试**（8 个用内联括号计数、7 个用本地 `braces_balanced` 辅助）。

`test_pool` 与 `test_hub` 都不碰公网：前者自建回环 7709 服务器，后者注入
确定性 feed。`test_zst` 在不存在的样本目录上会 `skip:` 并以 0 退出；
`test_download` 完全不联网（传输链路的活体验证放在
`output/zst_transfer_evidence.txt`）。

## 尚未完成

### 需要本机之外的条件

1. **真服务端推送（B 方案）**：`FastHQ.Subscribe` 需要已登录的 tpbus/TaApi 会话，
   且推送帧格式尚未恢复。连"L1 有没有服务端推送"都还没证实——需要一次客户端被动观察。
2. **Ctrl+C 的真实验证**：优雅退出代码路径完整，但只在脱离控制台的进程上测过
   （Windows 走的是控制台关闭事件 `0xC000013A`，不经过本处理器）。需要在前台控制台手按一次。
3. **指定日期分时 `0x0FB4`**：请求格式还没复原。范围已收窄到"11 字节体被识别、那 4 个
   尾部字节不是 YYYYMMDD 日期"，探测日志与下一步假设见 `output/history_timeline_recovery_log.txt`。
   最省事的收尾办法是从 TdxW 里直接读出它自己的调用点，而不是继续盲试。
4. **两条 L1 命令各留一大段未建模字节**：`0x0547` 的 46 字节、`0x054C` 的 61–62 字节尾部。
   本实现只报数量、不编语义；要标定它们需要另找消费者证据（TdxW 侧或对照物）。
5. **L2 秒级逐笔成交 / 逐笔委托**：交易所口径的那个，走内置 `1364`/`1374` 或 SDK
   `4655`/`1801`/`1802`，需要授权业务事件。本仓库只做到结构确认与被动探针，`c/` 侧没做
   ——公开会话拿不到，做了也无法验证。
6. **`0x0010` 里三个未标定的股本类别槽位**（national / promoter_legal_person / legal_person）：
   实测在工行、茅台身上给出不可能是股本的数值，需要另找消费者证据。
7. **`1i..1m` 阶段辅助块还没有语义**。
8. **历史 `.img` 的公网取数被挡**：公开节点一律报长度 0；需要恢复带权益会话的握手/口令
   链路，或继续依赖客户端自己的缓存。

### 已测量但只覆盖了一部分

9. **除数表 `13` 族只标定了 `132`**：`132xxx` 已测量并修正（`{"132", 100}`），
   `130`/`131`/`133..139` 在本机**没有可测代码**（13 族 23 个代码全是 `132xxx`，
   且只有 2 只仍在交易），因此保持未测量状态，由 `price_plausible` 兜底。
   要补全需要另一台装有更多债券日线的机器，或另一个数据源。

### 本轮清单对照发现的遗漏（在范围内，尚未移植）

11. **`professional_data` 的 HTTPS 取数**：解析两半都已交付（见"公开数据族"一节），
    但取数需要 TLS，会打破"部署就是单个 exe + zlib"，因此维持"外部取回、本实现解析"。
12. **`market/` 下其余在范围内的模块**：`daily`、`minute`、`hyzt`、`valuation`、`ranking`、
    以及 `seal_order` **整个模块**（涨跌停规则 + 封单量/封单比）已交付；
    还剩——`panorama` 与 `minute_download*`（分钟线的下载与展开）。
13. **板块层级展开**：`hyzt` 之上还有一层父子板块/成员并集/层级树，依赖
    `tdx/blocks.hpp` 与一路 cloud 数据源；**其范围归属尚未判定**。

### 性能：已测量，无余量

10. **批次数实测已是最小**（见"调度"一节）：全量 24 批 = `ceil(2320/100)`、
    订阅 3 只时 1 批 = `ceil(3/100)`，且 `batches == upstream_requests`、
    首轮后不新开会话。原本这条写的是"批次边界可以更紧凑"——那是**未验证的说法**，
    实测后确认没有余量，可省的只有"要不要轮询这只票"。

### 是事实，不是缺口（用的时候注意口径）

11. `.img` 的累计字段止于 15:00，而成交明细含盘后固定价格成交（`status=5`）。
    两者口径不同是**已验证的事实**；用的时候按 `time` 或 `status` 切。
12. `132xxx` 的价格比其他债券段多一层历史包袱（见"可转债定价视图"一节），
    现在解码正确，但 `price_plausible` 标注保留为通用安全网。
