# A 股全市场业绩预告主表与动态截面对账

## 结论

`list/func_cbpl101_1.jsn` 不是财报预约披露日历，而是“业绩预告”页的 A 股
全市场静态主表。它与现有 `market forecasts` 的行业统计/动态明细语义相同但
刷新截面不同，因此并入既有命令的 `view=latest`，没有新造功能入口。

远端资源为 1,212,113 字节，MD5
`522226a1f2d5d08832d2a3736e01b0d9`，共 1,805 行、18 列：深市 933、沪市 838、
北市 34；公告日期覆盖 2026-01-23 至 2026-08-07。报告期分布为中报 1,796、
三季报 5、年报 4。1,805 行均有正文和原因，1,801 行有净利润下限，1,800 行
有同比下限。

## 与现有能力的边界

现有 `func_yjygtj101_1` 全 A 统计声明 1,846 条中报预告，对应动态资源
`yjyg/88000120260630.jsn` 也有 1,846 行。静态主表与动态表按市场、代码和报告期
对账后：静态主表少 50 条当前中报，但多 9 条未来三季报/年报，不能互相覆盖。
API 因此同时返回：

- `current_report_expected=1846`；
- `current_report_rows=1796`；
- `current_report_gap=50`；
- `future_report_rows=9`。

CFG 还证明两张表的同比尺度不同：静态表 `zj1/zj2` 是直接百分比，动态表
`zj3/zj4` 是小数。统一 normalizer 分别采用直接值和乘 100，单元测试固定
20%/30% 样本，防止出现 100 倍错误。

## 产品接入

- CLI：`market forecasts --view latest`；
- API：`/api/v1/market/forecasts?view=latest`；
- 支持方向、文本、市场/代码、报告期和数量过滤；
- 返回净利润区间、同比区间、上年同期、年化利润、EPS、股本、正文、原因、
  `source_variant=all-market-static` 及完整 `raw`；
- Svelte“业绩预告”页新增默认的“全市场最新”视图，点击证券进入个股工作台；
- JSN 类型化覆盖把 `func_cbpl101_1` 加入现有 `market forecasts` 资源族。

## 验证与部署

- C++ 67/67；Svelte check 0/0，生产构建通过；
- `forecast-latest-live`、可转债定价和盘前逆回购三项在线契约 3/3；
- 全量在线契约 100/101，唯一失败为既有两融分类资源在上游临时返回零字节；
  新增与本轮修复契约均通过；
- 本地 JSN 目录 354 个资源、168,465 行、33,351,080 字节；
- 正式服务 `http://127.0.0.1:8765`，PID `42828`，功能目录 115 项。

证据：

- `output/probes/forecast-latest-sample.json`；
- `output/probes/api-contract-forecast-pricing-repo-20260807.json`；
- `output/probes/api-contract-full-final-20260807-forecast-latest.json`。

最终 EXE SHA-256 为
`EE926E61EB3B43E30992BCD0A51348B86359B5E422FF14F55458D274763B0483`；HTML、
JS、CSS SHA-256 分别为
`A2978B2C604C573BD86C3745EFA7A0CEFB4C0107F91ED564D450C46186F8ACBC`、
`063F621BDA7E3621F00D0E0BA5E2288A09C426F3EAE1608C2DF7B4F06436C36C`、
`8DAE8079E47DEAD4B188B36550F3AC70E169BCC1BD3AEC861E0A6C9990BA1FED`。

## 下一步

剩余高价值静态候选应转向 A/B 股行情日历、股改/限售日历和定向增发，先与现有
`market calendar`、`market ownership/unlocks` 和发行统计做字段级去重，再决定
是合并视图还是仅保留为已审计重复资源。
