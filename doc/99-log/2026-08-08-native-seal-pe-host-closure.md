# 封单字段、动态 PE 与宿主列全覆盖

> 日期：2026-08-08。范围：非 L2、纯 C++ TBigData 云配置宿主列与 TCalc 解释器。

## 结论

当前安装的 660 个 `cloud_cfg` 共含 2570 次有效系统列声明。此前剩余的
`$FCAMO`、`$FCB` 和 `$PE` 已有严格自动来源，审计结果为 2570/2570（100%）；
1370 条计算列仍为 1370/1370 可执行、零解析错误。

## 封单额与封成比

TBigData selector 4 读取 TdxW 类型 163 的 `Destination+384/+388`。
纯 C++ 实现复刻了 `sub_5AA080/sub_9B37A0`：

- 主板、ST 规则生效日期、创业板、科创板、北交所、N 股及特殊证券涨跌停；
- 公开 `0x0452` 逐证券当日边界优先，本地 `hqrule.dat` 为严格回退；
- 360 字节 TNF `+78` 的交易单位；
- 盘口状态拒绝、连续竞价涨停/跌停、集合竞价净委托和第二档承接；
- 涨停返回正值，跌停按原生语义返回负值。

真实 SH603228 的结果为 `$FCAMO=651186090`、
`$FCB=0.2008569411943874`，与公开封板榜的
`94.95 × 68582 × 100` 和 `68582 / 341447` 完全一致。

## `$PE` 与解释器修正

TdxZdView 描述表证明 `$PE` 是“动态市盈率”，相邻的 `$PES` 才是静态市盈率，
`$PETTM` 为 TTM 市盈率。TCalc 的 `DYNAINFO(39)` 请求 TdxW 类型 105，读取
`Destination+185` 的年化每股利润：

```text
净利润 × 12 / 报告期月份 / 总股本
```

随后用现价（无有效现价时回退昨收）除以上述值。统一工具从公开 `0x0010`
财务包与 L1 快照严格构造该值；SH603468 实盘为 `33.61823777538364`。
公式解释器的 `DYNAINFO(39)` 自动上下文同步改用该口径，不再误用原始 EPS。

## `price2` 生命周期边界

全配置审计里仅剩的两次外部 `price2` 引用都来自 `func_qxfa201` 的隐藏锁定期
收益列。该页的 `date2` 是未来解锁日，所以“解锁日前收盘价”尚不存在；解锁后
记录进入 `func_qxfa202`，届时 `price2` 是明确的 JSN 列。审计结果现标为
`upstream-lifecycle-conditional`，不会再误报为需要用户手工提供。

## 证据与产物

- `output/tdx-tbigdata-cloud-calc-audit.json`
- `output/tdx-tbigdata-cloud-calc-seal-603228-live.json`
- `output/tdx-tbigdata-cloud-calc-pe-live.json`
- `output/formula-dynainfo39-603468-live.json`
- `output/ida-tdxw-type105-security-info.json`
- `output/ida-tdxzdview-pe-labels.json`

## 验收

- 原生 CTest：96/96 通过；
- 正式服务 API 契约：132/132 通过，133 次网络请求；
- 正式服务：`127.0.0.1:8765`，PID `40400`；
- 部署 SHA-256：`E9322D21CEB4D27CAE030F7F0E70051F6801621A8DC04E9E42297C4ADA6746AF`；
- 契约报告：`output/api-contracts-seal-pe-price2-closure-full.json`。
