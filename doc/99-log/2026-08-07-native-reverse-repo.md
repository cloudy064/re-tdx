# 国债逆回购实时年化、净收益与交收日历

## 客户端关系与资源

客户端同时保留三个逆回购页面：

```text
cloud_dax/GZNHG.sp   通用回购 -> func_gznhg100（沪深合并）
cloud_pad/GZNHG.sp   国债逆回购 -> func_gznhg101（沪）+ func_gznhg102（深）
cloud_dax/ZQ_GZHG.sp 通用回购 -> gxjty_zq_gznhg101（沪深合并替代页）
```

2026-08-07 通过 709/1721 在线刷新并校验：

| 资源 | MD5 | 行数 | 口径 |
|---|---|---:|---|
| `func_gznhg100_1` | `407c0a0b3ba10f922f78e5a5d852db1a` | 18 | 沪深合并 |
| `func_gznhg101_1` | `b6afac9209695b36ad0bb668c50815ee` | 9 | 上海 GC |
| `func_gznhg102_1` | `1b69e8740f4af164d51b395c347bdd91` | 9 | 深圳 R |
| `gxjty_zq_gznhg101_1` | `615c4f3dc189713dd3525a8d5f56ab50` | 18 | 沪深替代页 |

四表交收字段逐行一致。沪市为 `204001..204182` 九个 GC 品种，深市为
`131810..131806` 九个 R 品种。证券目录 `0x044D/0x044E` 也恰好返回 18 个
`category=repo` 品种，名称和代码可完整对账。

## 字段语义和客户端公式

CFG 明确给出：

- `TS`：合约期限天数；
- `SJTS`：实际计息天数；
- `SXF`：每十万元手续费（元）；
- `SCZJJS`：首次资金交收日；
- `ZJKY`：资金可用日；
- `ZJKQ`：资金可取日；
- `YL = 1000 * $NOW * SJTS / 365`：十万元本金毛收益。

`$NOW/$MAX/$MIN/$ZCJJE` 是宿主行情列，不在 JSN 内。为了恢复完整页面，服务
用单次公开 `0x054C` 请求批量获取 18 个品种的年化率、日内高低和成交额，再把
客户端公式推广到查询参数 `principal_yuan`：

```text
毛收益 = 本金 × 年化率 / 100 × 实际计息天数 / 365
手续费 = 每十万元手续费 × 本金 / 100000
净收益 = 毛收益 - 手续费
净年化 = 净收益 / 本金 × 365 / 实际计息天数 × 100
```

`TS` 与 `SJTS` 不可混用。2026-08-07 为星期五，一日品种合约期限为 1 天但
实际计息 3 天；182 日品种因节假日实际计息 192 天。

## 行情尺度缺口与修复

原 `0x054C/0x0547` 解码器只对债券和基金前缀调整价格尺度，导致逆回购年化率
放大 100 倍：`GC001` 的 1.300% 被输出为 130。现在公共函数
`market_price_divisor_for_code` 对 `204` 和 `1318` 前缀使用额外的 100 除数，
快照与五档共同修复；普通股票、基金和债券原有尺度回归测试保持通过。

修复后在线样本：

- `GC001`：1.300%，一日合约实际计息 3 天，10 万元毛收益 10.68 元、手续费
  1 元、净收益 9.68 元；
- `GC182`：1.405%，实际计息 192 天，10 万元毛收益 739.07 元、手续费 30 元、
  净收益 709.07 元、净年化 1.348%。

闭市后 `0x054C` 返回最近快照，不描述为新成交；行情请求失败时，接口仍以
`schedule-only` 返回完整交收日历和手续费，不让实时源故障遮蔽静态可用信息。

## 纯 C++ 接口与网页

```powershell
tdx-tool market reverse-repo --view rates --principal-yuan 100000 --sort net-rate
tdx-tool market reverse-repo --view rates --market sh --max-term-days 14
tdx-tool market reverse-repo --view security --market sh --code 204001
tdx-tool market reverse-repo --view rates --no-quotes
tdx-tool market reverse-repo --view catalog
```

固定 API 为 `/api/v1/market/reverse-repo`。支持市场、品种、期限、本金、搜索、
排序、分页、行情开关、日历缓存和 5 秒行情缓存。数据中心新增“国债逆回购”，
可切沪深市场、输入本金并按净年化、报价年化、净收益、期限或成交额排序。
这是一项现金管理工具，不属于股票关系，因此没有增加个股工作台页签。

## 覆盖与验证

- 四个等价静态模板绑定 `market reverse-repo`；覆盖由 262/616 提升到 266/616，
  通用独占降至 350，已下载通用独占由 66 降至 65，解析错误为 0；
- CTest 59/59；新增逆回购规范化/公式测试，并扩展行情尺度回归测试；
- 新增实时收益和无行情交收日历两项在线契约，全量 API 契约 61/61；
- Svelte 检查 0 错误、0 警告，生产构建通过；
- 正式 API 实测 18/18 行取得行情，`quote_source.command=0x054C`；
- 正式服务 `http://127.0.0.1:8765`，PID `23968`，功能目录 104 项；
- 可执行文件 SHA-256：
  `7F6499600C7839F14C513A1BAEC1230B3FED4B09AC5FCB56EB13304C66518DFA`。

证据文件：

- `output/probes/reverse-repo-current/`；
- `output/probes/reverse-repo-current/snapshot-fixed.json`；
- `output/probes/reverse-repo-current/rates.json`；
- `output/probes/reverse-repo-current/contracts-live.json`；
- `output/probes/reverse-repo-current/api-contracts-full.json`；
- `output/probes/reverse-repo-current/jsn-variants.json`；
- `output/probes/reverse-repo-current/ui.png`。

## 下一步

覆盖排序中的下一项 `kjhz_kjhzsy201` 只有 2 行且与现有可转债条款重叠，优先级
不应只按分数机械决定。更值得审计的是 `jysjk101/102` 交易监管期限/区间表现与
`hylhb101/102/104` 活跃龙虎榜周期统计；随后再判断 `gqgg101..104` 国企改革
分组是否只是现有板块数据的另一套聚合视图。
