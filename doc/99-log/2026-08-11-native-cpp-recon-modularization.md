# 原生 C++ 对账与诊断编排模块化

日期：2026-08-11

## 目标

拆解 687 行的 `recon.cpp`。原文件同时承担安装目录扫描、清单渲染、API 响应校验、HTTP
契约巡检、长篇 CLI 帮助和本地 doctor，职责之间没有清晰物理边界；安装命令还重复实现了
`inventory_document()` 的扫描与投影逻辑。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `recon_inventory.cpp` | 81 | EXE/DLL 扫描、插件目录策略、清单 JSON 投影 |
| `recon_install_command.cpp` | 67 | 安装盘点参数、Markdown/JSON 输出与落盘 |
| `recon_contract_response.cpp` | 133 | HTML、PNG、SSE、JSON 响应入口校验与领域分派 |
| `recon_contract_audit.cpp` | 212 | 契约选择、依赖路径、HTTP 重试和审计汇总 |
| `recon_contract_command.cpp` | 72 | 契约巡检 CLI 参数和输出 |
| `doctor_command.cpp` | 44 | 本地安装完整性诊断 |

安装命令现在直接复用 `inventory_document()`，消除一份扫描实现。`api-contracts --help` 不再
硬编码 160 余行易过期文本，而是从 `ApiContractSpec` 类型化目录生成；当前 224 个帮助项与
目录条目数量一致。HTTP 巡检中的报告期依赖、强势股动态区间和三次重试被抽为具名内部策略，
公开函数签名、返回 schema 和命令入口不变。

## 增量验证

- `tdx-recon-contract-tests` 与 `tdx-tool` 构建通过；
- `market-core` 定向契约域通过；
- `api-contracts --help` 输出 224 个目录生成项，与 `recon_contract_catalog.cpp` 的 224 项一致；
- 未改变共享 JSON/JSN 解析器、传输实现或公开 schema，按短回归规则未运行完整 CTest。

## 后续候选

`level2.cpp` 继续按要求搁置；共享 `jsn.cpp` 需要更宽回归门禁。下一低风险候选为 685 行的
`native/src/research.cpp`。
