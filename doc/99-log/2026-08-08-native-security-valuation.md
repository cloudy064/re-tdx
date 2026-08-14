# 个股四项原生估值列闭环

> 日期：2026-08-08。范围：非 L2、TdxZdView 系统列、TdxW 缓存和纯 C++ 工具。

## 逆向结论

TdxZdView 的 22 字节列描述符把 49—52 依次定义为 `$PE`、`$PES`、
`$PETTM`、`$PBMRQ`。系统列批记录与 TdxW 回调的精确映射为：

| 系统列 | 含义 | TdxW 来源 |
| --- | --- | --- |
| `$PE` | 动态市盈率 | type 120 `Destination+60` |
| `$PES` | 静态市盈率 | type 163 `Destination+95` |
| `$PETTM` | 市盈率(TTM) | type 163 `Destination+99` |
| `$PBMRQ` | 市净率(MRQ) | type 163 `Destination+380` |

type 163 又把静态 PE、TTM PE 分别取自 688 字节证券缓存的 `+117/+113`。
该缓存不是 TNF 固有字段：`sub_5A5B00` 解压并逐行读取 `zhb.zip` 内的
`tdxstat.cfg`，把第 10、4 字段写入上述位置。PB 在有效每股净资产存在时按
当前价（无效时回退昨收）除以每股净资产；只有该输入缺失时才使用缓存 PB。

动态 PE 的 `sub_5956D0` 口径为：

```text
float 年化EPS = 净利润 × 12 / 报告期月份 / 总股本
float 动态PE = 当前价或昨收 / 年化EPS
```

阈值为严格大于 `0.0001f`，中间年化 EPS 和最终结果都经过单精度舍入。

## 纯 C++ 实现

`market stats` 新增 `--valuation`，要求恰好一个 `--security`。固定只读 API
`/api/v1/market/stats?market=...&code=...&valuation=1` 使用已有五分钟
`zhb.zip` 缓存，并行请求公开 `0x054C` L1 和 `0x0010` 财务包，返回四项指标、
输入值、统计日、财务更新日、价格回退来源和逐项逆向证据。任一实时上游失败时
保留其错误，并让可独立取得的静态/TTM 指标继续返回。

原有 `TdxStatRow` 和网页类型同步增加 `pe_static`；个股工作台“估值与涨停统计”
面板现在显示动态 PE、静态 PE、PE(TTM)、PB(MRQ)、估值使用价和财务更新日。
公式解释器与云计算宿主的 `DYNAINFO(39)/$PE` 同时修正为原版的 `>0.0001f`
阈值和最终单精度舍入。

## 实盘校准

2026-08-08 公开主站结果：

| 股票 | 动态 PE | 静态 PE | PE(TTM) | PB(MRQ) | 状态 |
| --- | ---: | ---: | ---: | ---: | --- |
| 平安银行 `SZ000001` | 3.7380745 | 5.1299000 | 5.0799999 | 0.4680050 | 4/4 |
| 华海药业 `SH600521` | 16.2533207 | 92.5105972 | 66.0400009 | 2.7353492 | 4/4 |

展示值保留宿主单精度结果；网页按两位小数格式化。两只样本统计日均为
`20260806`，且返回零上游错误。

## 证据与产物

- `output/ida-tdxw-finance-record-accessor-xrefs.json`
- `output/ida-tdxw-valuation-message-parser-xrefs.json`
- `output/ida-tdxw-finance-store-range.json`
- `output/ida-tdxw-valuation-upstream-helpers.json`
- `output/probe-stats-valuation-000001.json`
- `output/probe-stats-valuation-600521.json`

## 验证与部署

- 原生 CTest：96/96 通过；
- 新增 `stock-native-valuation-live` 契约，单项正式服务验证 1/1 通过；
- 正式服务 full API 契约：133/133 通过，报告为
  `output/api-contracts-security-valuation-full.json`；
- 安装版 SHA-256：
  `C706E6EE434B6A41D77E251716D3EFB229F8C4BE7CD426A56846DABCB46956C2`；
- 正式服务继续监听 `127.0.0.1:8765`，部署进程 PID 为 `40168`。
