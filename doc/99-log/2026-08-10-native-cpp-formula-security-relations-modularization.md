# 原生 C++ 公式证券关系与行业上下文拆分

日期：2026-08-10

## 拆分结果

`formula_context.cpp` 从 2,645 行降至 1,567 行。新增：

- `formula_context_relations.cpp`（1,137 行）：主指数身份、标的证券、期权 IVOLAT、
  Beta/市场指数、跨证券字段、行业估值、行业指数序列与行业文字；
- `formula_context_relations_internal.hpp`：提供静态关系、时序关系两个组合入口，
  `RelationBlockRequirements` 统一声明所需板块族；
- `formula_context_support.cpp`（75 行）：统一市场编号、扩展市场证券缓存和公式文字
  符号写入；
- `formula_context_support_internal.hpp`：仅供公式上下文翻译单元共享；
- `BKJYONE` 的行业/宽基指数目标解析迁入关系服务，并返回
  `ProfessionalBoardTarget` 给专业数据模块。

主组合器不再直接调用 IVOLAT、Beta、指数、标的证券和行业序列的十余个辅助函数，
只在静态上下文阶段和 K 线时序阶段各调用一个关系服务入口。

## 行为保持

- `DPZSCODE/DPZSNAME` 的市场与板块映射未变；
- `UNDERCODE/UNDERLYC` 仍只对真实期权或可转债标的建立关系，并按日期时间对齐；
- IVOLAT、Beta、INDEX* 与外部证券字段的缓存键和 float32 语义未变；
- HYSYL/HYSJL 的行业选择、公开 HYZT 边界和缺失值处理未变；
- HYBLOCK/HYZSCODE/MOREHYBLOCK/LEVEL1HYBLOCK 与行业指数 OHLCV 的层级选择未变；
- 板块加载由 `RelationBlockRequirements.families` 集中配置，概念、风格、普通行业、
  研究行业和指数族不会再由主文件重复判断。

## 增量验证

- `tdx-formula-engine-tests` 通过；
- `tdx-tool` 增量链接通过；
- 公开 `sh:110075` 120 根日线执行主指数、标的证券、UNDERLYC 和 DIVFACTOR
  关系公式成功；主指数为 `999999`，标的为 `600029`；
- 与拆分前证据比较，前 119 根的 952 个公共输出值全部相等；最后一根只有
  `U/U2` 两项从 `5.13` 变为 `5.15`，对应重新请求时标的公开即时价变化，其他
  6 项相等；
- 输入：`output/probes/formula-security-relations.tdx`；
- 证据：`output/native-formula-security-relations-v35.json`；
- 未运行完整 CTest 或 API 合约，因为公共 schema、共享传输与缓存协议未修改。

## 后续边界

`formula_context.cpp` 已降到约 1,500 行。剩余大块主要是当前财务/DYNAINFO 与
证券状态、本地板块/单点数据的编排；下一轮可抽取动态行情构建器，之后该文件即可
稳定成为纯组合根。
