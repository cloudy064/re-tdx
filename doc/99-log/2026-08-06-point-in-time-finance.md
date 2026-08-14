# 财务公式按真实披露时点生效

日期：2026-08-06

## 问题

官方 `gpcwYYYYMMDD.zip` 可以给出各报告期的 `FINVALUE(0..584)`，但包内没有
单只股票的公告日。过去的公式上下文只能把“当前最新报告”作为常量放到整段
K 线，适合看当前值，不适合无前视偏差的历史观察。

通达信公开披露资源提供预约、变更、实际披露、业绩快报和近一月已披露摘要，
但历史窗口有限。因此不能把报告期末日当作公告日，也不能只靠一次下载重建
任意多年历史。

## 已实现

- `market disclosures --archive` 以纯 C++ 把每次公开观察增量合并到
  `T0002/tdx-tool/disclosure-availability.json`；
- 归档键为 `security_id + report_period`，重复运行幂等，实际日期改变时保留
  `revisions`；
- `report_available_from` 只接受正式报告的实际披露日；
- `express_available_from` 单独保存，业绩快报不会错误解锁完整专业财务包；
- `formulas evaluate --point-in-time-finance` 读取归档，按报告期下载并校验官方
  专业财务包，生成逐 K 线的 `FINANCE(43/44)` 和任意 `FINVALUE(id)` 序列；
- HTTP 公式接口接受 `point_in_time_finance=1`；披露查询保持 GET 只读，归档写入
  只接受 `/api/v1/market/disclosures/archive` 加
  `X-TDX-Action: disclosure-archive` 的显式 POST；
- 由于资源没有公告时分，严格口径为实际披露日后的第一根 K 线才生效；披露
  当天仍为空，最早已知时点之前也不回退到当前值；
- 严格模式暂只允许可由历史专业财务包精确重建的 `FINANCE(43/44)`；其他
  `FINANCE(id)` 会明确拒绝，避免混入当前快照。

首次归档结果：

```text
观测资源行          6609（含港股）
A 股/B 股归档观测行 5617
证券×报告期条目     5552
正式报告已披露条目   131
业绩快报已公告条目    52
日期修订               0
```

## 真实验收

`SZ300503 昊志机电` 的 `20260630` 报告在披露资源中的实际日期为
`20260721`。使用 800 根真实日 K 线执行：

```text
日期        FINVALUE(0)  FINVALUE(183)  FINVALUE(184)
2026-07-20  null         null           null
2026-07-21  null         null           null
2026-07-22  20260630     65.8600006     266.5700073
2026-07-23  20260630     65.8600006     266.5700073
```

`FINANCE(43)` 与字段 184、`FINANCE(44)` 与字段 183 同步一致。单元测试还覆盖
待披露与快报隔离、重复合并、日期修订、同日不可用、下一日切换及多个报告期
选择；全套原生测试为 `26/26`。

验收产物：

- `output/probes/disclosures-archive-bootstrap.json`
- `output/probes/formula-300503-point-in-time-finance.json`
- `output/probes/formula-300750-point-in-time-finance.json`
- `output/probes/point-in-time-finance.tdx`

## 使用

```powershell
tdx-tool market disclosures --root C:\new_tdx --view all --archive

tdx-tool formulas evaluate `
  --root C:\new_tdx --market sz --code 300503 --period day `
  --source-file point-in-time-finance.tdx --formula PITFIN `
  --point-in-time-finance
```

首次全市场运行覆盖当前公开窗口；指定单票还可用静态 F10 公告回补最近一年、
最多 300 条公告中的正式报告：

```powershell
tdx-tool market disclosures --root C:\new_tdx --market sz --code 300503 `
  --backfill-announcements --archive
```

以后定期执行归档命令，历史边界会随时间自然积累；已经错过且公告窗口不再提供的
旧披露日仍保持未知，不做推测。回补改善披露时点，但当前官方 `gpcw` 包可能含
后续追溯修订，不能视为逐公告版本的双时态财务数据。
