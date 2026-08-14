# 原生 C++ 交易所基金链路模块化

日期：2026-08-11

原 857 行 `exchange_funds.cpp` 同时维护 22 个 JSN 资源常量、多套证券字段规则、11 类基金
归一化、实时行情派生公式、两级缓存、查询汇总和 CLI。现拆分如下：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `exchange_funds_internal.hpp` | 22 项资源、11 种类型的编译期目录与内部端口 | 102 |
| `exchange_funds_catalog.cpp` | 资源→类型、视图→类型、标签和排序映射 | 67 |
| `exchange_funds_support.cpp` | JSON 标量、证券身份、市场路由、本地 JSN 和来源摘要 | 249 |
| `exchange_funds_normalize.cpp` | ETF/LOF/封闭基金/货币基金/REITs 归一化 | 203 |
| `exchange_funds_quotes.cpp` | L1 行情合并、规模、溢价及套利年化派生 | 85 |
| `exchange_funds_fetch.cpp` | 22 资源抓取、本地回退及主数据/行情缓存 | 96 |
| `exchange_funds_service.cpp` | 过滤、类型排序、统计、行情容错和文档组合 | 208 |
| `exchange_funds_command.cpp` | CLI 参数与文件输出 | 56 |

旧根文件已移除。原先重复的字符串条件映射改为两张编译期类型目录；公开查询对象、服务接口、
字段、单位和 `tdx-market-exchange-funds-native-v1` schema 保持不变。

增量验证：`tdx-exchange-funds-tests` 与 `tdx-tool` 构建、测试通过。真实 ETF 份额榜返回
458 条匹配、22 个来源，抽取 3 条均为 `etf-share-ranking`，无行情错误；样本位于
`output/exchange-funds-refactor-ranking.json`。未运行完整 CTest。
