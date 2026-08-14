# 原生 C++ 个股/行业估值链路模块化

日期：2026-08-11

## 目标

拆解 671 行的 `equity_valuation.cpp`。原文件同时承载日期与数值解析、证券/行业身份投影、
五类行归一化、五套 PBRPC 参数模板、缓存重试、响应组合和 CLI；view 的别名、请求号、
配置文件、归一化与排序规则分散在多条字符串条件链中。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `equity_valuation_catalog.cpp` | 69 | 五类 view、十个别名、请求号与策略绑定 |
| `equity_valuation_support.cpp` | 184 | 日期、数值、字段和市场基础转换 |
| `equity_valuation_projection.cpp` | 124 | 证券/行业身份、判断、预测、来源与方法论 |
| `equity_valuation_normalize_industry.cpp` | 29 | PE 与 PB/ROE 行业归一化 |
| `equity_valuation_normalize_security.cpp` | 66 | 历史 PE、PE 成分和 PB/ROE 成分归一化 |
| `equity_valuation_normalize.cpp` | 38 | 策略调用、身份去重与统一排序 |
| `equity_valuation_request.cpp` | 84 | 查询约束与五套请求参数模板 |
| `equity_valuation_service.cpp` | 173 | 缓存、PBRPC 重试、截断和响应组合 |
| `equity_valuation_command.cpp` | 67 | CLI 参数与文件输出 |

`ViewSpec` 现在统一绑定规范 id、请求号、配置文件、视图属性、排序类型和行归一化函数；
view id、PBRPC request id 与别名均有编译期唯一性检查。固定规则使用枚举和函数端口组合，
没有增加无状态继承层。公开头文件、构造函数、命令参数与 JSON schema 保持不变。

## 增量验证

- `tdx-equity-valuation-tests` 与 `tdx-tool` 构建通过；
- 专项测试覆盖五种行结构、证券/行业名称解析、日期排序、预测判断枚举和重复身份拒绝；
- 真实 `pe-industries` 样本通过请求 `200302` 从 `gp_gz_peg.xml` 获取 30 行，上限输出 3 行，
  availability 为 `live`，首个行业为“煤炭”；
- 输出保存在 `output/equity-valuation-refactor-sample.json`，schema 保持
  `tdx-equity-valuation-native-v1`；
- 未改共享 PBRPC、服务路由或 schema，按增量验证规则未运行 HTTP 契约和完整 CTest。

## 后续候选

`level2.cpp` 继续搁置，共享 `jsn.cpp` 需要宽回归；下一低风险候选为 669 行的
`native/src/capital_strength.cpp`。
