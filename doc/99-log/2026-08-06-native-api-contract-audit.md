# 固定 API 契约巡检纯 C++ 固定化

## 背景

此前真实使用中出现过“所选证券不在主动基金季度汇总”“不在机构龙虎周期”
“不在外资预警清单”等提示。这些是正常空关系，不应成为 HTTP 400。相关接口
已经逐个修复，但验收仍依赖人工请求；`serve --self-test` 只检查本地索引、板块
和公式，不覆盖这些服务契约。

新增纯 C++ `recon api-contracts`。它只读取指定服务，不启动 Python、不修改
通达信目录，也不允许传入任意上游模板；默认检查本机
`http://127.0.0.1:8765`。

## 契约集合

quick 共 5 项：

- 健康检查必须为原生 C++、不使用 Python；
- 功能目录的 count 与数组一致，并包含基金、异常说明和契约巡检命令；
- OpenAPI 必须登记健康、基金分析和异常说明固定路径；
- 首页为 HTML 且保留 Svelte `#app` 挂载点；
- 非法市场必须返回 `400 + bad_request` 和明确参数信息。

full 在此基础上增加 7 项：

- 不存在的合法六位证券在异常说明、主动基金、机构龙虎和外资预警中均按
  `200 + 空数组/null` 返回；
- 不存在的六位基金在显式报告期返回 `availability=empty`、零记录且不回退；
- 先发默认基金报告期请求，读取服务解析出的请求期，再显式请求同一报告期，
  要求解析期保持不变且 `latest_report_fallback_used=false`，防止缓存污染。

每条检查记录 HTTP 状态、内容类型、响应大小、耗时和逐断言 expected/actual。
连接失败和解析失败写入同一 JSON 报告；只要一项失败，命令退出码为 1。
支持 `--case` 重复筛选、`--profile quick|full`、单请求超时、紧凑 JSON 和
原子输出文件。

## 验收

- 离线专项测试覆盖健康、正常空主动基金、旧式 400 错误、非法参数、报告期
  缓存隔离正反例及 Svelte 首页；
- 临时新服务 full 真实巡检 12/12 通过，12 次请求约 6.7 秒；其中主动基金
  空关系约 3.38 秒、机构龙虎约 1.67 秒，其余均低于 0.4 秒；
- quick 真实巡检 5/5 通过；
- 对未监听端口的单项检查生成 WinHTTP 12029 失败证据并返回退出码 1；
- 证据保存在 `output/probes/api-contracts-full-current.json`、
  `api-contracts-quick-current.json` 和 `api-contracts-unavailable-check.json`；
- 全量 CTest 40/40 通过。

## 正式部署

正式 `8765` 服务已部署，PID 为 `40416`，EXE SHA-256 为
`C6847363881CA643ECBD8F25E45902E6DAF13A30A6F9EE49FBEF2A3D5816C87D`。
健康检查为 `ok=true`、`native_cpp=true`、`python_runtime=false`，功能目录由
88 增至 89 并包含 `recon api-contracts`。

发布目录中的新命令反向巡检正式服务，full 12/12 通过、退出码 0，证据为
`output/probes/api-contracts-official-current.json`。主页 HTTP 200；Svelte
HTML/JS/CSS 哈希仍为 `0F1FBB...5777`、`B03C28...4011`、
`733085...30A`，没有覆盖用户 UI。临时 `8877` 服务已停止。

## 后续扩展：分时资金契约

同日后续加入 `intraday-funds-live`，检查平安银行单票模式、`live` 或
`stale-cache` 可用性、缓存状态、`found=true` 以及 `200340/200341` 来源。
full 因此从 12 项增至 13 项；失败检查还会保存最多 4 KiB 响应摘要，便于
区分业务断言失败和瞬时上游错误。当前正式服务 full 13/13 通过，全量 CTest
42/42 通过，最新部署和容错证据见
[分时资金与共享 JSN 上游容错](2026-08-06-native-upstream-resilience.md)。
