# 原生 C++ TBigData 计算列解释器模块化

日期：2026-08-10

## 目标

将同时承担 CFG 解析、表达式解释、36 项内建函数、宿主字段和请求编排的
`cloud_calc.cpp` 按稳定职责拆分，保持纯 C++、公开 schema 和 TBigData 语义不变。

## 拆分结果

`cloud_calc.cpp` 从 3,103 行降至 1,175 行，现主要保留审计报告、最小输入模板、
共享行情计划、单行/批量执行和 CLI。新增：

- `cloud_calc_builtins.cpp`（647 行）：36 项 `Builtin` 注册表、日期处理、
  应计利息、PV/YTM 数学与 dispatcher；
- `cloud_calc_config.cpp`（192 行）：`cloud_cfg` XML 容错解析、属性解码和 CFG 发现；
- `cloud_calc_interpreter.cpp`（348 行）：表达式解析、标识符检查、列依赖拓扑、
  环检测和按 unit 求值；
- `cloud_calc_host.cpp`（824 行）：L1、涨速、五档、财务、市值、行业、封单、
  组合证券与债券同行字段；
- 四个小型 `*_internal.hpp`：共享强类型 `Config/Unit/Column`、`Builtin` 和模块入口，
  公共 `tdx/cloud_calc.hpp` 未变化。

固定内建函数不再由主文件持有数组或 switch。日期校验、日期序号和 JSON 数字适配
通过内部工具入口供内建函数与宿主债券应计利息共同使用，没有复制两套算法。

## 行为保持

- 36 个内建函数的 ID、参数数、处理地址和实现状态未改变；
- 表达式支持范围、`calcflag=1` 缺失值回退、依赖环报告和 unit 隔离未改变；
- CFG 的 GBK/XML 容错、注释跳过、`calcref/syscol/refzqdm/refunit` 语义未改变；
- `0x054C/0x0547/0x053E/0x0010` 请求选择、封单补充、行业和组合证券聚合未改变；
- 单行/批量响应继续不回显原始请求行，公共 API schema 未修改。

## 增量验证

- `tdx-cloud-calc-tests` 通过；其中包含 31 个日期/债券内建函数的固定向量、
  PV/YTM 回算、表达式、CFG、宿主字段、封单和批量边界；
- `tdx-tool` 增量编译、链接通过；
- 真实 `func_kzz_kzzsy101`、`gxjty_zq_kzzsy101_1.jsn` 首行在公开行情模式下
  完成 15/15 个计算列，0 不可用、0 错误；
- 宿主字段为 9 个绑定、0 未解析，快照来源仍为 `public-l1-0x054c`；
- 内建拆分证据：`output/tdx-tbigdata-cloud-calc-builtins-modularization-v38.json`；
- 解释器拆分证据：`output/tdx-tbigdata-cloud-calc-interpreter-modularization-v39.json`；
- 宿主拆分证据：`output/tdx-tbigdata-cloud-calc-host-modularization-v40.json`；
- v38→v39、v39→v40 的 15 个计算值分别逐项相等，变化数均为 0；
- 未运行完整 CTest 或 API 契约，因为公共 schema、共享传输、缓存和服务路由未修改。

## 后续边界

`cloud_calc.cpp` 已处于约 1,200 行的组合器规模。后续如果扩展请求能力，应把共享
行情/财务抓取计划独立成 `CloudCalcFetchPlan`，而不是把新数据源分支重新写入 CLI；
宿主字段新增应进入 `cloud_calc_host.cpp` 的分类/策略表。
