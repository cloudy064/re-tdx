# 原生 C++ 企业与精选数据契约模块化

日期：2026-08-11

## 目标

拆解 810 行的 `recon_contract_market_corporate_catalog.cpp`。该文件并非单一公司研究逻辑，
而是七个互相独立的 API 响应契约及一条持续增长的 `else if` 分派链。

## 落地结果

根文件缩至 46 行，使用带编译期重复 ID 检查的类型化契约目录。七个实现分别承载：

| 契约 | 实现文件 | 行数 |
|---|---|---:|
| `special-situations-live` | `recon_contract_market_special_situations.cpp` | 126 |
| `corporate-transitions-live` | `recon_contract_market_corporate_transitions.cpp` | 121 |
| `exchange-funds-live` | `recon_contract_market_exchange_funds.cpp` | 191 |
| `etf-share-ranking-live` | `recon_contract_market_etf_share_ranking.cpp` | 68 |
| `curated-data-live` | `recon_contract_market_curated_data.cpp` | 165 |
| `special-attention-live` | `recon_contract_market_special_attention.cpp` | 128 |
| `fund-statistics-live` | `recon_contract_market_fund_statistics.cpp` | 126 |

测试入口新增 `--domain <name>`。无参数执行仍覆盖全部七个测试域，CTest 行为不变；增量开发
可以只运行受影响域。

## 增量验证

- `tdx-recon-contract-tests` 增量构建通过；
- `--domain realtime-corporate` 通过，覆盖特殊情形、企业变迁、交易所基金和 ETF 份额契约；
- `--domain curated-issuance` 通过，覆盖精选数据、特别关注和基金统计契约；
- 两个域均保留成功样本与关键错误样本，七个迁移契约全部被直接执行；
- 未改变契约断言、公开 API schema 或 CTest 默认入口，因此未运行完整测试套件。

## 后续候选

当前最大生产实现为 794 行的 `native/src/special_situations.cpp`。
