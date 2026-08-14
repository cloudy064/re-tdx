# 原生 C++ 大宗交易链路模块化

日期：2026-08-11

原 848 行 `block_trades.cpp` 同时承担三项核心资源、四周期营业部目录、证券/意向/月度/行业/
营业部归一化、主与明细缓存、四种查询模式和 CLI。现拆分如下：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `block_trades_internal.hpp` | 3 项核心资源、4 个营业部周期、4 种视图目录及内部端口 | 78 |
| `block_trades_support.cpp` | JSON 标量、市场/证券、过滤、来源、时间和参数支持 | 224 |
| `block_trades_normalize_core.cpp` | 成交、证券历史、意向和月度归一化 | 144 |
| `block_trades_normalize_analysis.cpp` | 行业、行业证券、营业部排行和营业部明细归一化 | 137 |
| `block_trades_fetch.cpp` | 核心批量抓取、主缓存、明细/失败缓存 | 125 |
| `block_trades_service.cpp` | 证券、营业部、行业与目录查询编排及结果组合 | 278 |
| `block_trades_command.cpp` | CLI 参数与文件输出 | 72 |

旧根文件已移除。固定资源、周期与视图由编译期类型目录统一管理；公开接口、金额/股数单位、
缓存语义和 `tdx-market-block-trades-native-v1` schema 保持不变。

同时把大宗交易断言从 1,564 行的通用 `native_tests.cpp` 迁入 139 行的独立
`tdx-block-trades-tests`，通用测试降为 1,471 行，测试目录已无超过 1,500 行的 `.cpp`。
专项测试与 `tdx-tool` 构建、执行通过。真实目录样本返回 13 个月度点、280 条近期成交、
280 只证券、三项核心来源且无错误，位于 `output/block-trades-refactor-catalog.json`。

平安银行证券模式另验证到一条成交历史；其意向明细 `dzjy13/0000001.jsn` 当前由上游返回
零长度，服务按原有容错语义返回 `partial`，证据位于
`output/block-trades-refactor-sz000001.json`。本轮未运行完整 CTest。
