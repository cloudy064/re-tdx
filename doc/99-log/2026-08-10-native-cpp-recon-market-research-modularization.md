# 原生 C++ 市场研究契约验证器模块化

日期：2026-08-10

## 结果

`recon_contract_market_research.cpp` 从 2,005 行的单函数条件链缩减为 27 行策略调度器。
41 个市场研究契约按领域拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `recon_contract_market_research_internal.hpp` | 39 | 类型化研究契约验证器端口 |
| `recon_contract_market_research.cpp` | 27 | 五策略固定顺序调度 |
| `recon_contract_market_research_catalog.cpp` | 599 | 预测、经济指标、主题与战略机会目录 |
| `recon_contract_market_research_signals.cpp` | 523 | 板块轮动、涨停梯队、阈值股与强势股 |
| `recon_contract_market_research_events.cpp` | 477 | 商品联动、公告、逆回购、要约与监管事件 |
| `recon_contract_market_research_institutions.cpp` | 266 | 活跃龙虎榜和国企改革关系 |
| `recon_contract_market_research_intelligence.cpp` | 234 | 价值关注、失信、安全亮点与单票亮点 |

主调度器使用 `std::array<MarketResearchContractValidator, 5>` 实现职责链。公共入口、
断言内容、契约请求和 JSON schema 保持不变。

## 等价性与验证

- 五个领域实现区段逐字符核对通过；
- 拆分前后的 41 个唯一契约 ID 集合完全一致；
- 五个领域验证器全部进入类型化策略注册表；
- `tdx-recon-contract-tests`、`tdx-tool` 编译链接通过，专项测试通过；
- 正式服务选择 `economic-indicators-catalog-live`、`limit-ladder-live`、
  `intelligence-highlights-live`，结果 3/3 通过；报告保存在
  `output/recon-market-research-modularization-contracts.json`，结构证据保存在
  `output/recon-market-research-modularization.json`；
- 当前目录不是 Git 工作树，因此未执行 `git diff --check`。

依据增量验证规则未运行完整契约套件或 CTest。本轮只调整契约求值代码的组织结构。
