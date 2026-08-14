# 公开 L1 持久会话与本地 SSE

## 结论

无需账号的公开 L1 链路已从“每轮重新连接、分别读取快照和五档”升级为纯 C++
持久会话。统一工具用 `0x0547` 一次取得证券快照与买卖五档，本地服务把所有
浏览器订阅合并为去重证券集合，再通过 SSE 分发 `snapshot/change/heartbeat/
missing/reconnect` 事件。上游仍是 7709 公共行情的有界轮询；只有本地服务到
浏览器这一段是推送，未调用需要已登录 tpbus/TaApi `CTAJob_InetTQL` 会话的
`FastHQ.Subscribe`。

## 原生实现

- `MarketL1Session` 在多轮和多批请求间保留同一条 `QuoteConnection`，并报告
  `connection_generation/connections_opened/upstream_requests/successful_polls`；
- `market watch` 改用 `0x0547`，JSONL 模式升级为
  `tdx-market-l1-watch-event-v2`，保留端点切换和指数退避；
- `MarketStreamHub` 维护共享工作线程，把活动订阅合并后一次批量请求，再按证券
  扇出；每个订阅队列上限 32 条，积压时丢弃最旧事件并报告投递健康；
- 新订阅者可立即重放最后快照；上游失败会发 `reconnect`，随后按最大 30 秒
  指数退避重建会话；
- `GET /api/v1/market/stream` 提供 SSE，
  `GET /api/v1/market/stream/status` 提供连接、订阅、请求和权限边界状态；
- SSE 连接交给独立处理线程，长连接不会阻塞现有普通 HTTP 请求。普通 API 仍
  保持原来的串行模型，避免把已有非线程安全缓存无意改成并发访问。

真实 CLI 证据 `output/probes/market-watch-persistent-current.jsonl` 连续三轮只打开
一条连接：第一轮 `connection_reused=false/connections_opened=1`，后两轮均复用
`connection_generation=1`，累计 `upstream_requests=3`。

两个并发 SSE 客户端同时订阅平安银行时，状态为 `subscriber_count=2`、
`active_security_count=1`、`connections_opened=1`；两端都收到快照/心跳，且并行
`/health` 在 101 ms 内返回。证据为
`output/probes/market-stream-client-1.sse` 与
`output/probes/market-stream-client-2.sse`。

## 频率与网页行为

共享轮询在沪深交易日的 09:15—11:35、12:55—15:35 默认每秒一次，盘前后缓冲
覆盖集合竞价和收盘附近状态；其他时间降为 15 秒，避免网页长期开启后盘后仍持续
压测公共服务器。状态接口显式返回 `exchange_session_active` 与
`effective_interval_ms`。

Svelte 个股工作台先用普通深度接口取得首屏，此后由 `EventSource` 接收共享
SSE 并更新行情和五档；连接异常时才启用 15 秒兜底轮询。K 线不会随每个报价
事件重拉：仅当交易时段的交易所分钟发生变化时，用 `0x052D` 拉取最新页并合并
到现有历史序列，因此保留用户缩放视口与已经加载的历史。

## 验证与部署

- 新增确定性 `tdx-market-stream-tests`，覆盖首快照、变化、迟到订阅重放、重复
  订阅去重、失败重连、恢复和 SSE 帧；全量 CTest 45/45；
- Svelte `npm run check` 为 0 error/0 warning，生产构建通过；
- 临时真实 `market-stream-live` 契约 1/1；正式完整契约 27/27，证据为
  `output/probes/api-contracts-official-current.json`；
- 正式服务 PID `31624`，命令行为
  `dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`；
- 部署 EXE SHA-256：
  `A877F0686FBCC012F487EAA23B970FB7201DF68699E20FD3067B079C25C401DB`；
- 功能目录 91 项，运行时仍为纯 C++；当前盘后状态实测
  `market_hours_throttle=true/exchange_session_active=false/effective_interval_ms=15000`；
- 发布 HTML、JS、CSS SHA-256 分别为
  `7C65BE586AE0744C201F857B9837A6525732B74D0F2D6A0BDDF001ACB632F534`、
  `9DD7BA36591D8D44D6B480CEB74159F08C18D8F26DB781F476198087246BF849`、
  `274DAB2086C2B8E352F4A4DA7B075F8B86D664A4DAF189ACEBDBDE60685D1F19`，安装目录
  与 `web/dist` 完全一致。

## 下一项高收益方向

~~对当前 616 个 JSN 静态/动态模板做与云配置相同的语义覆盖审计，并提升首批
高价值关系链。~~ 已完成，详见
[JSN 语义覆盖与个性数据全景](2026-08-06-native-jsn-variant-panorama.md)。
