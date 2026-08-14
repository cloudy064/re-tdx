# 财报披露覆盖率审计与可行动回补队列

日期：2026-08-06

## 结论

纯 C++ `market disclosures` 已增加离线覆盖率审计。它复用批量回补的单票、
自选股、JSON/CSV/文本和父子板块证券选择器，读取现有披露归档，输出每只证券、
每个报告期的状态，并把当前确实可回补的证券放进顶层 `items`。该 JSON 可直接
作为下一次 `--backfill-input`，无需转换脚本。

```powershell
# 默认审计归档中最新四个季度
tdx-tool market disclosures --root C:\new_tdx `
  --audit-coverage --backfill-watchlist

# 父级行业递归审计
tdx-tool market disclosures --root C:\new_tdx `
  --audit-coverage --backfill-block research-industry:X50

# 显式报告期和评价日期，便于重放历史状态
tdx-tool market disclosures --root C:\new_tdx --audit-coverage `
  --security sz300503 --audit-periods 20250930,20251231,20260331,20260630 `
  --audit-as-of 20260806

# 只预览审计生成的可行动队列
tdx-tool market disclosures --root C:\new_tdx `
  --backfill-announcements --backfill-input output\tdx-disclosure-coverage-audit.json `
  --dry-run
```

若不指定报告期，工具从归档中选择最新四个季度末；`--latest-periods` 可调整数量。
显式日期必须是 `0331/0630/0930/1231` 季末，其他日期会拒绝。

## 状态与行动性

审计不再把所有“尚无正式报告日”都视为同一种缺口：

| 状态 | 含义 | 进入回补队列 |
|---|---|---:|
| `covered` | 已有正式报告实际披露日 | 否 |
| `scheduled_future` | 预约日在评价日之后 | 否 |
| `express_only_scheduled_future` | 已有快报，正式报告预约日仍在未来 | 否 |
| `overdue_without_report` | 预约日已过但未观察到正式报告 | 是 |
| `express_only` | 只有快报且没有未来预约保护 | 是 |
| `observed_without_report` | 归档观察过该报告期，但没有正式报告日 | 是 |
| `missing_entry` | 归档完全没有“证券×报告期”条目 | 是 |
| `invalid_report_date` | 正式报告日字段损坏 | 是 |
| `not_applicable` | ETF、指数等不是 A 股财报适用证券 | 否 |

为支持该判定，披露归档现在额外保存当前预约日、首次预约日、预约状态、变更日期
和预约修订历史。`--audit-as-of` 默认使用本地当天，也可显式重放未来/历史评价日。
摘要同时返回选中证券总数、A 股适用数、非适用数、已覆盖、待预约、可行动缺口
以及按状态计数；覆盖率分母只使用 A 股适用证券。

后续已从本地 `T0002/hq_cache/base.dbf` 的 `SSDATE` 补齐上市日期。报告期末早于
上市日的组合现在标记为 `pre_listing` 并排除；解析证据和一键维护流程详见
[上市日期与增量维护记录](2026-08-06-disclosure-listing-maintenance.md)。

## 审计驱动的解析修复

真实银行审计最初发现 8 家沪市银行统一缺少 `20251231`。原始公告证明上游数据
存在，问题来自两种标题/分类边界：

1. 沪市常用“2025年度报告”，其中“年”同时是年份后缀和报告短语首字。年份扫描
   使用严格小于，错误排除了该位置；改为包含边界后，招商银行等 7 家恢复年报。
2. 浙商银行的正式年报被上游归入通用 `LSGG`，摘要仍是 `101`。现在仅当
   `typename` 对应年报/季报且标题以精确正式报告短语结尾时接受 `LSGG`；审计、
   履职、第三支柱等同桶文件仍不能解锁专业财务包。

真实招商银行返回的“2025年度报告”现识别为 `20251231 / 20260328`；浙商银行
`LSGG` 正文与 `101` 摘要配对后选择正文，识别为 `20251231 / 20260331`，并保留
两条公告证据。

## 真实验收

自选股审计：

- 37 个条目中 34 只 A 股、2 只 ETF、1 个指数；
- 四季度适用组合 136 个，已覆盖 103、等待未来预约日 33；
- 可立即回补缺口 0，ETF/指数 12 个期间组合标为 `not_applicable`；
- 覆盖率 75.74%，剩余未覆盖均是尚未到期的 `20260630` 半年报。

银行父行业审计与修复：

- `research-industry:X50` 递归得到 42 只 A 股；
- 初始四季度覆盖率 3.57%，批量回补后为 70.24%；
- 修复沪市标题和 `LSGG` 后，168 个组合中已覆盖 126、等待预约日 42；
- 当前评价日可行动缺口 0，覆盖率 75%；
- 将评价日模拟为 `20260901` 时，39 个预约过期和 3 个快报未转正式报告进入
  42 只证券队列，证明未来预约不会被提前请求，过期后会自动转为可行动状态。

当前归档共有 5,857 个“证券×报告期”条目、436 个正式报告实际披露时点；批量
状态含 79 只证券，完成 79、失败 0。全套原生测试 `26/26` 通过。

验收产物：

- `output/probes/disclosure-coverage-watchlist-closed.json`
- `output/probes/disclosure-coverage-bank-closed.json`
- `output/probes/disclosure-coverage-bank-as-of-20260901.json`
- `output/probes/disclosure-coverage-300503-explicit.json`
- `output/probes/disclosures-announcement-600036-fixed.json`
- `output/probes/disclosures-announcement-601916-fixed.json`

最终版本已部署到 `dist/tdx-tool/bin/tdx-tool.exe`，SHA-256 为
`C3D207A99164B13846ACA45B4FF98236F2E8F4EB24C0C08BA2E6DF1E0FEC7BE5`，服务
进程为 `14948`。部署后二进制再次审计自选股，得到 34 只适用 A 股、33 个未来
预约、0 个可行动缺口；健康检查返回 `ok=true`、`native_cpp=true`、
`python_runtime=false`。
