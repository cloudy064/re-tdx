# 原生 C++ 涨跌停复盘链路模块化

日期：2026-08-11

## 目标

拆解 644 行的 `limit_review.cpp`。原文件同时承担五项固定资源、两项日期资源、一项单票历史
资源、五类行归一化、六种 view、分类校验、缓存、缺失资源容错、筛选、响应和 CLI，资源选择
与 view/category 条件链互相交织。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `limit_review_catalog.cpp` | 94 | 五项固定资源、六种 view 及公开目录 |
| `limit_review_support.cpp` | 201 | 字段转换、证券身份、缺失来源、筛选和公共工具 |
| `limit_review_normalize_current.cpp` | 56 | 当日涨停、跌停和冲板记录归一化 |
| `limit_review_normalize_history.cpp` | 105 | 年度、市场温度和单票历史归一化 |
| `limit_review_normalize_daily.cpp` | 37 | 指定日期涨跌停成员归一化 |
| `limit_review_query_plan.cpp` | 128 | 参数规范化、校验和类型化资源计划 |
| `limit_review_fetch.cpp` | 37 | JSN 抓取、缓存和动态资源缺失容错 |
| `limit_review_service.cpp` | 73 | 通用计划执行、汇总与响应组合 |
| `limit_review_command.cpp` | 73 | CLI 参数、证券目录装载和输出 |

`ViewSpec` 集中管理 view、分类、资源说明和语义，`ResourceSpec` 将五项固定资源绑定到
`NormalizeKind`；资源路径、归一化类型和 view 均有编译期唯一性检查。`QueryPlan` 在一次
规范化中决定允许的分类、固定/动态资源、归一化策略、缺失容错和单票历史资源，服务层只需
循环执行计划。公开头文件、命令参数、缓存边界和 JSON schema 保持不变。

## 增量验证

- `tdx-limit-review-tests` 与 `tdx-tool` 构建通过；
- 专项测试覆盖北交所身份、涨停基因、年度统计、成交额单位、连板分布、指定日记录，以及
  市场历史和证券过滤的无效组合；
- 真实 `current/all` 样本从三路资源取得 198 行，匹配 198 行、抽取 3 行，availability 为
  `live`；
- 样本保存在 `output/limit-review-refactor-sample.json`，schema 保持
  `tdx-market-limit-review-native-v1`；
- 未修改共享 JSN 解析、传输、HTTP 路由或 schema，按增量验证规则未运行完整 CTest。

## 后续候选

`level2.cpp` 继续搁置，`jsn.cpp` 是共享解析器，`formula_functions_series.cpp` 属共享解释器
语义，二者都需要更宽回归。下一低风险候选为 639 行的 `native/src/relative_valuation.cpp`。
