# JSN 扩展健康契约与公式全量审计

## 目标

继续收口非 Level2 的高价值通达信功能：把第二批六类个股 JSN 服务纳入统一的
来源健康与陈旧缓存语义，同时修复公式运行审计会静默省略不可执行记录的问题。

## 第二批 JSN 服务

以下纯 C++ 服务已统一使用共享来源元数据和 `cache.upstream` 聚合健康：

- 机构龙虎 `market institution-lhb`；
- 港股/行业评级 `market ratings`；
- 外资预警 `market foreign-alerts`；
- 限售解禁 `market unlocks`；
- 大宗交易 `market block-trades`；
- 龙虎榜事件 `market lhb`。

响应统一区分 `live`、`partial` 和 `stale-cache`。动态详情的真实零长度资源不做
重试或陈旧回退；龙虎榜批量详情若只缺少部分事件，会拆成单事件请求，保留成功
事件并在 `detail_errors` 中报告缺失键。传输故障仍遵循三次新连接重试、完整批次
缓存和冷缓存 HTTP 503 规则。

连同机构调研、一致预期、行业画像、股权变动、股份回购和主动基金，目前共有
十二类高频 JSN 服务采用相同健康语义。`recon api-contracts --profile full` 的真实
个股健康样本由五类扩展为十一类；完整巡检由 18 项扩展为 24 项。

## 公式审计 v2

旧审计的汇总以 379 条公式库为分母，但只把进入求值路径的 355 条写入
`formulas`，依赖不可用和市场不适用记录可能被静默跳过。新结构保证每条库记录
都输出身份、状态和缺失条件，并增加 `reported/unreported` 不变量：

- `passed`：实际求值通过；
- `dependency_unavailable` / `context_unavailable`：依赖或上下文不可用；
- `future_read_only_disabled`：只读未来模式未显式开启；
- `market_inapplicable` / `period_inapplicable`：市场字段或周期不适用；
- `error`：解释器执行错误。

使用平安银行 `SZ000001` 日线、800 根 K 线、完整市场上下文和显式未来只读模式
实测：379/379 条均有报告；354 条通过，20 条依赖不可用，4 条市场不适用，
1 条周期不适用，解释器错误为 0。354 条通过项均产生数值，352 条在最新一根有
数值；两条云端涨跌停广度公式受当前上游时间对齐影响，最新点为空。

证据文件为 `output/probes/formula-runtime-audit-current.json`。

## 验证与部署

- 原生 CTest：42/42；
- 隔离服务扩展契约：24/24，证据
  `output/probes/api-contracts-jsn-expanded-temp.json`；
- 正式 `127.0.0.1:8765` 契约：24/24、24 次网络请求，证据
  `output/probes/api-contracts-official-current.json`；
- 正式服务 PID：`3172`；
- 部署 EXE SHA-256：
  `459C2EAEAC32ADE3E3815B332FA5210A56A92E2A7395659F44B601104934F471`；
- 健康检查：`ok=true`、`native_cpp=true`、`python_runtime=false`，功能目录 89 项。

本轮未修改用户的 Svelte UI。发布 HTML、JS、CSS SHA-256 仍分别为
`0F1FBB7E19DCFAACE0A9C10A6741AFA1244A53E7977FF8B958D0D454573D5777`、
`B03C287DF8EF4C36EBC0E03456ACD934439D820E7BC606540FD80E8F66384411`、
`733085A8400D3DBF634DA5CD2ADAADD04F312BF75397F9102E303FE42665830A`。

## 后续高收益方向

1. 为 73 个 TQLEX 模板/34 个唯一 ReqId、58 个 PBRPC 模板/29 个唯一 ReqId
   建立参数、Entry、模块和占位符变体矩阵，定位相同请求号下尚未闭合的功能分支；
2. 在不使用 Level2 权限的边界内，继续还原 L1 行情订阅生命周期，并评估由服务端
   通过 SSE 推送快照、五档和分时增量，减少网页轮询；
3. 将公式审计的缺失依赖按数据来源形成可行动清单，明确哪些只能等待合法 L2/
   券商私有上下文，哪些能由公开期货、港股或期权数据继续闭合。
