# 严格财务时点接入选股与回测

日期：2026-08-06

## 结论

此前 `--point-in-time-finance` 只用于单公式解释。本轮已把同一组公告归档、报告期
专业财务包和“公告日后下一根 K 线生效”规则接入：

- `formulas scan` CLI；
- `/api/v1/formulas/scan`；
- `formulas backtest` CLI；
- `/api/v1/formulas/backtest`；
- Svelte 公式库的计算、扫描、回测共用“严格财务时点”开关。

示例：

```powershell
tdx-tool formulas scan --root C:\new_tdx --formula A006 `
  --securities sz300503,sz000001 --period day `
  --point-in-time-finance

tdx-tool formulas backtest --root C:\new_tdx `
  --library output\probes\point-in-time-backtest-library.json `
  --formula PITEXPERT --market sz --code 300503 --period day `
  --point-in-time-finance
```

HTTP 使用同名参数 `point_in_time_finance=1`。

## 安全语义

扫描器为每只证券独立构造财务时间序列，历史缺口会作为取数错误返回，不会使用
当前报告兜底。命中项携带 `context_metadata`，可审计归档事件数、成功加载报告数
和严格模式名称。

回测的安全门更严格：

- 财务公式未显式开启严格时点时直接拒绝；
- 严格模式仅接受可由历史专业包重建的 `FINANCE(43/44)` 与
  `FINVALUE(0..584)`；
- `FINANCE(30/41/56)` 等当前快照字段仍明确拒绝；
- 混入尚未证明历史安全的其他外部依赖也拒绝，不能把当前快照常量铺到整段历史；
- 纯 OHLCV 专家系统保持原有行为。

回测核心现在接受显式公式上下文，并把 `context_bindings` 与 `context_metadata`
原样返回，调用方能验证实际使用的时点模式。

## 真实验收

先用官方公告缓存回补 `SZ000001`，其归档新增：

```text
20250630  20250823
20250930  20251025
20251231  20260321
20260331  20260425
```

`PITSCAN: FINVALUE(0)>0` 对 `SZ300503,SZ000001` 扫描结果为：

```text
请求 2，完成 2，命中 2，取数/执行错误 0
每只证券 archive events=4，loaded reports=4
mode=archived-actual-full-report-next-bar-no-current-fallback
```

验收专家公式：

```text
ENTERLONG:FINVALUE(0)=20250930;
EXITLONG:FINVALUE(0)=20260630;
```

在昊志机电真实日线上得到一笔交易：

```text
进入 2025-11-03 开盘
退出 2026-07-23 开盘
零佣金/零滑点收益 125.8643%
公告事件 4，加载报告 4
```

去掉 `--point-in-time-finance` 后同一命令退出码为 2，并报告当前报告常量不具备
回测安全性。测试同时覆盖核心回测消费时点序列、扫描命中保留审计元数据；全套
C++ 测试 `26/26`，`svelte-check` 无错误，Vite 生产构建通过。

验收产物：

- `output/probes/point-in-time-backtest-library.json`
- `output/probes/formula-backtest-300503-point-in-time-finance.json`
- `output/probes/formula-scan-a006-point-in-time-finance.json`
- `output/probes/formula-scan-pitscan-point-in-time-finance.json`
- `output/probes/disclosures-announcement-000001-archive.json`

服务已部署为 `dist/tdx-tool/bin/tdx-tool.exe`，SHA-256：
`AD566CDAF02CDFED3B7A30BC451D2113C3205D63C2720F9EA3FDB320180D1EFC`，进程
`35740`。HTTP 严格扫描完成 2/2、0 错误；普通 MA 专家回测仍可运行，而对不含
财务依赖的 MA 强制传入 `point_in_time_finance=1` 会返回 HTTP 400，证明服务端
安全门生效。生产页面已包含“严格财务时点”开关。
