# 公开 7709 分页链主站选择与断连恢复补齐

> 日期：2026-08-08。范围：非 L2、公开 7709 的集合竞价、成交明细、服务端
> 排行、统计资源、证券目录及普通 K 线。

## 问题

快照、五档和财务链已经使用 `connect.cfg/[HQHOST]/PrimaryHost` 与有界瞬时
重连，但其余多页/多证券读取仍各自硬编码旧节点 `110.41.147.114:7709`。
这些链不能机械地在任意字节位置续传，否则可能重复证券、跳页或把半包写入缓存。

## 恢复边界

| 链路 | 命令 | 失败后的安全恢复点 |
| --- | --- | --- |
| 集合竞价 | `0x056A` | 单证券完整解析后才推进证券游标 |
| 当日/历史成交 | `0x0FC5/0x0FC6` | 单证券全部成交页完成后才提交 |
| 服务端排行 | `0x054B` | 每次连接从排行首请求重建临时结果 |
| 统计资源 | `0x06B9` | 从 ZIP 第 0 块重建，解析成功后才更新缓存 |
| 证券目录 | `0x044E/0x044D` | 每个市场从数量请求和第 0 页重建 |
| 普通 K 线 | `0x052D` | 仅保留已经完整解析的页，从下一页游标继续 |

所有链只对 DNS、建连、发送、接收和对端提前断开使用新 TCP 连接恢复；每节点
最多三次尝试，默认最多三个配置节点。协议、解码和业务错误立即失败。显式
`--host` 保持用户顺序并覆盖配置。

## 实现

- 新增共享 `select_public_quote_endpoints` 和
  `public_quote_transport_document`，统一显式节点覆盖与审计元数据；
- 内部复用调用未传 root 时先安全自动发现本机安装目录，发现失败才使用编译期
  回退，使公式、期权和事件组合链也继承当前 HQHOST；
- 竞价、逐笔、排行、统计、证券目录和普通 K 线接入配置主站与故障转移；
- API 的统计缓存同时保存传输元数据，证券目录按市场保存来源；
- K 线保留原 `transport=tdx-7709-0x052d` 协议标识，并新增
  `transport_detail`，避免破坏既有客户端；
- `market kline`、`minute download` 和 `market securities` CLI 支持从
  `--root` 读取当前公开主站；
- 7727 扩展行情没有证据属于 `HQHOST`，因此保持独立节点池，不做错误合并；
- 新增五项 API 契约，覆盖 30 分钟跨日历史、竞价、逐笔、排行和证券目录，
  契约总数由 168 增至 173。

## 实测

当前 `C:\new_tdx\connect.cfg` 声明 43 个 HQHOST，六条真实 CLI 探针都命中
`123.60.84.66:7709`。平安银行 30 分钟线以两页各 80 根下载，共 160 根并跨越
多个交易日；历史成交获得 4,573 条。故意使用
`127.0.0.1:1 → 123.60.84.66:7709` 时，K 线仍返回 160 根，深市证券目录仍完整
下载 23,937 条；两者均报告 4 次连接尝试、2 次瞬时重试、2 个端点及故障转移。

验证结果：

- 原生全量 CTest：99/99；
- 临时专项 API 契约：6/6；临时全量：173/173；
- 最终正式专项 API 契约：6/6；正式全量：173/173。

正式服务为 `http://127.0.0.1:8765/`，PID `8772`。部署 EXE SHA-256：
`AE7E67AE5AD610FA5A896CDFACCB83B12A70D7E98B93D50551900F76DFAA7A0C`。
仅有一个 `tdx-tool` 进程，8766 未监听。

证据：

- `output/probes/endpoint-auction-20260808.json`
- `output/probes/endpoint-trades-20260808.json`
- `output/probes/endpoint-ranking-20260808.json`
- `output/probes/endpoint-stats-20260808.json`
- `output/probes/endpoint-securities-20260808.json`
- `output/probes/endpoint-kline-20260808.json`
- `output/probes/failover-kline-20260808.json`
- `output/probes/failover-securities-20260808.json`
- `output/probes/api-contracts-quote-chains-temp-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-quote-chains.json`
- `output/probes/api-contracts-quote-chains-formal-20260808.json`
- `output/probes/api-contracts-full-formal-20260808-quote-chains.json`
