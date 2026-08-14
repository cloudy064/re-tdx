# 原生服务公式控制器拆分与 TCalc 注册边界闭环

日期：2026-08-10

## 结果

- `server.cpp` 从约 6,100 行降至 4,657 行，仅保留套接字生命周期、路由编排和
  尚未独立的市场控制器；
- `server_catalog.cpp` 独立维护功能目录和 OpenAPI 注册；
- `server_formula.cpp` 独立维护公式、云计算、TPool、扫描、策略与回测控制器；
- 公式控制器通过窄化的 `FormulaHttpState` 取得必要依赖，不再耦合完整
  `ApiState`；
- TCalc 390 条静态注册名完成数据化归类，避免把运行时专属入口继续误报成公开
  数据可实现的解释器缺口。

## 注册边界

| 分类 | 数量 | 执行策略 |
|---|---:|---|
| 已由解释器或自动上下文识别 | 317 | 按现有精确语义执行 |
| 语法/编译记号 | 4 | 由解析器处理，不注册为数值函数 |
| 券商私有信号 | 3 | 仅接受调用方精确序列，不提供公共解析源 |
| L2 订单流 | 13 | 仅接受有权限的数据，不从 L1 推断 |
| 实时交易账户、持仓与下单状态 | 38 | 只读工具不暴露交易宿主 |
| 外部 DLL/用户插件回调 | 15 | 当前不加载第三方回调 |

五类边界合计 73 条，集合并集由程序计算并发布；能力文档返回
`static_registry_fully_classified=true`、
`remaining_public_non_l2_candidate_count=0`。完整证据位于
`output/native-formula-coverage-tcalc-boundary-v22.json`。

## 增量验证

- `tdx-formula-engine-tests`：通过；
- `tdx-recon-contract-tests`：通过；
- 候选服务自检：通过；
- 候选端口 8875 抽查 4 项：健康、144 条 OpenAPI 路径、公式覆盖边界、
  `CALCSTOCKINDEX` 内联执行，全部通过；
- 未运行全量 CTest 或全 API；候选进程已停止，8765 正式服务未替换。

当前候选 `tdx-tool.exe` SHA-256 为
`4B1DF7D310839CFBEEB22C16A683EF9107D6010B37D30B4670B161DDC2792BDD`。
