# 原生 C++ API 契约测试模块化

日期：2026-08-11

## 问题

`recon_contract_tests.cpp` 原有 3,883 行，健康检查、公式解释器、渲染 IR、市场数据、
研究信号、公司事件和网页路由契约全部位于一个 `main()`。契约数量增长后，失败位置和
重编译边界都不再清晰。

## 新结构

| 单元 | 契约域 | 行数 |
|---|---|---:|
| `recon_contract_tests.cpp` | 七域类型化调度和错误域标注 | 40 |
| `recon_contract_test_support.cpp/.hpp` | 公共断言和公式 K 线 fixture | 33/29 |
| `recon_contract_formula_evaluation_tests.cpp` | 公式求值、上下文和核心语义 | 1,115 |
| `recon_contract_formula_workflow_tests.cpp` | 渲染、云计算、TPool、扫描与回测 | 605 |
| `recon_contract_market_core_tests.cpp` | 基金、机构、两融、互联互通和债券 | 500 |
| `recon_contract_research_signals_tests.cpp` | 预测、经济指标、主题与选股信号 | 423 |
| `recon_contract_realtime_corporate_tests.cpp` | 实时流、解禁、发行和交易所基金 | 260 |
| `recon_contract_curated_issuance_tests.cpp` | 精选数据、特殊关注与 IPO 生命周期 | 390 |
| `recon_contract_calendar_resilience_tests.cpp` | 日历、机构补充、JSN 韧性与网页路由 | 340 |

入口通过 `constexpr std::array<ContractDomain, 7>` 维持固定执行顺序。异常会附加对应域名，
但仍只启动一个 `tdx-recon-contract-tests` 进程。

## 完整性与验证

- 七个连续区间覆盖原 `main()` 的全部契约主体，未跨越局部变量依赖；
- 当前仍有 288 个 `require()` 断言调用；
- 所有九个翻译单元编译通过；
- `tdx-recon-contract-tests` 完整执行通过；
- 本次只改变测试组织，没有运行完整 CTest。

拆分后测试目录最大文件降至 1,564 行，已不存在超过 3,000 行的测试 `.cpp`。
