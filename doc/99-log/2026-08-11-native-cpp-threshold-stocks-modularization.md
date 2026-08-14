# 原生 C++ 百元股与千亿市值链路模块化

日期：2026-08-11

## 目标

拆解 634 行的 `threshold_stocks.cpp`。原文件混合两类 universe、八个别名、四种 view、
11 项排序、三类资源、三类归一化、期间选择、成员/趋势对账、缓存、筛选、响应和 CLI。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `threshold_stocks_catalog.cpp` | 100 | universe、别名、view 与排序目录 |
| `threshold_stocks_support.cpp` | 152 | JSN 字段、市场身份、状态和公共转换 |
| `threshold_stocks_normalize_history.cpp` | 70 | 两类历史主表及趋势归一化 |
| `threshold_stocks_normalize_members.cpp` | 40 | universe 专属成员字段归一化 |
| `threshold_stocks_sort.cpp` | 82 | 两个排序域及 11 项类型化排序策略 |
| `threshold_stocks_summary.cpp` | 67 | 期间选择、历史摘要和成员对账 |
| `threshold_stocks_query_plan.cpp` | 45 | 查询规范化、身份校验和排序计划 |
| `threshold_stocks_fetch.cpp` | 27 | JSN 缓存与抓取 |
| `threshold_stocks_service.cpp` | 146 | 主表、趋势、成员、筛选、分页与响应编排 |
| `threshold_stocks_command.cpp` | 75 | CLI 参数、证券目录装载和输出 |

`UniverseSpec` 统一绑定公开 id、中文标签、主资源及趋势计数字段，`ViewSpec` 描述排序域、单票
要求和成员加载策略，`SortSpec` 管理历史/成员两个域的 11 项排序。universe、主资源、别名、
view 和排序 id 均有编译期唯一性检查。动态 `bygtj1/bygtj3` 资源只从所选期间的
`detail_key` 派生，服务层不再散落 universe/view/sort 字符串条件链。

## 增量验证

- `tdx-threshold-stocks-tests` 与 `tdx-tool` 构建通过；
- 专项测试覆盖两类历史桶、空计数归零、成员身份/状态、`spj/gjjz` 分 universe 类型、趋势
  排序及非法排序；
- 真实 `high-price/members` 样本选中 `20260810`，返回 233 个成员、119 个趋势点，抽取
  3 行；活跃 228、入围 3、跌出 5，主表与趋势两项计数对账均通过；
- 样本保存在 `output/threshold-stocks-refactor-sample.json`，schema 保持
  `tdx-market-threshold-stocks-native-v1`；
- 未修改共享 JSN 解析、HTTP 路由或 schema，按增量验证规则未运行完整 CTest。

## 后续候选

`level2.cpp` 继续搁置；共享 JSN、解释器序列/动态上下文与 TPool 解析需要更宽回归。下一低
风险候选为 630 行的 `native/src/reverse_repo.cpp`。
