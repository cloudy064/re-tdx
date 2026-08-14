# 原生 C++ 公式上下文薄组合根

日期：2026-08-11

## 目标

将 `formula_context.cpp` 从“解析分析结果、加载资源、执行宿主重建、绑定本地数据、补充财务、
最后组合结果”的单体流程，收敛为可读的上下文阶段编排器，并保持公式解释器对外行为不变。

## 结构调整

原文件为 1,108 行，现根文件为 239 行。新增四个实现阶段：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `formula_context_host.cpp` | 397 | MULTIPLIER、宿主日历、外部信号/序列和证券状态 |
| `formula_context_plan.cpp` | 299 | 从分析结果构建 17 类绑定集合、依赖标志和板块加载计划 |
| `formula_context_local.cpp` | 227 | BLOCKSETNUM、主营业务、评分、自定义/组合/相似板块 |
| `formula_context_finance_extensions.cpp` | 108 | 融资资格、事件日、增长和专业财务扩展 |

`FormulaContextPlan` 是一次构建、只读消费的配置对象。根函数现在明确执行：计划与校验、共享
板块加载、静态关系、宿主数据、当前财务、本地元数据、聚合/拆分、动态行情、财务扩展、
关系时序与专业数据。共享 `BlockData`、公式库和当前财务记录的生命周期仍由根函数持有。

## 兼容性

- `build_formula_market_context_document` 的公开签名未改变；
- 原有错误条件、板块族加载、公式库按需加载和阶段调用顺序保持不变；
- MULTIPLIER、LOCALDAYNUM、EXTERNVALUE/EXTERNSTR、EXTDATA_USER、IST0CODE、
  MAINBUSINESS、SAFESCORE、BLOCKSETNUM、FINANCE/FINVALUE 等绑定保持原 schema；
- 点时财务、动态行情、证券关系与专业数据仍共享原来的上下文对象。

## 增量验证

- `tdx-formula-engine-tests` 与 `tdx-tool` 使用五个新实现单元重新编译、链接通过；
- `tdx-formula-engine-tests.exe` 执行通过；
- `sh:110075` 代表公式返回 120 根日线、8 个输出；
- 主指数仍为 `999999`，标的仍为 `600029`，最后一根 UNDERLYC 为
  `5.150000095367432`；
- 样例位于 `output/formula-context-root-refactor-sample.json`；
- 没有执行完整 CTest。

## 后续候选

生产代码当前最大的单文件为 1,083 行的 `native/src/leverage.cpp`。公式上下文根已达到薄编排
状态，后续新增宿主数据应进入相应阶段模块，而不是重新堆回根函数。
