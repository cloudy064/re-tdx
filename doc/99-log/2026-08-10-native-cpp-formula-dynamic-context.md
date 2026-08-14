# 原生 C++ 公式动态行情上下文拆分

日期：2026-08-10

## 拆分结果

`formula_context.cpp` 从 1,567 行降至 1,149 行。新增：

- `formula_context_dynamic.cpp`（590 行）：集中实现 `DYNAINFO`、实时 L1、
  涨速、五档、量比、5 分钟前价格、动态市盈率、涨跌停、价格笼子与封板状态；
- `formula_context_dynamic_internal.hpp`（30 行）：声明
  `DynamicQuoteRequirements` 和唯一的动态行情组合入口；
- `DynamicQuoteContextBuilder`：按“涨速、行情、基础派生值、历史派生值、
  五档、价格边界、快捷别名”的阶段装配上下文；
- `kDynamicAliases` 常量表：统一配置 `DYNA_NOW/DYNA_ZAF/DYNA_LB/DYNA_ZAS`
  到 selector、TCalc opcode 和源命令的映射。

主组合器现在只负责从分析结果取得强类型需求、按需准备财务记录，然后调用一次
`bind_dynamic_quote_context`。它不再理解行情命令选择、五档字段、板块涨跌停比例或
价格笼子算法。

## 行为保持

- 普通实时字段仍走 `0x054C`，需要五档的 selector 仍走 `0x0547`；
- `DYNAINFO(24)` 与 `DYNA_ZAS` 仍使用 `0x053E`，并保持百分数转小数的
  float32 语义；
- `DYNAINFO(39)` 仍使用当前价/昨收回退以及年化每股净利润；
- `DYNAINFO(26/27)` 仍优先使用 `0x0452` 特殊涨跌停表，缺失时按证券类型回退；
- `DYNAINFO(28/29)` 与 `DYNAINFO(88)` 的价格笼子、封板判定和元数据 schema
  未改变；
- `BUYVOL/SELLVOL` 和四个快捷别名仍写入公式符号表；`dynainfo` 仍始终存在，
  无请求时为空对象。

## 增量验证

- `tdx-formula-engine-tests` 通过；
- `tdx-tool` 增量编译、链接通过；
- 公开 `sz:000001` 20 根日线执行 17 个动态行情输出成功；
- 五档命令为 `0x0547`、涨速命令为 `0x053E`；
- 四个快捷别名与对应 selector 的最后一根结果逐项相等；
- 动态市盈率、量比、上下限、价格笼子、封板状态与内外盘均成功产出；
- 输入：`output/probes/formula-dynamic-quote-context.tdx`；
- 证据：`output/native-formula-dynamic-quote-context-v36.json`；
- 未运行完整 CTest 或 API 合约，因为公共 schema、共享传输、缓存协议和服务路由
  均未修改。

## 后续边界

公式环境、运行时、渲染、语义分析、证券关系、专业数据与动态行情均已独立。
`formula_context.cpp` 剩余约 1,150 行主要是财务、证券状态、外部信号和各上下文模块的
组合顺序；下一轮应优先把当前财务装配迁为独立 Builder，使该文件最终稳定为组合根。
