# TPool 内联 XML 规则解释与网页导入

> 日期：2026-08-08。范围：非 L2、纯 C++ TPool/TCalc 解释器服务面。

## 缺口审计

本轮先重新核对解释器与资源缺口，而不是继续增加展示页面：

- TCalc 当前 379/379 条公式都有正文或原生恢复体，公开非 L2 公式运行审计为
  0 个解释器错误；剩余 20 条依赖合法 L2 或券商私有序列，不用零值伪造；
- 当前 JSN 镜像 580 个文件全部匹配类型化命令，解析错误和通用独占均为 0；
- 功能目录误把已经存在固定 API 的 `formulas context-template` 和
  `pool evaluate` 标为 `CLI only`；
- 当前安装没有 `tpool/*.xml`，导致已有 TPool 网页虽然能执行安装目录内 XML，
  实际只能显示空目录。

## 实现

新增 `evaluate_tpool_xml_document`，把解析后的 inspection 与原文件模式共用同一
求值函数。它继续执行：

- TCalc 快速计算或恢复源码解释器；
- ST、停牌和退市过滤；
- 比较、交叉、拐点和 5/6/7 跨证券排名；
- 无环 flow 的只读拓扑投影。

固定接口 `/api/v1/pools/evaluate` 现在有两种输入：

- GET：只接受通达信根目录下真实存在的 XML；
- POST：必须携带 `X-TDX-Action: pool-evaluate`，JSON 提交最多 512 KiB 的
  `xml` 和安全 `source_name`。

POST 不接受服务器文件路径、不创建临时文件、不回显 XML，并固定报告
`request_body_retained=false`、`read_only=true`、`tdx_state_mutated=false`。
整个本地服务 POST 上限仍有界，统一由 64 KiB 调整为 1 MiB，以容纳转义后的
512 KiB XML；具体业务字段和证券数量仍由原有更严格上限约束。

Svelte TPool 实验室新增 XML 粘贴和本地文件导入。浏览器只把正文发送给同源
本地 C++ 服务，不把本机路径发送给服务端。持续 `pool watch` 仍保持 CLI-only，
因为它涉及长时间运行和独立状态文件，不应伪装成一次 HTTP 请求。

功能目录同时修正：

- `formulas context-template` → `/api/v1/formulas/context-template`；
- `pool evaluate` → `/api/v1/pools/evaluate`。

## 真实验证

内联合成池包含平安银行 `SZ000001`、`MACD.DIF > -999999` 和 `1 → 2` 单次
flow。临时与正式服务均完成 1 个证券、1 条规则，规则命中且流程成功投影；
响应没有 `xml/request_body`。缺少确认头的 POST 返回 405。

- 原生全量 CTest：98/98；
- Svelte 检查：0 error / 0 warning；生产构建通过；
- 临时专项：4/4；临时全量：168/168；
- 正式专项：4/4；
- 正式全量首轮：166/168。旧逆回购与个股估值接口分别遇到公开 L1、财务连接
  瞬时关闭，HTTP 仍为 200 但按严格业务契约判失败；单独重试 2/2 后，全量复跑
  168/168。该证据保留，未放宽契约去接受降级结果。

正式服务 PID `3556`，可执行文件 SHA-256
`43DE249FA0368DB240ACF5D5836C8D1ADA46E8C622BAC8C80E441F2A7A5F59F1`；前端
入口引用 `assets/index-Cmt0b2nt.js` 和 `assets/index-CoWsYwNK.css`。

产物：

- `output/tdx-jsn-discovery-20260808-late.json`
- `output/tdx-jsn-candidates-20260808-late.json`
- `output/probes/api-contracts-tpool-inline-temp-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-tpool-inline.json`
- `output/probes/api-contracts-tpool-inline-formal-20260808.json`
- `output/probes/api-contracts-full-formal-20260808-tpool-inline.json`
- `output/probes/api-contracts-tpool-inline-formal-transient-retry-20260808.json`
- `output/probes/api-contracts-full-formal-20260808-tpool-inline-retry.json`

## 保留边界

内联 POST 只解决“没有安装目录池也能使用解释器”的输入缺口。原版 worker、声音、
弹窗、板块保存、历史文件、宿主回调、循环 watch 和通达信 XML 写回仍不通过 HTTP
执行。安装目录尚无真实多节点用户池，因此本轮也不声称完成原版 UI 逐轮差分。

