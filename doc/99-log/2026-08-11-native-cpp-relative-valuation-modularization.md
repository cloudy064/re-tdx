# 原生 C++ 相对估值链路模块化

日期：2026-08-11

## 目标

拆解 639 行的 `relative_valuation.cpp`。原文件混合五类指数、五个基准、三种估值方法、日期
窗口、证券投影、两类归一化、TQLEX 主表、PBRPC 明细、两级缓存、重复重试、响应和 CLI。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `relative_valuation_catalog.cpp` | 88 | 指数类型、基准、估值方法及别名目录 |
| `relative_valuation_support.cpp` | 150 | 日期、字段、数值和 CLI 公共转换 |
| `relative_valuation_projection.cpp` | 165 | 市场身份、分位标签、来源、参数和摘要投影 |
| `relative_valuation_normalize.cpp` | 61 | 主表与历史行归一化、唯一性和排序 |
| `relative_valuation_query_plan.cpp` | 46 | 查询规范化、日期窗口、替换表和缓存键 |
| `relative_valuation_fetch.cpp` | 93 | 统一缓存/重试模板及两阶段传输策略 |
| `relative_valuation_service.cpp` | 90 | 主表选择、历史截断和响应组合 |
| `relative_valuation_command.cpp` | 60 | CLI 参数、证券目录装载和输出 |

`IndexTypeSpec`、`BenchmarkSpec`、`MethodSpec` 和类型化别名表替代字符串条件链，固定 id/code
均有编译期唯一性检查。`QueryPlan` 一次生成规范化参数、日期窗口、TQLEX/PBRPC 替换表与缓存
键。主表 `200000` 和明细 `200001` 通过同一个 `fetch_cached` 模板执行三次重试及陈旧缓存
回退，消除了两段重复控制流；无状态策略没有引入不必要的继承层级。

## 增量验证

- `tdx-relative-valuation-tests` 与 `tdx-tool` 构建通过；
- 专项测试覆盖名称解析、比例/分位、历史排序、缺失值、重复日期、内部市场和非法市场；
- 真实完整路径返回 180 个指数，选中 `SH000019` 后取得 483 个历史点并保留最近 3 点，
  两路来源均成功且 availability 为 `live`；
- 样本保存在 `output/relative-valuation-refactor-sample.json`，schema 保持
  `tdx-relative-valuation-native-v1`；
- 未修改共享云响应解析、传输、HTTP 路由或 schema，按增量验证规则未运行完整 CTest。

## 后续候选

`level2.cpp` 继续搁置；`jsn.cpp`、`formula_functions_series.cpp` 与 `tpool.cpp` 分别涉及共享
JSN、解释器序列语义和 XML/公式兼容解析，需要更宽回归。下一低风险候选为 635 行的
`native/src/anomaly_risk.cpp`。
