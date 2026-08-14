# 自定义条件扫描与专家回测纯 C++ 闭环

## 目标

把已经能单票执行的自定义通达信公式源码继续贯通到条件选股和专家回测，保持
统一工具、HTTP 服务和网页全部为纯 C++ 执行，不转发 Python，不写入用户的
通达信公式目录。本轮不涉及 L2。

## 原生实现

新增共享的 `make_formula_source_definition`，把源码、类型、参数默认值和
`analyze_formula_source` 结果构造成与内置公式一致的定义。CLI、HTTP 扫描与
HTTP 回测由此共用同一语义分析和安全门，不再复制临时公式结构。

- `formulas scan --source-file PATH`：把源码视为 `selection`，支持显式证券、
  全市场、最近 N 根触发和缓存；
- `formulas backtest --source-file PATH`：把源码视为 `expert`，要求输出
  `ENTERLONG/EXITLONG`，保留佣金、滑点、最大回撤和下一根开盘成交语义；
- `POST /api/v1/formulas/scan` + `X-TDX-Action: formula-scan`；
- `POST /api/v1/formulas/backtest` + `X-TDX-Action: formula-backtest`。

POST 正文与源码仅在请求内存中存在。响应标记
`formula_source_mode=inline-post`、`source_bytes` 和
`request_body_retained=false`。源码文件模式则标记
`formula_source_mode=source-file`。

## 安全边界

- HTTP body 64 KiB、源码 16 KiB、参数 64 个；
- 条件扫描最多接受 50 只证券；
- 扫描和回测都要求 `numeric_signal_safe=true`；
- 未来函数继续只允许单票只读绘图，不能进入扫描或回测；
- 历史财务回测继续要求严格披露时点，不能回退到当前报告常量；
- 专家回测信号在当前 K 线收盘确认，在下一根 K 线开盘成交。

## 网页

Svelte 公式工作台的自定义模式增加三种任务：单票计算、条件扫描、专家回测。
切换任务时提供对应安全示例，用户编辑过的源码不会被任务切换意外覆盖。
Ctrl+Enter、证券代码、公式代码、历史页数和参数输入框的回车均按当前任务执行。
扫描结果显示命中证券与触发信号；回测结果显示收益、回撤、交易数、胜率和成交
时序。

## 验证

真实公开行情验证：

- 自定义条件 `RESULT:CLOSE>MA(CLOSE,N)` 扫描平安银行和浦发银行：请求 2、
  完成 2、命中 2、公式错误 0、取数错误 0；
- 自定义均线专家系统回测平安银行 240 根日线：21 笔交易，总收益
  `-2.5693198285%`；成交时序为 `signal at close, execute at next open`；
- CLI 的相同两份源码分别生成
  `output/probes/custom-selection-cli.json` 和
  `output/probes/custom-expert-cli.json`；
- C++ 完整测试 65/65；
- Svelte 0 错误、0 警告，生产构建成功；
- 新增两项部署态契约，完整巡检 95/95：
  `output/probes/api-contract-full-20260807-custom-formulas.json`。

部署服务为 `http://127.0.0.1:8765`，PID `4396`；发行包 SHA-256 为
`D0191E5FAAD6CBDC1F7A224A374801E72529F163847A901671FEE10E5ADA3C48`。

