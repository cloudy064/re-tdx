# 2026-08-08：价值关注、举牌与政策性金融债

本轮从已处理的通达信配置与实际 `709→1721` 数据继续收口三个公开能力，全部由
纯 C++ 直接请求、解析和提供 API，不使用 Python 转发。

## 价值关注（JZGZ）

- `list/func_jzgz101_1.jsn` 恢复 8 个“跌破相关价格/价值挖掘”口径；
- 主表当前得到 1,254 条口径内去重关系、1,015 只去重股票；
- `jzgz1/<分类ID>.jsn` 动态明细按需加载，并与主表内嵌成员做集合级对账；
- `3109` 当前主表/动态明细均为 39 只，集合精确一致；
- 静态明细保留相关价格、复权相关价格和近三月复权收盘价。客户端跌破幅度依赖
  宿主 `$NOW`，接口将 `breach_depth_pct` 保持为 `null`，不伪造实时现价。

入口为 `market intelligence --view value-attention [--category-id ID]` 和
`/api/v1/market/intelligence?view=value-attention&category_id=3109`。Svelte 市场情报页
支持分类下钻，个股工作台支持反查命中分类。

## 投资参股与被举牌（TZCG）

`market institution-analysis` 从 22 扩展为 25 张类型化主表：

- `func_tzcg106_1`：梧桐树系持股，当前为空表但保留正常空语义；
- `func_tzcg109_1`：中科汇通持股，当前为空表但字段、单位已固定；
- `func_tzcg108_1`：被举牌，当前 46 条、45 只证券。

举牌视图保留公告/起止日期、举牌股东、平均价格、增持及增持后股数/总股本占比、
是否继续增持和是否险资；区间表现按客户端 `(price2/price1-1)*100` 复算。当前
“继续增持=是”10 条、险资 2 条。该视图与普通股东增减持台账分开建模。

## 政策性金融债（JRZ）

`market bond-reference --group category --bucket policy-financial` 新增政策性金融债：

- 全市场主表 `list/zqjrz201.jsn` 当前 10 只；
- 沪市投影 `list/zq_jrz201_1.jsn` 7 只，深市投影
  `list/zq_jrz201_2.jsn` 3 只；
- 两个投影的并集与全市场主表集合精确一致；
- `GM` 明确按亿元读取，API 同时保留源亿元值并换算成基础元。

## 覆盖、隔离与验证

- JSN 变体审计：621 个模板、561 个类型化、60 个剩余通用模板；正式镜像
  574 个文件、510 个命中模板、0 个解析错误、0 个已下载通用模板；
- 4 张尚待继续解释的可转债投影/子集表和 1 张疑似陈旧的虚拟现实表，移入
  `output/tdx-jsn-probes/20260808-next-small/`，不计入正式支持面；
- CTest：98/98；Svelte：0 error / 0 warning，生产构建成功；
- 新增 3 项真实 API 契约，完整契约由 155 增至 158，临时服务实测 158/158。

正式发行包已部署到 `127.0.0.1:8765`，进程 PID 为 `28812`；健康检查显示
`native_cpp=true`、`python_runtime=false`、574 个 JSN 文件、47,668 个证券键和
379 条公式。正式服务全量契约为 158/158，发行版 `tdx-tool.exe` SHA-256 为
`A032DEEE508FBBFA08A9006DBCA81B8CEFC732DD49FA12AADD2E1F29A8B8A030`。

关键证据：

- `output/tdx-value-attention-native.json`
- `output/tdx-institution-stake-building-native.json`
- `output/tdx-bond-policy-financial-native.json`
- `output/tdx-jsn-variants.json`
- `output/probes/api-contracts-full-temp-20260808-jzgz-stake-policy.json`
- `output/probes/api-contracts-full-formal-20260808-jzgz-stake-policy.json`
