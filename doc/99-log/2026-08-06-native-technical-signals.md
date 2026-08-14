# 通达信技术选股页纯 C++ 固定化

## 动机

`cloud pbrpc` 已能执行全部启用的 reqformat=22 模板，但调用方仍需知道 ReqId、
XML 占位符、板块分支和原始列名。以下通达信页面已有真实返回证据，却还不是
稳定业务接口：九转、RPS、创新高低、横盘突破、强势启动、趋势通道和事件驱动。
本轮把它们合并为纯 C++ `market technical-signals` 和固定只读 API，运行时不
调用 Python，也不转发脚本。

## 视图与原始请求

| view | ReqId | 客户端配置 | 默认参数 |
|---|---:|---|---|
| `nine-turn` | 200451 | `func_sqjz101.xml` | 上涨/下跌由 `NTType` 过滤 |
| `rps-stock` | 200452 | `gp_gz_rpsxg.xml` | 10日/90、20日/90、60日/90 |
| `rps-block` | 200453 | `gp_gz_rpsxg.xml` | 10日/85、20日/85、60日/85 |
| `new-high/new-low` | 200327 | `gp_gz_xgxd.xml` | 2日、120日历史、5% 回撤 |
| `breakout` | 200329 | `gp_gz_xgxd.xml` | 横盘20日、振幅10%、突破2日 |
| `strong-start` | 200316 | `gp_gz_xgxd.xml` | 客户端固定请求 |
| `trend-up` | 200320 | `gp_gz_SSTD.xml` | 主板/创业板/科创板/北证分支 |
| `trend-down` | 200325 | `gp_gz_cdgg.xml` | 同上 |
| `event-driven` | 200225 | `tdxxgcl6.xml` | 客户端固定请求 |

同文件还存在七个固定 `200250 / XgName` 模型，本轮后续审计也已纳入：

| view | XgName | 客户端名称 |
|---|---|---|
| `model-new-high` | `CXGXG` | 创新高 |
| `two-day-event` | `SJQUXG_QLR` | 前两日事件驱动 |
| `liquidity-space` | `LDXKJXG` | 流动空间 |
| `limit-break` | `ZTKBXG` | 涨停开板 |
| `low-turnover-chase` | `FLOW_UP_LOW_XG` | 低换手追涨 |
| `high-turnover-chase` | `FLOW_UP_HIGH_XG` | 高换手追涨 |
| `high-liquidity-enhance` | `GLDXZQ_XG` | 高流动性增强 |

七类统一返回代码、名称、市场、涨幅、开盘涨幅、实际换手率、成交额、实际
流通市值和入选时间。XML tooltip 披露了前六类筛选条件；高流动性增强没有规则
正文，因此业务响应保留 `client_rule_disclosed=false`，不根据名称反推算法。

接口把日期转为 `YYYY-MM-DD`、时间转为 `HH:MM:SS`，去除百分号和千分位后输出
数值，并把支撑/压力、距支撑/压力、通道持续、起始涨跌、安全分等字段按业务名
返回。个股通过本地证券目录补名称，板块 RPS 通过本地多级板块树补 `block_id`、
family 和名称。

## 客户端过滤

服务默认复现 XML 的界面级过滤，而不是把服务端全集直接冒充页面结果：

- 创新高低、横盘突破、强势启动：安全分不低于 60；
- 上涨通道：持续天数非零、起始至今涨幅大于零、支撑位非零、综合安全分
  不低于 60；
- 下跌通道：持续天数不低于 50；
- 九转：按 `direction=up/down/all` 过滤 `NTType`。

`--raw` 或 API 的 `raw=1` 只跳过这些界面过滤，仍执行字段规范化。返回同时给出
`upstream_rows`、`normalization_skipped`、`client_filtered_out`、过滤后数量和
截断状态，便于核对语义。

## 当前样本

2026-08-06 使用当前客户端配置重新请求，原始集合分别为：九转 515、个股 RPS
54、板块 RPS 2、创新高 22、创新低 6、横盘突破 2、强势启动 14、上涨通道
108、下跌通道 1,167、事件驱动 79。样本保存在
`output/probes/pbrpc-signal-*.json`。PBRPC 服务器在紧接着的并发复验阶段短暂
返回 HTTP 503；新业务服务只对 429/502/503/504 做三次短退避，不重试业务性的
`RpcID=-1`。

## 接口

```powershell
tdx-tool market technical-signals --root C:\new_tdx `
  --view new-high --index-period 2 --history-period 120 --retracement 5

tdx-tool market technical-signals --root C:\new_tdx `
  --view trend-up --board main
```

HTTP 路径为 `/api/v1/market/technical-signals`，查询参数使用下划线形式，例如
`view=rps-stock&duration1=10&rps1=90`。常驻服务按完整参数组合缓存 15 秒；结果
只保留固定请求编号、配置文件、模块、RpcID、轮次和大小，不暴露任意上游地址
或原始请求转发能力。

