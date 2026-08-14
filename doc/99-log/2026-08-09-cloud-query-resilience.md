# 通用云查询整批重试与解释器边界复核

## 结果

TQLEX、PBRPC 的通用 CLI、固定 API 和多步云工作流已统一使用纯 C++ 瞬时
故障分类与整次操作重试。TQLEX 分页失败时从第一页重建完整查询；PBRPC 任一
轮失败时丢弃当前 `RpcID` 和分段载荷，从 `RpcID=0` 重开完整会话，避免把两次
会话的半包拼在一起。

默认最多三次，线性等待基数 250 ms。只重试以下明确瞬时故障：

- HTTP 429、502、503、504；
- TQLEX `ErrorCode 4`；
- PBRPC 业务/服务端 code 4 及服务端返回 `RpcID -1`；
- WinHTTP 发送或接收失败。

`ErrorCode 5`、配置选择歧义、协议结构、字段解析和其他稳定错误只执行一次并
原样失败。通用查询和工作流没有陈旧缓存，因此三次耗尽后不会伪造旧结果。
已有独立外层重试的类型化服务不再套一层通用重试，避免一次故障膨胀成九次请求；
十大流通股东仍保留自身的按历史/详情独立陈旧缓存语义，但复用同一分类器。

## 对外入口

- `cloud tqlex` 新增 `--attempts`、`--attempt-delay-ms`，响应记录
  `attempts/max_attempts`；
- `cloud pbrpc` 使用同名参数控制完整 RPC 会话重开；
- `/api/v1/tqlex/query`、`/api/v1/pbrpc/query` 固定最多三次并返回实际次数；
- `cloud workflow` 及 `/api/v1/cloud/workflow` 的每一步分别重试，并在步骤中
  保留实际尝试次数。

真实配置烟测中，TQLEX `200626` 一次返回 33 行因子目录，PBRPC `200340`
一次返回 39 行指数分时段资金，两者均为 `attempts=1/max_attempts=3`。新增固定
契约 `generic-tqlex-live`、`generic-pbrpc-live`，检查配置选择、上游
`ErrorCode=0`、非空结果和重试元数据。

## 验证与发布

- 故障注入覆盖：第三次恢复、稳定错误只调用一次、PBRPC `RpcID -1` 三次耗尽
  且保留最终错误；
- 原生 CTest：101/101；
- 临时 8766：新增契约 2/2、全量 176/176；
- 正式 8765：新增契约 2/2、全量 176/176；
- 正式进程 PID：`34156`；
- EXE SHA-256：
  `72B401F46C6F21E7D2A33AEAFF5C3D09384A62FAA96F38D36B1F04B6DCD244FC`；
- 8766 已关闭，只有 `127.0.0.1:8765` 在监听。

证据文件：

- `output/probes/tqlex-generic-live-20260809.json`
- `output/probes/pbrpc-generic-live-20260809.json`
- `output/probes/api-contract-cloud-resilience-temp-20260809.json`
- `output/probes/api-contracts-full-temp-20260809-cloud-resilience.json`
- `output/probes/api-contract-cloud-resilience-formal-20260809.json`
- `output/probes/api-contracts-full-formal-20260809-cloud-resilience.json`

## 解释器边界

本轮没有用重试或云查询去填充公式缺失依赖。解释器仍是 379/379 源码/语法覆盖；
平安银行全库审计为 354 通过、20 外部依赖不可用、4 市场不适用、1 周期不适用、
0 解释器错误。20 条依赖仍由 14 条需授权 L2 序列和 6 条券商私有
`SIGNALS_QS` 构成；项目已有显式上下文注入入口，没有授权数据时继续留空，不
推导、不伪造。
