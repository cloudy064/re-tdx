# 财报披露日历原生闭环

## 结论

通达信安装中的 `T0002/cloud_dax/CBPL.sp` 明确把四张公开 JSN 表定义为
“业绩预告”“业绩快报”“财报披露时间”和“最新财报披露（1 月内）”。本轮把
后三类及对应港股表接入纯 C++ 工具，新增：

```powershell
tdx-tool market disclosures --root C:\new_tdx --view schedule
tdx-tool market disclosures --root C:\new_tdx --view schedule `
  --market sz --code 000001
tdx-tool market disclosures --root C:\new_tdx --view all `
  --report-period 20260630 --status disclosed
```

网页服务同步暴露：

```text
GET /api/v1/market/disclosures
```

可按 `view=all|schedule|express|recent|announcement`、状态、市场、代码、报告期
和日期区间过滤；`backfill_announcements=1` 可为指定的深沪京证券查询最近一年
官方公告缓存。

## 原生数据语义

| 资源 | 通达信页面 | 核心字段 |
|---|---|---|
| `func_cbpl103_1.jsn` | A 股财报披露时间 | 报告期、首次预约日、三次变更、实际披露日 |
| `func_cbpl102_1.jsn` | A 股业绩快报 | 公告日、报告期、利润、ROE、EPS、净资产 |
| `func_cbpl104_1.jsn` | A 股最新财报披露 | 披露日、报告期及主要财务摘要 |
| `func_ggplsj101_1.jsn` | 港股财报披露时间 | 报告区间、预约/变更/实际披露日 |
| `func_ggyjpl101_1.jsn` | 港股最新财报披露 | 披露日、报告区间及主要财务摘要 |

`available_from` 只在服务器给出实际披露日或公告日时才有值。预约日即使已经
确定，也不会被当作财务数据可用日期；变更过预约日但尚未实披露时，状态为
`rescheduled`，`available_from` 仍为 `null`。

## 真实验收

2026-08-06 在线数据合并得到 6,609 条：

- 披露日程 5,807 条；
- 业绩快报 52 条；
- 近一月已披露财报 750 条；
- 带实际公告/披露日期的记录 812 条。

平安银行 `sz:000001` 返回报告期 `20260630`、预约日 `20260815`、实际披露日
为空、状态 `scheduled`，因此当前不会误用尚未披露的半年报。产物保存在：

- `output/probes/disclosures-000001-current.json`
- `output/disclosures-current-all.json`
- `output/finance-disclosure-probe/`

全部 26 组 C++ 测试通过。

## 历史边界

财报披露时间主表只保存当前报告期，最新披露表是滚动一月窗口；它们足以准确
判断当前报告季及从现在开始持续归档的可用时点，但不能一次性回填多年历史实披露
日期。因此本功能没有把报告期日期、预约日期或法定截止日冒充历史可用日期，
公式解释器的多年财务回测限制仍然保留。后续已经找到静态 F10 的单票公告缓存，
可额外回补最近一年、最多 300 条公告中的正式报告日；更久历史仍需从当前开始
保存每日快照，或继续寻找不受该窗口限制的官方资源。
