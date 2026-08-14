# 因子与技术信号研究页及云端重试收敛

日期：2026-08-11

## 结果

新增 Svelte 路由 `/data/factor-signals`，把此前只有原生 API、网页没有入口的两类能力接入
数据中心：

- 标准因子、K 线形态目录，以及因子到证券的直接成员下钻；
- 因子看板；
- 九转、RPS、趋势、模型、竞价、盘中机会等 32 类技术信号；
- 九转向上/向下过滤，以及结果证券到个股工作台的跳转。

页面复用现有资源加载器、表格和路由状态，没有复制 API 请求与错误处理框架。实现位于
`web/src/views/data/FactorSignalsView.svelte`，导航入口归入“预期与研究”。

## 瞬态错误策略

真实样例曾出现 `WinHttpReceiveResponse failed: 12030`。根因不是因子协议解析，而是因子和
技术信号模块各自维护了不完整的字符串判断，漏掉了共享传输层已经识别的 WinHTTP 连接
中断。两个模块现统一调用 `tdx::detail::is_transient_cloud_error`，共同覆盖 WinHTTP
发送/接收失败、HTTP 429/502/503/504、业务 `ErrorCode 4` 和 RPC `-1`。

这次只收敛受当前页面影响的两个模块，没有顺手改动其他业务服务，避免扩大验证范围；剩余
重复分类器可以按功能接入顺序继续迁移。

## 聚焦验证

- `tdx-cloud-resilience-tests`、`tdx-factors-tests`、`tdx-technical-signals-tests` 通过；
- `tdx-tool` 编译链接通过；
- Svelte 类型检查为 0 error / 0 warning，生产构建通过；
- 正式服务不重启，仅只读检查新路由返回 200 且包含 Svelte 挂载点；
- 新二进制直接执行真实上游样例：因子目录 33 条、因子 `1` 成员 128 条、九转信号
  1,295 条，三项均为 `availability=live`；
- 证据汇总位于 `output/web-factor-signals-coverage.json`，真实响应分别位于
  `output/factor-signals-catalog-sample.json`、`output/factor-signals-members-sample.json`
  和 `output/factor-signals-nine-turn-sample.json`。

依据增量验证规则，本轮没有运行完整 CTest 或完整 API 契约套件。
