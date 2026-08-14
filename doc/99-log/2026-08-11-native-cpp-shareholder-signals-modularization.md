# 原生 C++ 股东信号链路模块化

日期：2026-08-11

## 目标

拆解 661 行的 `shareholder_signals.cpp`。原文件混合五项资源、字段/单位转换、四套 CFG
信号公式、牛散目录、逐人持仓、本地 JSN、远端回退、六种 view、八种排序、缓存、筛选、
对账、响应和 CLI；资源到类型、标签及公式的映射分散在多条条件链中。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `shareholder_signals_catalog.cpp` | 98 | 五资源、六 view、八排序及四个函数策略绑定 |
| `shareholder_signals_support.cpp` | 197 | 数值公式、证券身份、本地 JSN、来源与公共转换 |
| `shareholder_signals_normalize.cpp` | 140 | 牛散、机构、调研和小市值四类信号归一化 |
| `shareholder_signals_investor.cpp` | 74 | 牛散目录与逐人持仓归一化 |
| `shareholder_signals_query_plan.cpp` | 50 | 查询规范化与类型化排序 |
| `shareholder_signals_fetch.cpp` | 65 | 五资源本地优先/远端回退与主缓存 |
| `shareholder_signals_investor_projection.cpp` | 91 | 目录筛选、动态持仓获取及合计对账 |
| `shareholder_signals_service.cpp` | 100 | 信号筛选、汇总和响应组合 |
| `shareholder_signals_command.cpp` | 58 | CLI 参数与输出 |

`ResourceSpec` 统一绑定资源路径、类型、公开 id、标签和行归一化函数；`ViewSpec` 与 `SortSpec`
分别管理六种 view 和八种排序。资源路径、资源类型、公开 id、view 与 sort 均有编译期唯一性
检查。四类公式使用真实函数策略，不再依赖 kind 字符串条件链；本地 JSN 优先及远端回退、
公开构造函数、命令参数和 schema 保持不变。

## 增量验证

- `tdx-shareholder-signals-tests` 与 `tdx-tool` 构建通过；
- 专项测试覆盖持股比例/市值、机构与股东变化、小市值三项 CFG 比例、调研成长、牛散目录及
  逐人持仓差额；
- 真实样本返回 1,085 条信号：牛散 912、机构增持 45、调研成长 82、小市值机构 46；
- 牛散目录为 1,755 人，五路来源均为本地 JSN，抽取 3 行且 availability 为 `live`；
- 样本保存在 `output/shareholder-signals-refactor-sample.json`，schema 保持
  `tdx-market-shareholder-signals-native-v1`；
- 未修改共享 JSN 解析、HTTP 路由或 schema，按增量验证规则未运行完整 CTest。

## 后续候选

`level2.cpp` 继续搁置，共享 `jsn.cpp` 需要宽回归；解释器语义文件
`formula_functions_series.cpp` 也需要公式域宽回归。下一低风险候选为 644 行的
`native/src/limit_review.cpp`。
