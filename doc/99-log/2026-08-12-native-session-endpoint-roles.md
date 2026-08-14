# connect.cfg 服务器组角色、默认端口与零基主站语义

## 结论

`recon session-config` 现在不再只罗列 section 名称。每个服务器组都附带独立的
`tdx-endpoint-group-role-v1` 文档，报告角色、证据等级、协议族、TdxW 加载器默认
端口、主站选择器、静态证据和限制。证据不足的组保留 `unknown/unresolved`，不会从
缩写猜测用途，也不会通过联网探测补齐结论。

同时修正了 `PrimaryHost` 的索引语义：TdxW 把它限制在 `0..HostNum-1`，它是零基
数组位置；`HostName01/IPAddress01/Port01` 的配置后缀才是从 1 开始。旧 C++ 代码
直接拿二者比较，导致 `PrimaryHost=0` 没有主节点，其他值则错位一项。

## 静态证据与角色目录

| section | 角色 | 状态 | 加载器默认端口 | 结论边界 |
| --- | --- | --- | ---: | --- |
| `HQHOST` | `public_quote` | `protocol-verified` | 7709 | 公共行情协议已实现；不代表节点当前可达 |
| `DSHOST` | `expansion_market_quote` | `protocol-verified` | 7721 | 当前配置显式 7727 覆盖默认值，7727 协议已实现 |
| `HFHOST` | `high_frequency_quote_host_pool` | `client-binding-verified` | 7709 | 同一组分别受 `L1HFPrimaryHost` 与 `L2HFPrimaryHost` 选择，不能只归为 L1 或 L2；协议与授权未解 |
| `INFOHOST` | `information_service` | `loader-semantic` | 7711 | 专用加载分支与 `HostTypeNN` 已确认，线协议未解 |
| `INFOHOST2` | `information_service` | `loader-semantic` | 7711 | `PortNN` 可显式覆盖；当前配置名称为“资讯主站”、端口 7712 |
| `WTHOST` | `unknown` | `unresolved` | 7708 | 只确认加载 `HostTypeNN/YYBIDSNN/SSLNN`；不把缩写猜成交易协议 |

对应 IDA 证据来自当前 TdxW 的：

- `sub_657A80`：加载 HQ/INFO/INFO2/WT/DS 组，给出默认端口并把
  `PrimaryHost` 限制到 `0..HostNum-1`；
- `sub_6536E0`：selector 10 从 `HFHOST` 加载 L2 HF 表，selector 6 从
  `DSHOST` 加载扩展市场表；
- `sub_75C630`：再次从 `HFHOST` 加载 L1 HF 表，并分别读取
  `OTHERHOST.L1HFPrimaryHost` 与 `OTHERHOST.L2HFPrimaryHost`；
- 下载启动链：通过 `DSHOST_EXTERN` 选择扩展市场节点。

`L2HFhost` 字符串本身没有交叉引用；本次结论不依赖该孤立字符串，而依赖两张实际
加载表和两个 PrimaryHost 选择器的闭合调用链。

## 实现

- `market/session_endpoint_roles.cpp`：固定类型化角色表、默认端口与主站选择模式；
- `market/session_audit.cpp`：按角色默认端口解析缺省 `PortNN`，按零基位置选择主站，
  输出 `configured_primary_index/primary_index/primary_index_base/role`；
- `recon/runtime_topology_config.cpp`：TCP 精确命中时附带精简的
  `role/role_status/protocol_family`，仍不把整份私密状态带入运行快照。

HFHOST 没有唯一 primary：同一端点池由 L1HF、L2HF 两个上下文分别选择，所以审计
明确输出两个 selector，并保持 `selected_primary=null`。其他已确认使用 section 内
`PrimaryHost` 的组按零基位置生成唯一选择。

## 当前安装样本

`C:\new_tdx\connect.cfg` 当前仍有 6 组、69 个公开端点：

- `hqhost=43`，`PrimaryHost=1`，正确选择第 2 项 `110.41.2.72:7709`；
- `dshost=16`，`PrimaryHost=0`，选择第 1 项 `112.74.214.43:7727`；
- `infohost2=8`，`PrimaryHost=0`，选择第 1 项 `121.36.199.182:7712`；
- `hfhost=2`，分别等待 L1HF/L2HF 上下文选择；
- `infohost/wthost` 当前为空，其中 WTHOST 仍明确为 unresolved。

真实 `market snapshot --security sz:000001` 首次连接即命中新 HQ 主节点
`110.41.2.72:7709`，取得 1/1 行情，连接尝试 1、故障切换 0。

证据输出：

- `output/session-config-runtime-correlation-20260812.json`
- `output/runtime-topology-config-correlated-20260812.json`
- `output/session-primary-zero-based-live-20260812.json`

## 验证

- `tdx-session-audit-tests`：角色、默认端口、零基主站、HF 双 selector 与私密值不泄漏；
- `tdx-runtime-topology-tests`：精确端点匹配保留角色、状态和协议族；
- 当前安装：69 个端点、5 个私密字段仅报告存在性，敏感内容泄漏检查为 0；
- 生产源闭包：736/736、重复 0、未收录 0、缺失 0；
- 完整 CTest：114/114；
- 正式 8765 服务仍为 PID 24096，未重启或替换。

完整构建期间曾因一次超时构建与下一次 make 短暂重叠，使两个测试归档被并发写入；
清单审计确认测试 target 没有重复源。由 CMake 单进程重新链接后，两项专项及随后单次
完整 114/114 回归均通过。

## 未解项

- INFOHOST/INFOHOST2 的请求帧、内容模型和两组分工仍需从实际调用方继续追踪；
- HFHOST 的具体线协议、L1/L2 模式差异和合法授权前提尚未解码，本轮没有连接；
- WTHOST 虽具有 `YYBIDS/SSL/HostType` 字段，但在请求链闭合前维持 unknown。