真实 API 复验中，`nine-turn&direction=down&limit=100` 首次请求约 749 ms，
返回 515 条上游记录中的 12 条下跌九转；相同参数在缓存期内约 2 ms。返回中的
`availability` 明确区分 `live` 与 `stale-cache`，并附带缓存命中、年龄和上游
错误信息。首次访问若遇到不可恢复的上游故障，HTTP 使用 503 和
`upstream_unavailable`，不会把服务端异常误报为用户参数错误；过期缓存存在且
上游发生可重试故障时，才返回带来源标记的旧快照。

当前业务命令探针还验证了：板块 RPS 返回 `881207 电子商务` 并映射本地
`research-industry:X2506`；创新高默认安全分过滤由 15 条收敛为 14 条；上涨
通道由 138 条收敛为 127 条。集合会随交易日及盘中状态变化，探针只作为协议、
字段和过滤规则的回归证据，不作为固定业务基线。

## 单票反查

```powershell
tdx-tool market technical-signals --root C:\new_tdx `
  --market sz --code 000001
```

反查顺序现检查全部三十二个业务视图。三十一类个股页直接按 `SZ/SH/BJ+代码`
匹配；
`rps-block` 按本地多级板块成员关系把入选板块关联回股票。趋势通道会根据代码
自动选择主板、创业板、科创板或北证分支。结果逐视图保留状态和错误：只有全部
页面均为实时、未截断时，`complete` 才为真；存在错误或旧缓存时，未命中不能
被解释为确切缺席。

平安银行真实探针耗时 10.724 秒，九个页面成功，命中 2026-08-03 的上涨九转；
个股 RPS 当次返回业务性 `RpcID -1`，因此总结果为 `partial` 且
`absence_conclusive=false`。探针保存为
`output/probes/technical-signals-security-SZ000001.json`。

部署后的常驻 API 再次请求时十个页面全部成功，`availability=live`、
`complete=true`，首次聚合约 9.113 秒；相同参数再次访问约 39.9 ms。该结果也
证明 `RpcID -1` 是动态上游状态而非本地证券适配错误，同时验证了逐页失败隔离
和后续恢复语义。

## 七类模型策略复验

2026-08-06 再次逐页请求，规范化结果为：创新高 1、前两日事件驱动 526、
流动空间 42、涨停开板 0、低换手追涨 7、高换手追涨 6、高流动性增强 42。
零行是成功的 `ErrorCode=0` 空集合，不是请求失败。首批样本的入选时间均恢复为
`HH:MM:SS`；原始和业务探针分别保存在 `output/probes/pbrpc-200250-*.json` 与
`output/probes/technical-signals-<view>.json`。

首轮扩展后的平安银行单票反查约 10.080 秒完成当时的十七个视图，17/17
成功、状态为 `live/complete`。七类竞价策略加入后，最新探针约 13.990 秒
完成二十四个视图，24/24 成功、无旧缓存，当前仍只命中上涨九转。最新探针
保存为 `output/probes/technical-signals-security-SZ000001-24views.json`。

部署后的高流动性增强 API 首次约 698.0 ms、缓存命中约 25.4 ms，返回 42 条
并保持 `client_rule_disclosed=false`。当时常驻服务中的十七视图单票首次聚合
约 10.560 秒，相同参数再次访问约 53.1 ms；17/17 成功，首页 HTTP 200。

## 七类竞价策略扩展

`sc_jjcl.xml / 200400..200406` 已进一步并入同一固定接口，视图和中文标题见
[竞价策略专项记录](2026-08-06-native-auction-signals.md)。这次扩展把总数由
十七增加到二十四；单票完整性判断、快照差异和逐页故障隔离语义保持不变。

## 快照差异

```powershell
tdx-tool market technical-signals --root C:\new_tdx --view nine-turn `
  --snapshot output\state\technical-signals-nine-turn.json
```

首次执行创建状态，后续执行先校验 schema、视图和全部业务参数，再按证券或
板块稳定 ID 输出 `added`、`removed`、`changed` 和 `unchanged`，最后原子替换
状态文件。不同参数误用同一文件会直接拒绝，旧状态保持不变。连续两次真实九转
快照均为 515 条，第二次得到新增 0、消失 0、变化 0、不变 515。

## 200661/200662 盘中信号扩展

同一 `gp_gz_dxjh.xml` 的个股/T+0 机会和积突、高低、上下、破立、踩拉、
托压八个 TQLEX 视图已继续并入，总数由二十四增至三十二。字段、客户端过滤、
可选 L1 行情增强、同股积/突双事件和真实数量详见
[盘中机会与六类因子信号专项记录](2026-08-06-native-intraday-factor-signals.md)。
