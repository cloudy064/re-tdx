# 原生 C++ 公式上下文横向聚合与市场汇总拆分

日期：2026-08-10

## 拆分结果

`formula_context.cpp` 从 3,964 行降至 2,903 行，继续只负责上下文的组合顺序。
新增两个按数据来源闭合的领域模块：

- `formula_context_aggregate.cpp`（820 行）：`HORCALC` 横向行情计算以及
  `INSORT/INSUM` 跨证券指标求值、日期对齐、成员覆盖和缓存统计；
- `formula_context_market_summary.cpp`（351 行）：`BETAVALUE/SHAPE_*` 个股
  统计，以及 `MAINZSHQ/TOTALHQINFO/TOTALMMPAMO` 公共市场汇总；
- `formula_context_aggregate_internal.hpp` 用 `HorcalcBinding` 与
  `IndicatorAggregateBinding` 两个值对象传递解析结果；
- `formula_context_market_summary_internal.hpp` 只暴露两个领域绑定入口；
- `TDX_FORMULA_SOURCES` 显式登记两个新翻译单元。

拆分按功能拥有的数据加载、缓存和上下文写入边界完成，没有把同一算法拆成跨文件
的半段实现，也没有为没有运行时多态的流程引入继承层次。

## 行为保持

- `HORCALC` 仍只接受日线，保留板块解析、成员行情加载、五种权重与精确覆盖元数据；
- `INSORT/INSUM` 仍按成员证券独立执行嵌套指标，并保留日期缺失、排行方向、
  极值成员序号和求值缓存语义；
- 个股统计缓存、形态字段拆位、主要指数选择、公开 L1 汇总和 Level2 显式边界未变；
- 公式名称、上下文键、异常文本、CLI 与 HTTP schema 均未调整。

## 增量验证

- `tdx-formula-engine-tests` 通过；其中现有 fixture 直接覆盖 `HORCALC` 与
  `INSORT/INSUM` 的上下文构建、数值结果、缺失日期和缓存元数据；
- `tdx-tool` 增量链接通过；
- 公开 `sz:000001` 的 20 根日线执行
  `BETAVALUE/SHAPE_SHORT/MAINZSHQ/TOTALHQINFO` 成功，最后一根分别为
  `-0.2025/5/3537.21/103`；
- 输入：`output/probes/formula-context-market-summary.tdx`；
- 证据：`output/native-formula-context-aggregate-v29.json`；
- 未运行完整 CTest、API 合约或前端检查，因为本次没有改动共享传输、缓存、
  公共 API schema 或网页代码。

## 后续边界

`formula_context.cpp` 已降到 3,000 行以内。下一轮应审计其剩余的行情基础字段、
证券关系与行业指数绑定，把可独立加载并一次写入上下文的服务继续迁出；随后处理
`formula_engine.cpp` 的语义分析、执行器和绘图 IR 边界。
