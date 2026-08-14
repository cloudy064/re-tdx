# 原生 C++ 公式证券关系上下文二次模块化

日期：2026-08-11

## 目标

继续拆分此前从 `formula_context.cpp` 抽出的 1,137 行证券关系实现，使公式解释器的跨证券
时序、证券身份、期权、行业估值和行业指数不再集中在同一翻译单元，同时保持宿主字段语义
和上下文契约不变。

## 结构调整

| 文件 | 行数 | 职责 |
|---|---:|---|
| `formula_context_relation_industry.cpp` | 368 | 行业归属、HYSYL/HYSJL、行业指数与文字序列 |
| `formula_context_relation_series.cpp` | 357 | BETA、INDEX*、EXTERNAL# 与 IVOLAT 时序 |
| `formula_context_relation_identity.cpp` | 258 | 主指数、地域、概念、期权/转债标的身份 |
| `formula_context_relations.cpp` | 140 | 静态关系、时序关系和 BKJYONE 组合入口 |
| `formula_context_relation_support.cpp` | 51 | JSON 字段读取与依赖族判定 |

新增内部目录：

- `formula_context_relations_detail.hpp`：声明跨实现单元的最小内部端口；
- `formula_context_relations_catalog.hpp`：集中 30 个依赖符号、8 个行业序列字段、33 个地区
  名称和 13 个宽基指数映射。

这里继续使用函数组合和只读类型目录；没有为无状态公式绑定器增加继承层级。

## 兼容性

- `formula_context_relations_internal.hpp` 的公开内部组合入口未改变；
- DPZSCODE/DPZSNAME、UNDERCODE/UNDERLYC、BETA、INDEX*、IVOLAT、HYSYL/HYSJL、
  HY_INDEX*、DYBLOCK 与 GNBLOCK 的绑定逻辑保持不变；
- float32 收窄、日期时间对齐、近零值延续和公开 HYZT 回退边界保持不变；
- 公式求值结果 schema 没有改变。

## 增量验证

- `tdx-formula-engine-tests` 与 `tdx-tool` 使用新实现重新编译、链接通过；
- `tdx-formula-engine-tests.exe` 执行通过；
- `sh:110075` 关系探针返回 120 根日线和 8 个输出；
- 主指数代码为 `999999`，标的代码为 `600029`，最后一根 UNDERLYC 为
  `5.150000095367432`；
- 样例保存为 `output/formula-relations-refactor-sample.json`；
- 没有执行完整 CTest。

## 后续候选

生产代码当前最大的单文件为 1,108 行的 `native/src/formula_context.cpp`。下一轮可继续把其
剩余的组合与加载职责收敛为薄入口。
