# 原生 C++ 公式运行环境 Builder 拆分

日期：2026-08-10

## 拆分结果

`formula_engine.cpp` 从 946 行继续降至 512 行。新增：

- `formula_environment_internal.hpp`：定义 `FormulaEnvironment` 构建结果和
  `FormulaEnvironmentBuilder`；
- `formula_environment.cpp`（474 行）：负责从 K 线文档、解析后的 `Program`、
  用户参数和自动/显式上下文构建完整运行环境。

Builder 的输出包含数值环境、字符串环境、规范化实际参数、上下文绑定名称以及
RAND 种子元数据。执行器只消费该值对象，随后逐语句求值、交给渲染器并封装结果。
构建器通过引用组合输入，没有继承、全局可变状态或对外公共 API。

## 迁出的职责

- OHLCV、扩展市场持仓量、港股沽空量、分时均价和结算价符号；
- 复权标志、方向 K 线、证券类别、ST、筹码与成交量倍率；
- 财务、DYNAINFO、标量、稀疏序列、证券名称和外部信号上下文；
- IVOLAT 历史、日期时间、柱状态、周期、市场、TR/MTM 与机器时间；
- 参数广播、随机种子校验和上下文绑定清单。

## 验证

- `tdx-formula-engine-tests` 通过；
- `tdx-tool` 增量链接通过；
- 公开 `sz:000001` 40 根日线执行
  `BETAVALUE + MA + COLORRED/LINETHICK2 + DRAWICON` 成功；
- 自动上下文包含 `BETAVALUE`，末值为 `-0.2025`；MA 末值约 `11.288`；
- 生成 3 个绘图图元、4 个图标事件，分析无不可用绑定；
- 输入：`output/probes/formula-environment-context-render.tdx`；
- 证据：`output/native-formula-environment-builder-v33.json`；
- 未运行完整 CTest、API 合约或前端检查，因为公共 schema、传输、缓存与网页未改动。

## 当前解释器结构

原始近 5,000 行的解释器现在由分析、语言、环境、运行时求值、绘图和 512 行组合根
组成。后续解释器工作应以语义补齐和数值对照为主，不再向组合根堆叠领域实现。
