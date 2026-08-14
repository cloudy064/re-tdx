# JSN 主站选择、跨节点恢复与解释器剩余边界

> 日期：2026-08-08。范围：非 L2 的公开 7709 JSN `709→1721` 链，以及当前
> TCalc 全库审计边界。

## 结论

解释器没有新的可证实语法缺口：379/379 公式均有源码并通过语法分析；平安银行
日线全库 379 条中 354 条通过、20 条依赖不可用、4 条市场不适用、1 条周期不适用，
解释器错误为 0。20 条依赖缺口分别需要 14 条合法 L2 序列和 6 条券商私有
`SIGNALS_QS`。C++ 上下文模板和显式注入入口已经具备，但没有授权数据时不生成
猜测值。

公开资源侧找到一处真实剩余缺口：JSN 元数据探测、文件下载及所有固定 JSN API
共用的批量读取器仍直接使用 `110.41.147.114:7709`，没有继承已经用于行情链的
`connect.cfg/[HQHOST]` 主站和备用节点。

## 实现

- `transfer_jsn_resource`、`fetch_jsn_resource_rows` 和批量
  `fetch_jsn_resources_rows` 接入 `select_public_quote_endpoints`；未传 root 时安全
  自动发现通达信安装目录，失败才回退编译期公共节点；
- 每节点最多三次新连接，默认最多三个端点；前三次失败后才切换下一节点；
- 完整批量解析成功后才写共享缓存，半批、半文件和解析失败均不提交；
- 零长度资源仍作为真实缺失立即返回，不跨节点或用陈旧缓存伪造；
- 探测/下载结果增加 `transport`：端点来源、可用总数、连接次数、瞬时重试、
  尝试节点数及是否故障转移；
- `jsn download` 新增 `--root`，默认说明改为 HQHOST 主站优先；服务端 JSN 写操作
  和动态候选探测显式传递当前安装根目录；
- 修正共享选择器中显式 `--host` 未遵守 `max_endpoints` 的边界，仍保留
  `available_endpoint_count` 为调用方提供的总候选数；
- `zhb.zip` 帮助文本同步移除旧固定首站说明。

## 真实验证

以 `list/func_yzyq109_1.jsn` 为样本：

- 默认选择命中 `123.60.84.66:7709`，服务名“上海双线主站15”，资源 19,824
  字节、MD5 `509b5d8b7030d631ec760240e8acd7c3`；`endpoint_source` 为
  `connect.cfg:hqhost-primary-first`，配置池共 43 个节点；
- 显式顺序 `127.0.0.1:1 → 110.41.147.114:7709` 时前三次连接失败，第 4 次在
  第二节点成功；结果为 `connection_attempts=4`、`transient_retries=2`、
  `endpoints_attempted=2`、`endpoint_failover=true`；
- 共识固定命令一次批量下载 9 个资源，963 行，来源全部命中配置主站且非陈旧；
- 原生全量 CTest 99/99；临时服务全契约 173/173；正式服务全契约 173/173。

正式服务为 `http://127.0.0.1:8765/`，PID `37296`。只有一个 `tdx-tool` 进程，
8766 已关闭。部署 EXE SHA-256：
`6FF3CB56E939B36C3B6D9E32BB066933CA4F8DB4760DA77E9E0732E19E89CFA8`。

## 证据

- `output/probes/jsn-endpoint-primary-20260808.json`
- `output/probes/jsn-endpoint-failover-20260808.json`
- `output/probes/jsn-shared-fetch-smoke-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-jsn-endpoints.json`
- `output/probes/api-contracts-full-formal-20260808-jsn-endpoints.json`

> 同日后续发布又补齐了股东穿透 TQLEX 容错；当前正式 PID、哈希和最终
> 100/100、174/174 结果以
> `2026-08-08-holder-tqlex-resilience.md` 为准。本节 99/99、173/173 是 JSN
> 检查点本身的实测，不作追溯改写。
