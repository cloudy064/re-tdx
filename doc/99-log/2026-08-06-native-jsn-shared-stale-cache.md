# 共享 JSN 陈旧缓存与个股接口健康契约

## 本轮目标

此前共享 7709 JSN 读取器已经具备三次新连接重试，但重试耗尽后只有主动基金
服务拥有明确的陈旧缓存语义。机构调研、一致预期、行业画像、股权变动和股份
回购都是个股工作台高频链路，仍可能把一次短暂断连直接暴露成失败，也无法让
调用方区分业务局部缺失、上游陈旧和冷缓存不可用。

## 实现边界

- `fetch_jsn_resources_rows` 成功取得并解析完整批次后，把每个原始资源文档写入
  线程安全的进程内共享缓存；缓存保存紧凑序列化 JSON，上限为 512 项和
  256 MiB 序列化载荷，按最近使用序列淘汰；陈旧命中后在锁外重新解析，避免
  长期保留第二份展开 JSON，也不让大文档解析阻塞其他缓存访问；
- 每次读取最多建立三条新 7709 连接，间隔 250/500 ms。成功来源记录
  `attempts=1..3`、`stale=false`、`age_seconds=0`；
- 重试耗尽时，只有本次所有资源键都存在成功缓存才整批返回陈旧数据，并附带
  缓存年龄和最终上游错误。缺任一键则保持失败，避免把不完整主表拼成伪成功；
- 服务端报告零长度资源的真实 `MissingResource` 不重试、不回退，超大资源也不
  进入共享缓存；
- 机构调研、一致预期、行业画像、股权变动、股份回购和主动基金统一使用来源
  摘要与健康聚合。`availability` 分为 `live`、`partial`、`stale-cache`，其中
  `partial` 表示业务动态详情局部缺失，`stale-cache` 才表示上游刷新失败；
- 冷缓存的 7709、PBRPC 和 TQLEX 瞬时传输故障统一返回
  `503/upstream_unavailable/retryable=true`。TQLEX 429/502/503/504 和业务错误 4
  不再落成参数错误 400。

缓存只保存本进程曾成功取得的公开数据，不持久化、不绕过授权，也不改变通达信
客户端文件。进程重启后从冷缓存重新开始。

## 契约与故障证据

`recon api-contracts --profile full` 从 13 项扩展为 18 项，新增五个真实个股样本：

- `stock-research-live`；
- `stock-consensus-live`；
- `stock-industry-profile-live`；
- `stock-ownership-live`；
- `stock-repurchases-live`。

每项验证业务 schema、`security` 模式、三态可用性、缓存陈旧布尔值、聚合上游
健康，以及每个来源的尝试次数、年龄和错误字段。巡检对 429/502/503/504 最多
尝试三次并记录 `request_attempts`，最终失败仍返回非零退出码。

隔离服务第一次扩展巡检为 14/18：当时 TQLEX 与 PBRPC 同时短暂返回 502，失败
正文保存在 `output/probes/api-contracts-jsn-stale-temp.json`；紧接着重跑恢复为
18/18。增加规范化和巡检重试后，最终隔离证据为
`output/probes/api-contracts-jsn-stale-temp-final.json`，18/18 且 18 次网络请求。

确定性 C++ 测试覆盖：完整双资源批次写入、三次失败后整批陈旧回退、来源健康
聚合，以及唯一冷键三次失败后保持错误。全量原生 CTest 为 42/42。

## 正式部署

正式 `127.0.0.1:8765` 服务已由同日后续扩展版本替换，当前 PID 为 `4624`，命令行为
`dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`。部署 EXE
SHA-256 为
`46BDCCA0C7D081536BDA11942F369224731D980328CF62EB46DDE42899467DE7`；健康状态
为 `ok=true`、`native_cpp=true`、`python_runtime=false`，功能目录为 98 项。
正式 45/45 证据为 `output/probes/api-contracts-official-current.json`。

当前 Svelte HTML、JS、CSS SHA-256 分别为
`A06BDF9B815395DAF0302E29922AFBE9DE20CBEFD5B61A4F58E066C11C4510AD`、
`F37601F5F0451AC8398A7C921478E7946FEFDFD6E9069051AADA327136D89618`、
`274DAB2086C2B8E352F4A4DA7B075F8B86D664A4DAF189ACEBDBDE60685D1F19`。

## 下一批高收益方向

1. ~~将同一健康契约继续覆盖机构龙虎、评级、外资预警、限售解禁、大宗交易和
   龙虎榜事件详情。~~ 已完成，详见
   [扩展健康契约与公式全量审计](2026-08-06-native-jsn-expanded-health-formula-audit.md)；
2. ~~对 73 个 TQLEX 模板/34 个唯一 ReqId、58 个 PBRPC 模板/29 个唯一 ReqId
   建立参数、Entry、模块和占位符变体矩阵，找出“请求号已覆盖但功能分支未闭合”
   的真实缺口。~~ 已完成，详见
   [云配置变体与上市公司路演](2026-08-06-native-cloud-variant-roadshows.md)；
3. ~~在不涉及 L2 权限的前提下，探索通达信 L1 行情订阅链并评估 SSE 推送，减少
   当前个股轮询请求量。~~ 已完成，详见
   [公开 L1 持久会话与 SSE](2026-08-06-native-l1-sse-stream.md)。
