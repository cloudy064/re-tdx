# 公式 HTTP 与 TQFLAG 复权上下文闭环

## 结论

本轮把公式解释器的复权状态从“能读取离线 K 线文档元数据”推进为完整在线功能。
`/api/v1/formulas/evaluate` 的内置 GET、内联源码 POST、无源码内置公式 POST，
以及 `/api/v1/formulas/audit`，现在与 `/api/v1/kline` 复用同一条纯 C++ 本地复权链。

请求传 `adjust=none|qfq|hfq` 后，以下三者严格使用同一份已复权 K 线文档：

- 响应 `points[].open/high/low/close`；
- 公式中所有 OHLC 派生指标；
- 自动变量 `TQFLAG`，分别返回 0/1/2。

响应继续保留 `adjustment_mode` 和完整 `adjustment` 元数据，因此调用方无需根据
请求参数猜测服务端最终采用的口径。

## 共享实现

此前 `/api/v1/kline` 已具备成熟流程：下载目标周期 K 线，下载最多 20 页日线，
通过公开 `0x000F` 股本变更链取得除权事件，然后调用
`apply_kline_adjustment`。公式接口过去直接调用 `fetch_kline_document`，绕过了该
流程，所以在线 `TQFLAG` 只能是 0。

服务端现抽出 `apply_requested_kline_adjustment`，由 K 线、公式 GET、公式 POST
和全库审计共享。边界保持不变：

- raw/none 不发额外除权请求，并规范化为 `adjustment_mode=none`；
- qfq/hfq 使用本地 corporate-action factor v1；
- `fixed_qfq/fixed_hfq` 仍要求 `anchor_date`，供 API 调用方使用；
- 扩展市场和指数拒绝公司行为复权，不伪造因子；
- 未知模式由 `apply_kline_adjustment` 明确返回参数错误。

公式求值结果的元数据透传列表新增 `adjustment_mode/adjustment`。这一步很重要：
若只在环境中设置 `TQFLAG` 而不返回元数据，网页无法证明价格和标志来自同一口径。

## POST 与安全边界

本阶段 POST body 仅在公式 evaluate 路径读取字符串 `adjust` 和 `anchor_date`，
扫描与回测仍沿用既有原始行情口径。随后已单独验证调仓时点、财务时点、手续费、
批量错误边界与请求身份，并把同一能力开放到扫描、单票回测和组合策略；详见
[后续闭环](2026-08-09-native-formula-workflow-adjustment.md)。

## Svelte 工作台

公式工作台对深沪京证券新增“不复权 / 前复权 / 后复权”三态选择。选择值进入：

- 自定义技术公式 POST body；
- 显式上下文内置公式 POST body；
- 普通内置公式 GET URL；
- 全库审计 GET URL。

因此不同复权选择天然拥有不同请求身份，不会复用同一 URL/body。扩展市场切换时
自动回到 none，避免把 A 股公司行为参数带入期货、期权或港股。计算结果顶部显示
服务端实际返回的复权模式，而不是仅显示本地选择状态。

构建产物：

- `web/dist/assets/index-CyMlaSp9.js`：1,198,199 字节；
- SHA-256：
  `696620A65FE8208D7182F70A22ED247ED007B17641586B5C1AA6B70B1154FC57`。

同一资源已同步到自包含发行目录
`dist/tdx-tool/share/tdx-tool/web/assets/index-CyMlaSp9.js`。

## 契约与验证

原 `formula-adjustment-flag-inline-post` 契约由 raw 默认升级为真实 qfq：

```text
POST source="T:TQFLAG;TF:TQFLAG();" adjust=qfq
=> adjustment_mode=qfq
=> adjustment.source_command=0x000F
=> 120 根 T=1、TF=1
```

新增 `formula-adjustment-qfq-get`：

```text
GET formula=MACD&period=day&adjust=qfq
=> 120 根 DIF/DEA/MACD
=> adjustment_mode=qfq
=> local-corporate-action-factor-v1
```

另在正式服务直接验证 POST `adjust=hfq` 返回 `TQFLAG=2`。完整结果：

- `tdx-formula-engine-tests`：通过；
- `tdx-recon-contract-tests`：通过；
- CTest：101/101；
- Svelte production build：通过；
- 临时专项：6/6；
- 临时 full API：214/214，215 次网络请求；
- 正式专项：6/6。

报告为：

- `output/native-formula-http-adjust-targeted-api.json`；
- `output/native-formula-http-adjust-full-api.json`；
- `output/native-formula-http-adjust-formal-api.json`。

## 发布

正式 EXE 为 42,425,960 字节，SHA-256
`787A329C7C9C3D2EB6DA56D6C9317E4F243FA0B2054BB7B9DEDF6DFC6B2AFFDA`。正式服务
PID 24964，仅监听 `127.0.0.1:8765`；健康检查为 `native_cpp=true`、
`python_runtime=false`，临时 8875 已关闭。

发布时第一次普通覆盖没有改变 dist 哈希，专项工具因找不到新增契约而拒绝通过；
随后停止旧进程，以 `Copy-Item -ErrorAction Stop` 覆盖并强制比较 build/dist
SHA-256，确认一致后才重新启动。该失败没有被当作发布成功，最终 6/6 来自已核验
的新二进制。
