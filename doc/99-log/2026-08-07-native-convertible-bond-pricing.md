# 可转债收益表、公开 L1 与现金流定价

## 目标

继续处理 2026-08-07 静态候选中的高价值公开数据，将可转债收益表并入既有
`market convertible-bonds`，不新造割裂命令，也不涉及 L2。重点是严格区分
服务端 JSN 字段、行情宿主列和客户端计算列。

## 字段与公式证据

确认下载的 `list/gxjty_zq_kzzsy101_1.jsn` 为 1,242,522 字节，MD5
`af205d9a7c25e42c9c24235f51e97a2f`，包含 309 行、43 列：307 只可转债和
2 只可交换债。安装目录中的四份 `func_kzz_*.cfg` 明确给出：

- `MZ/ZGJ/ZGQSR/ZGJZR` 为面值、转股价和转股期；
- `FXRQXL/FXLLXL` 与 `SYFXRQXL/SYFXLLXL` 为完整及剩余付息日期/利率；
- `SGFXRQ/XGFXRQ` 为上次和下次付息日；
- `SYNXSYL` 为按剩余期限插值的企业债收益率；
- `$NOW3/$BONDAI/ZGXJ` 是宿主行情/计算列，并不在 JSN 原始字段中；
- 客户端全价为净价加应计利息，转股价值为 `正股价*面值/转股价`。

招募说明书文本同时明确应计利息使用 `IA=B*i*t/365`。据此 C++ 使用公开
`0x054C` L1 的债券净价和正股现价，按 Actual/365 复算应计利息，再从披露的
剩余现金流用二分法求 YTM，并用 `SYNXSYL` 折现得到纯债价值。双低值定义为
净价加转股溢价率。所有推导字段都放在独立 `valuation` 节点，完整 JSN 行保留
在 `raw`，不会冒充上游字段。

盘前 `last_price=0` 时使用非零 `pre_close_price`，并在
`bond_price_source/underlying_price_source` 标记 `pre-close`；盘中自动切回
`last-price`。同轮在线契约还暴露了国债逆回购的同类盘前零价问题，该功能也改为
昨收回退并新增单元测试。

## 实现

现有命令和 API 新增 `view=pricing`：

```powershell
tdx-tool market convertible-bonds --root C:\new_tdx --view pricing `
  --sort double-low --order asc
```

能力包括：

- 全价、转股价值/溢价、到期收益率、纯债价值/溢价和双低值；
- 债券/正股公开 L1、涨跌幅、成交额及盘前昨收来源标记；
- 票息日程、剩余现金流、期限、评级、三个触发比例及触发价格；
- 转债或正股的双向证券过滤、文本检索、存续过滤和七种排序；
- 静态收益表 900 秒缓存、按证券请求集合隔离的 L1 五秒缓存；单票工作台只请求
  关联债券与正股，不再为单票拉取全市场约 600 个行情代码；
- 行情失败时保留 `terms-only` 静态条款，不把零价误判为完整估值。

Svelte 数据中心的可转债页默认进入“实时定价”，可切换“已上市条款”和“待发
方案”；个股工作台的“关联转债”页新增实时定价与收益面板。

## 实测结果

2026-08-07 盘前目录为 309 条，308 条债券行情和 308 条正股行情可形成完整估值；
唯一无债券行情的是退市整理后的 `SZ404004 汇车退债`，其条款仍保留为
`terms-only`。固定样本 `SH110076 华海转债 → SH600521 华海药业` 使用昨收
114.98/16.46，复算结果为：

- 应计利息 7.616438，估算全价 122.596438；
- 转股价值 99.757576，转股溢价率 22.894364%；
- 到期收益率 -36.546033%，纯债价值 109.604251；
- 双低值 137.874364。

这些值由确定性单元测试固定，在线值会随 L1 行情变化。

## 验证与部署

- C++ 全量测试 67/67；
- Svelte 检查 0 错误、0 警告，生产构建通过；
- 新增 `convertible-bond-pricing-live` 在线契约通过，固定契约总数增至 100；
- 正式服务为 `http://127.0.0.1:8765`，PID `42176`，功能目录仍为 115 项；
- 证据文件：
  - `output/probes/convertible-bond-pricing-catalog.json`；
  - `output/probes/api-convertible-bond-pricing-110076.json`；
  - `output/probes/api-contract-convertible-bond-pricing-20260807.json`；
  - `output/probes/api-contract-full-final-20260807-convertible-bond-pricing.json`。

发布 EXE SHA-256 为
`290EC4D1D73D4557B1033E5696DF8DD93F0D3F3BAA065D64058CCF4F79C49055`；HTML、
JS、CSS SHA-256 分别为
`D59DF5A09B3D93A17B7AF4E428B50F25F2A26BD6B22E7D38918371F16DED9215`、
`77A3759F5F5EAA8BD2AE4BEC1C1E7D0DA28D05150E63573212F5539821BB2701`、
`8DAE8079E47DEAD4B188B36550F3AC70E169BCC1BD3AEC861E0A6C9990BA1FED`。

## 下一候选

下一批继续按高收益和可证明语义排序：优先审计约 1.21 MiB 的
`func_cbpl101_1` 财报披露主表，判断它是否能补齐现有 `market disclosures` 的
公司级披露计划/状态；随后再比较 A/B 股行情日历、股改/限售日历与定向增发表，
只合并真正新增的语义。
