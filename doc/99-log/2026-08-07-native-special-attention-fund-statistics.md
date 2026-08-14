# 特别关注、基金统计与公式转折距离

## 目标

- 从剩余 JSN 模板中继续选择非 L2、数据真实且与既有页面不重复的高收益功能；
- 补齐 TCalc 解释器中已有转折点序列但缺少的距离函数；
- 所有新能力继续由纯 C++ 实现，并接入固定 API 与 Svelte 页面。

## TBGZ 特别关注

`TBGZ.sp` 声明 101—110 十个功能。先以 7709 `709` 只查元数据，再下载有真实
长度的资源。当前结果为：

| 功能 | 资源 | 行数 | 结论 |
| --- | --- | ---: | --- |
| 股权分散 | `func_tbgz102_1` | 170 | 第一大股东、比例、行业、地区、截止日 |
| 可能成为 `*ST` | `func_tbgz103_1` | 136 | 风险类型、原因、股东户数和财务快照 |
| 摘星摘帽分析 | `func_tbgz104_1` | 73 | 三期净利润、PB、状态、实施日与说明 |
| 被立案调查 | `func_tbgz106_1` | 467 | 立案、原因、案件文本、进展、处罚日 |
| 商誉风险 | `func_tbgz110_1` | 300 | 两期商誉、利润、营收及客户端比例 |

五表合计 1,146 条、1,019 只去重证券，日期范围 `20160122—20260807`；立案表
当前 283 条按 `aqjz` 和空处罚日标为调查中。`$SC1/$ZQDM1` 才是被调查证券，
`$ZQDM` 是源事件键。摘星摘帽的 `jlr1/2/3` 为万元，规范层换算为元；商誉和
`*ST` 财务字段直接为元；`zxgdhs` 同时给出户和万户显示值。

`func_tbgz101/105/107/109` 当前连元数据都不存在，因此结论是“上游未下发”，
不是可以稳定依赖的空表。`func_tbgz108` 基金独门此前已进入机构持仓分析，未重复
建设。

新增纯 C++ `market special-attention`、
`/api/v1/market/special-attention`、数据中心“特别关注”和个股工作台同名页签。

## JJTJ 基金统计

覆盖审计显示下一批高收益候选是 `JJTJ.sp`。9 个模板的元数据均非空，下载后
共 3,811 行：

- 新发基金 244 条；
- 基金分红 581 条；
- 股票型基金多周期收益 2,723 条；
- 基金市场规模和规模图表各 20 条；
- ETF 市场规模和申购净量图表各 23 条；
- A 股 ETF 周度成交、份额、融资融券 51 条；
- 新上市基金 126 条。

这组数据和既有基金持仓、ETF 当前表现不是同一口径。新增纯 C++
`market fund-statistics` 与 `/api/v1/market/fund-statistics`，覆盖 3,618 个去重
基金。市场号 33 被建模为 `fund/FUND`，没有冒充 A 股市场。CFG 明示的亿元、
亿份字段同时输出原始亿单位和乘以 `1e8` 的元/份字段。`PXBL` 与分红说明的关系
不足以唯一证明现金单位，因此只保留 `distribution_ratio_raw` 和原始 `FHSM`，
不做未经证明的现金换算。

Svelte 数据中心新增“基金统计”，提供股票基金收益、新发、分红、新上市、基金
规模、ETF 规模申赎和 ETF 周度七类视图；基金规模、ETF 规模、ETF 周成交额使用
Lightweight Charts 展示可缩放历史序列。

## 公式解释器

`PEAKBARS(K,N,M)` 与 `TROUGHBARS(K,N,M)` 复用解释器已有的 `ZIG(K,N)` 转折
序列，返回当前 K 线距最近第 `M` 个峰/谷的根数。两者仍是未来函数，只允许显式
开启的只读求值，扫描和回测继续拒绝。公式网页同时显示后端
`tdx-formula-render-ir-v1` 的图元数与事件数。

全库 379 条内置公式依然全部可解析且数值信号安全。普通非 L2 公式没有新的语法
缺口；剩余自动上下文不可用项只包括 14 条 L2 公式和 6 条需要合法宿主序列的
`SIGNALS_QS`，没有用常数或零值伪造。

## 验证与发布

- `tdx-special-attention-tests`、`tdx-fund-statistics-tests`、
  `tdx-formula-engine-tests`、`tdx-recon-contract-tests` 通过；
- `svelte-check` 为 0 error / 0 warning，Vite 生产构建通过；
- 特别关注上线后 full 契约为 109/109；基金统计加入后为 110/110；
- 最终 JSN 覆盖为 382/616 类型化、234 个未下载/未类型化模板；本地 401 个
  文件涉及 346 个模板，全部匹配，解析错误 0，已下载 generic-only 为 0；
- 正式服务已更新到 `http://127.0.0.1:8765`，启动时显式指定绝对
  `--jsn-root`；正式端口的 OpenAPI、特别关注和基金统计 3/3 契约通过。

主要证据文件：

- `output/probes/tdx-market-special-attention-offline-20260807.json`
- `output/probes/tdx-market-fund-statistics-offline-20260807.json`
- `output/probes/tdx-jsn-variants-after-fund-statistics.json`
- `output/probes/tdx-api-contracts-full-after-fund-statistics-20260807.json`
- `output/probes/tdx-api-contracts-formal-final-20260807.json`

## 下一批候选

剩余模板中优先级较高但尚未探测的是：`HYJYFX` 银行/证券/保险专项经营指标、
`JZFX` 多轮牛熊基准分析、`GSRL` 尚未并入现有日历的配股/特别处理/上市状态，
以及分级基金历史表。后续仍应遵循“先元数据、再样本语义、最后类型化”的顺序，
避免把已下线资源或重复客户端投影误报成新能力。
