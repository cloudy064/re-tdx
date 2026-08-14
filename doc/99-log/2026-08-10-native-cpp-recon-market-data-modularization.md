# 原生 C++ 市场数据契约验证器模块化

日期：2026-08-10

## 结果

`recon_contract_market_data.cpp` 从 2,204 行的单函数条件链缩减为 27 行的策略调度器。
57 个市场数据契约按领域拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `recon_contract_market_data_internal.hpp` | 39 | 类型化验证器签名和五个领域端口 |
| `recon_contract_market_data.cpp` | 27 | 固定顺序的 Chain of Responsibility 注册表 |
| `recon_contract_market_data_quote.cpp` | 395 | 行情、估值、异常空结果和分时资金契约 |
| `recon_contract_market_data_institution.cpp` | 552 | 个股全景、机构、股东跨票和专属基金契约 |
| `recon_contract_market_data_flow.cpp` | 479 | 情报、两融、互联互通、持股与股东人数契约 |
| `recon_contract_market_data_session.cpp` | 165 | 涨跌停复盘和交易时段成交额契约 |
| `recon_contract_market_data_bonds.cpp` | 707 | 债券参考库、申购、补充条款和定价契约 |

主调度器只遍历 `std::array<MarketDataContractValidator, 5>`；新增领域契约不再修改一个
两千行函数。公共契约入口、断言语义、请求目录和 JSON schema 均未改变。

## 等价性与验证

- 五个领域实现区段逐字符核对通过；
- 原有 57 个唯一契约 ID 与拆分后集合完全一致；
- 五个验证器全部进入类型化策略注册表；
- `tdx-recon-contract-tests` 和 `tdx-tool` 编译、链接通过；
- `tdx-recon-contract-tests` 通过；
- 正式服务上选择 `invalid-market`、`stock-panorama-live`、
  `convertible-bond-pricing-live` 三项契约，结果 3/3 通过；报告保存在
  `output/recon-market-data-modularization-contracts.json`，结构证据保存在
  `output/recon-market-data-modularization.json`；
- 当前目录不是 Git 工作树，因此未执行 `git diff --check`。

根据增量验证规则未运行完整契约套件或 CTest。此次只移动验证实现，没有修改服务、
传输、缓存或 API schema。
