# 原生 C++ 债券市场契约模块化

日期：2026-08-11

## 目标

拆解 707 行的 `recon_contract_market_data_bonds.cpp`。原文件用一个条件链维护六类债券参考
契约和四类可转债契约，混合主表/投影对账、单位审计、申购公式、可交换债补充与定价验证。

## 落地结果

原根文件已移除，十项契约进入编译期唯一的 `BondContract` 类型目录：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `recon_contract_market_data_bonds_catalog.cpp` | 50 | 十项契约目录与统一分派 |
| `recon_contract_market_data_bonds_reference.cpp` | 160 | 全市场、企业债和私募债投影边界 |
| `recon_contract_market_data_bonds_rated.cpp` | 145 | AA+、国债和政策性金融债 |
| `recon_contract_market_data_bonds_pending.cpp` | 89 | 待发可转债及两套客户端投影 |
| `recon_contract_market_data_bonds_subscriptions.cpp` | 123 | 申购主表、公式与新债投影 |
| `recon_contract_market_data_bonds_exchangeable.cpp` | 101 | 可交换债补充与核心条款 |
| `recon_contract_market_data_bonds_pricing.cpp` | 71 | 可转债证券对、估值和来源 |

原公开验证入口和全部断言名称、阈值、来源资源及 schema 保持不变。新增契约只需在目录绑定
一个独立验证器，不再扩展跨 700 行的条件链。

## 增量验证

- `tdx-recon-contract-tests` 与 `tdx-tool` 增量构建通过；
- `market-core` 域直接覆盖九项债券契约及关键反例；
- `calendar-resilience-web` 域覆盖政策性金融债投影与亿元到元换算；
- 两个定向域均通过，未改变共享解析、传输、缓存或生产 API，因此未运行完整 CTest。

## 后续候选

排除 L2 和需要扩大回归的共享 `jsn.cpp` 后，下一低风险候选为 698 行的
`native/src/convertible_bonds_service.cpp`。
