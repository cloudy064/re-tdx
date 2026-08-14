# 原生 C++ 公式基础契约模块化

日期：2026-08-11

## 目标

拆解 715 行的 `recon_contract_formula_foundation.cpp`。原文件用一个条件链同时验证图标资源、
379 项公式覆盖率、解释器能力、服务健康、功能目录和 OpenAPI；其中约 300 行重复读取 39 组
能力数组。

## 落地结果

原根文件已移除，并拆为六个语义单元：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `recon_contract_formula_foundation_catalog.cpp` | 46 | 五项基础契约类型目录与分派 |
| `recon_contract_formula_foundation_icons.cpp` | 34 | DRAWICON 原生资源契约 |
| `recon_contract_formula_foundation_coverage.cpp` | 55 | 公式数值与绘图覆盖率契约 |
| `recon_contract_formula_foundation_capabilities.cpp` | 235 | 解释器能力数组、计数和注册证据契约 |
| `recon_contract_formula_foundation_service.cpp` | 69 | 健康状态与功能目录契约 |
| `recon_contract_formula_foundation_openapi.cpp` | 69 | 命令端点、固定路径和 POST 操作契约 |

五项契约由带函数指针的 `FoundationContract` 目录分派，并编译期检查 ID 唯一性。39 组
`custom_formula_*` 数组、对应计数字段和 `CapabilitySets` 成员由同一张成员指针绑定表关联；
实际集合、预期集合与预期计数从同一绑定派生，替代 39 段重复读取和 39 段独立计数条件。
固定功能、功能到 API 映射、OpenAPI 路径和 POST 路径也分别集中为类型化目录。

## 增量验证

- `tdx-recon-contract-tests` 与 `tdx-tool` 增量构建通过；
- `formula-evaluation` 与 `formula-render-workflow` 两个契约测试域通过；
- 独立 8879 临时服务上的 `health`、`features`、`openapi`、`formula-icons` 四个真实 HTTP
  契约全部通过；服务限制为五次请求并已自动退出；
- 本轮未改变生产 API、解析、传输或缓存，未运行完整 CTest。

## 后续候选

物理最大文件仍是已搁置的 727 行 `level2.cpp`。排除 L2 后，713 行 `jsn.cpp` 是共享解析器，
拆分需要更广回归；优先的低风险候选是 707 行的
`native/src/recon_contract_market_data_bonds.cpp`。
