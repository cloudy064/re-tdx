# 原生 C++ 公式专业数据上下文拆分

日期：2026-08-10

## 拆分结果

`formula_context.cpp` 从 2,903 行降至 2,645 行。新增：

- `formula_context_professional_internal.hpp`：集中定义 `TradingBinding`、
  `OnePointBinding`、`ProfessionalBoardTarget` 三个值对象和两个领域入口；
- `formula_context_professional.cpp`（330 行）：负责专业交易序列、专业财务占位、
  FINONE、GPJYONE、BKJYONE、SCJYONE 与 GPONEDAT 单点绑定。

`BKJYONE` 的行业目标解析仍由组合层负责，因为它复用本地多级行业层级和指数映射；
解析结果通过 `ProfessionalBoardTarget` 注入专业数据模块。模块本身只负责数据下载、
日期对齐、单点选择、稀疏序列和元数据，不反向依赖行业解析实现。

## 行为保持

- GPJYVALUE/SCJYVALUE 的日期对齐、字段和类型 selector 未变；
- BKJYVALUE 对普通股票仍返回空序列，对指数仍读取对应专业交易记录；
- FINVALUE 缺失证券或指数的显式空序列语义未变；
- FINONE 的季度、年份、MMDD 和相对期数选择与缓存目录未变；
- GPONEDAT 仍读取本地 10 字节记录，并在文件或记录缺失时返回宿主初始化值 0；
- 公开公式名、上下文键、异常消息和 API schema 未修改。

## 增量验证

- `tdx-formula-engine-tests` 通过；
- `tdx-professional-data-tests` 通过；
- `tdx-tool` 增量链接通过；
- 公开 `sz:000001` 20 根日线执行 `GPJYVALUE(3,1,1) + GPONEDAT(7)` 成功；
- 两个自动上下文键均已绑定，GPJYVALUE 得到 20 个数值点、末值
  `480204.8125`，GPONEDAT 缺失回退为 0；
- 输入：`output/probes/formula-professional-context.tdx`；
- 证据：`output/native-formula-professional-context-v34.json`；
- 未运行完整 CTest 或 API 契约，因为公共 schema、共享传输和缓存协议未变化。

## 后续边界

`formula_context.cpp` 剩余主要职责为证券关系/行业指数、当前财务与 DYNAINFO、
状态和本地板块文本。下一轮优先把证券关系与行业上下文合并成独立服务，避免继续
暴露十余个零散辅助函数。
