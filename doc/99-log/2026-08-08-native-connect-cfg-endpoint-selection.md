# connect.cfg 公开行情主站与备用池选择

> 日期：2026-08-08。范围：非 L2、公开 7709 服务器配置与纯 C++ 请求路由。

> 2026-08-12 纠正：TdxW 的 `PrimaryHost` 是 `0..HostNum-1` 的零基位置，
> `HostName01/IPAddress01` 的后缀才从 1 开始。本文原先把 `PrimaryHost=31`
> 解释成第 31 项，并在两节点测试中使用无效的 `PrimaryHost=2`，记录的是旧 C++
> 实现的错位行为，不是客户端真实选择语义。当前实现、证据和重新验收见
> [服务器组角色与零基主站语义](2026-08-12-native-session-endpoint-roles.md)。

## 发现

上一轮为 7709 一次性请求加入了同端点三次新连接恢复，但默认服务器仍硬编码为
`110.41.147.114:7709`。安装目录 `connect.cfg` 的 `[HQHOST]` 实际声明 43 个
公开行情节点，并用 `PrimaryHost=31` 指定零基第 32 个位置。旧实现误选中的
`123.60.84.66:7709` 确实支持 `0x054C` 行情和 `0x0010` 财务，但这不能证明它是
TdxW 当时的真实首选。

## 实现

新增共享 `load_public_quote_endpoints`：

- 只读解析 `[HQHOST]` 的 `HostNum/PrimaryHost/IPAddressNN/PortNN/HostNameNN`；
- 首先返回 `PrimaryHost`，再从下一索引开始循环，默认只取 3 个候选，避免故障时
  遍历 43 个节点造成无界延迟；
- 用户显式 `--host` 时完全使用调用方顺序；
- 配置文件缺失、分组缺失或解析失败时使用原公共节点作为兼容回退；
- 不读取登录票据、用户状态或 L2 会话字段。

快照、涨速、五档持久会话、财务、除权事件和特殊涨跌停表均接入该选择。公式
上下文、TBigData 计算列、逆回购和个股四项估值通过共享读取链自动继承。
CLI 的 `market finance/capital/limits` 新增 `--root`，以便与行情命令一样读取用户
自己的公开服务器配置。

## 验证

旧固定测试曾使用两节点配置的无效值 `PrimaryHost=2`；2026-08-12 已改为有效的
`PrimaryHost=1`，验证零基第 2 项优先并回绕到第 1 项。
真实 CLI 的快照、财务和五档均命中 `123.60.84.66:7709`：快照报告安装内共有
43 个节点，实际候选池为 3；持久五档会话也报告相同主站优先来源。

- 原生全量 CTest：99/99；
- 临时专项契约：5/5（逆回购、个股估值、两类涨速、L1 SSE）；
- 临时全量契约：168/168；
- 正式专项契约：5/5；正式全量契约：168/168。

正式服务已恢复在 `127.0.0.1:8765`，PID `44676`。部署 EXE SHA-256 为
`EF80AF2AD7C2D0F2E216BC87072D46A77A1CEB0D3AF26FC4CD2657CDDB844AD6`。

证据：

- `output/probes/session-config-current-20260808.json`
- `output/probes/connect-primary-snapshot-20260808.json`
- `output/probes/connect-primary-finance-20260808.json`
- `output/probes/connect-cfg-snapshot-20260808.json`
- `output/probes/connect-cfg-finance-20260808.json`
- `output/probes/connect-cfg-depth-20260808.json`
- `output/probes/api-contracts-connect-cfg-temp-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-connect-cfg.json`
- `output/probes/api-contracts-connect-cfg-formal-20260808.json`
- `output/probes/api-contracts-full-formal-20260808-connect-cfg.json`

## 边界

这是公开 7709 节点选择，不是 tpbus/TaApi 登录服务器选择，也不产生主动 L2
订阅。工具只使用安装文件已公开的地址与端口；三节点候选池和每节点三次瞬时
重试均有固定上限。
