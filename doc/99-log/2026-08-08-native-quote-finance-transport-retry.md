# 公开行情与财务 7709 瞬时断连恢复

> 日期：2026-08-08。范围：非 L2、纯 C++ 公开 7709 一次性请求。

## 问题

TPool 内联 XML 版本首次正式全量契约为 166/168，随后单独重试与全量复跑均
恢复到 168/168。失败不是业务字段变化：逆回购的 `0x054C` 快照和个股估值的
`0x0010` 财务请求都在 TCP 已建立后收到对端提前断开。两条读取链虽然能够切换
候选端点，默认却只有一个公开地址，因此同一地址没有第二次新连接机会。

## 实现

传输层新增共享的窄判定与有界执行器：

- 只把 DNS 解析、TCP 建连、发送、接收以及对端提前断开视为可恢复；
- 每个端点最多三次，每次重新建立 `QuoteConnection`，短退避为 50/100 ms；
- 响应解码、消息号、命令号和业务结构错误不重试；
- 持续传输错误在第三次后保留最后异常，不无限循环。

`0x054C/0x053E` 行情批次与 `0x0010/0x000F/0x0452` 财务相关读取均复用该执行器。
每份成功文档新增 `transport`，包含 `connection_attempts`、
`transient_retries`、`endpoints_attempted`、`max_attempts_per_endpoint` 与
`recovered_after_retry`。逆回购把它保留在 `quote_source.transport`；个股四项
估值把行情和财务分别保留在 `valuation.upstream_transport`，方便判断最终成功
是否经历透明恢复。

## 验证

新增 `tdx-transport-retry-tests` 的固定故障注入：前两次提前断开、第三次恢复；
持续 WSA 接收错误三次耗尽；稳定解码错误只调用一次。原生全量 CTest 为
99/99。

临时服务使用新构建直接读取平安银行与逆回购：行情、财务、涨速均一次成功，
返回 `connection_attempts=1`、`transient_retries=0`。增强后的既有四项契约
（逆回购、个股估值、平安银行涨速、ETF IOPV）在临时和正式服务均为 4/4，
两边全量均为 168/168。

正式服务已恢复在 `127.0.0.1:8765`，PID `32320`。部署 EXE SHA-256 为
`8789F472EF1307AF38AB0FEC8DB1042F780E315289EDC1F749BAB6B94DEE4988`。

证据：

- `output/probes/quote-retry-live-20260808.json`
- `output/probes/finance-retry-live-20260808.json`
- `output/probes/api-contracts-transport-retry-temp-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-transport-retry.json`
- `output/probes/api-contracts-transport-retry-formal-20260808.json`
- `output/probes/api-contracts-full-formal-20260808-transport-retry.json`

## 边界

这不是对任意异常的盲目重放，也没有改变 L2 权限边界。持久 `market watch`/SSE
仍使用自己的连接复用、端点切换和指数退避状态机；本轮处理的是一次性公开
行情与财务读取在首次连接被服务端关闭时的透明恢复。
