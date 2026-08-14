# 通达信原生公告回补财报实际披露日

日期：2026-08-06

## 结论

通达信静态 F10 的公司公告页通过 `CWSearch.tzx_rcache` 查询
`gg:<市场号>_<证券代码>`。响应包含公告日期、标题、交易所分类、PDF 和来源，
页面脚本同时明确其边界为“最近一年、最多 300 条公告”。这比当前报告季日历和
近一月财报表覆盖更长，但仍不能冒充任意多年历史。

本轮已把该入口接入纯 C++ `market disclosures`：

```powershell
tdx-tool market disclosures --root C:\new_tdx `
  --market sz --code 300503 --backfill-announcements --archive
```

HTTP 只读回补为：

```text
GET /api/v1/market/disclosures?view=announcement&market=sz&code=300503&backfill_announcements=1
```

增量写入归档已与 GET 分离，必须显式确认 POST，且正文不接受服务器路径：

```http
POST /api/v1/market/disclosures/archive
X-TDX-Action: disclosure-archive
Content-Type: application/json

{"market":"sz","code":"300503","backfill_announcements":true}
```

`view=announcement` 可只查看回补结果，`view=all` 可与现有日历、快报和近期财报
合并查看。回补必须指定单只深沪京六位代码，不会默认对全市场连续发起数千次请求。

在此基础上，CLI 已增加受数量上限和限速保护的自选股、板块及证券清单批处理，
并支持失败重试、断点续传和原子归档合并；详见
[批量回补验收记录](2026-08-06-disclosure-announcement-batch-backfill.md)。HTTP 只读入口仍保留为
短时单票查询；写归档只走上述确认式 POST，长任务不会占用服务请求。

## 识别规则

三家交易所的公告分类号并不相同：

| 报告 | 深市 | 沪市 | 北交所 | 报告期后缀 |
|---|---:|---:|---:|---:|
| 年报 | `010301` | `101` | `0101` | `1231` |
| 一季报 | `010305` | `102` | `0105` | `0331` |
| 半年报 | `010303` | `103` | `0103` | `0630` |
| 三季报 | `010307` | `104` | `0106` | `0930` |

实现同时要求标题中存在相应的正式报告名称和四位报告年度；“摘要”只作为审计
证据保留，不能解锁专业财务包。审计报告、业绩说明会、披露提示等即使标题含有
“年度报告”也会因分类号不匹配而排除。若同一报告期出现修订版，归档保留全部
公告证据，并以最早确认的完整报告公告日作为 `report_available_from`。

## 真实验收

在线样本均识别出四个正式报告期：

- `SZ300503`：`20250930 / 20251231 / 20260331 / 20260630`；
- `SH600000`：`20250630 / 20250930 / 20251231 / 20260331`；
- `BJ920000`：`20250630 / 20250930 / 20251231 / 20260331`。

昊志机电归档由 5,552 个“证券×报告期”条目增至 5,555，正式报告可用条目由
131 增至 134。严格公式加载了四个公告事件和四个官方专业财务包，真实日线切换为：

```text
首个生效交易日  报告期
2025-10-31      20250930
2026-04-22      20260331
2026-07-22      20260630
```

年报和一季报同在 `2026-04-21` 公告，因此下一交易日直接选择更晚的
`20260331`，不会短暂回退到年报。公告没有时分，仍严格在公告日后的第一根 K 线
生效。

验收产物：

- `output/probes/disclosures-announcement-300503.json`
- `output/probes/disclosures-announcement-600000.json`
- `output/probes/disclosures-announcement-920000.json`
- `output/probes/disclosures-announcement-300503-archive.json`
- `output/probes/formula-300503-point-in-time-finance-backfilled.json`

## 边界

这个闭环实现了“披露时间”的时点正确性，但历史 `gpcw` 数值来自当前仍在分发的
官方报告期包，可能已经包含后续更正或追溯重述。因此它不是完整的双时态财务库；
归档明确记录这一边界，不声称恢复了每个公告版本当时的原始数值。

全套原生测试为 `26/26`。服务已更新为
`dist/tdx-tool/bin/tdx-tool.exe`，SHA-256 为
`CC7DEC269F9258ADE6CA75223B41B10DD4093E75583DD942946CA27F0B1F7A13`，进程
`23220`；健康检查和 HTTP 单票回补均通过。
