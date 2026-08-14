# 原生 OpenAPI 与功能注册表完整性闭环

日期：2026-08-10

## 修复内容

静态审计确认四条已实现、已在网页使用且已进入市场路由表的接口没有出现在
`openapi.json`：

- `/api/v1/market/employees`；
- `/api/v1/market/special-situations`；
- `/api/v1/market/exchange-funds`；
- `/api/v1/market/curated-data`。

根因是功能注册表、运行时路由表和 OpenAPI 摘要表分别维护，新增功能时漏改了第三处。
`server_catalog.cpp` 现在先从 `command_registry()` 自动登记所有非空 API 端点，
再由显式目录覆盖更具体的摘要并补充没有 CLI 命令的资源路径。

一个命令可以登记多个逗号分隔路径；生成器会逐项 `trim` 后登记，不能生成包含逗号
的伪路径。`recon` 的 OpenAPI 契约使用同一规范化规则，逐个验证功能注册表中的
全部端点，从机制上防止相同遗漏复发。

## 验证

- `tdx-recon-contract-tests` 通过；
- `tdx-tool` 增量链接通过；
- 临时候选服务 `127.0.0.1:18766` 的单项 `openapi` 契约为 1/1，200 条断言全部
  通过；
- 四条缺失路径均存在并带有对应功能标题；
- 多路径命令的 `/api/v1/cloud/workflows` 与 `/api/v1/cloud/workflow` 分别存在，
  错误的逗号拼接路径不存在；
- 证据：`output/api-contract-openapi-catalog-v32.json`；
- 候选服务验证后已停止，正式 8765 服务未重启、未替换。

本次只修改能力目录与契约完整性，没有请求四个数据端点，也没有改动它们的返回
schema、缓存或上游访问逻辑。
