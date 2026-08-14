# JSN 语义覆盖审计与个性数据全景

## 目标与口径

过去的 `jsn download/catalog/query` 已能下载和横向检索资源，但它只能证明资源
可达，不能证明字段、参数分支和业务关系已经成为稳定的 C++ 功能。本轮新增纯 C++
`recon jsn-variants`，把以下三种状态严格分开：

- 模板可发现：来自 `reqformat=11` XML、`T0002/cloud_cfg/*.cfg` 或已确认的有限
  动态键；
- 文件已下载：本地 JSN 能匹配某个静态模板或动态模板；
- 语义已类型化：存在专用 C++ 命令/API，通用下载、编目和查询不计入覆盖。

审计只读，不联网、不改通达信文件。默认在仍有通用缺口时返回非零，探索时可用
`--allow-gaps`，也可用 `--gaps-only --top N` 生成优先队列。

## 616 个模板的真实审计

统一清单与历史发现口径完全对齐：

- 616 个资源模板，其中静态 541、动态 75；
- XML 来源文件 29 份，覆盖 23 个模板；CFG 来源文件 611 份，覆盖 614 个模板；
- 本地下载文件 330 个，全部匹配到候选，涉及 291 个模板；
- 解析错误模板 0；
- 新功能加入前为类型化 167、通用独占 449，其中已下载但通用独占 149；
- 加入十视图全景后为类型化 177、通用独占 439，其中已下载但通用独占 139。

当前证据为 `output/probes/tdx-jsn-variants-current.json`。未把 439 个缺口包装成
“已完成”，它们是后续迭代的可排序工作队列。剩余首批资源族为：`cgfx` 17/17
已下载、26,948 行；`hsgt` 14 个模板、7 个已下载、1,405 行；`zcjc` 6/6、
600 行；`gdrs` 6 个模板、5 个已下载、5,416 行；其次为 `zdtfx`、`qszj`、
`gqgg` 和 `bkld`。

## 十视图个性数据全景

`market panorama` 与 `/api/v1/market/panorama` 首批固定十张客户端“个性数据”
主表：

1. `quality-rating`：质量/安全评分；
2. `capital-flow`：1/5/10/20/30 日资金流；
3. `risk-watch`：风险关注；
4. `financials`：财务摘要；
5. `earnings-forecast`：业绩预告；
6. `analyst-estimate`：分析师预期；
7. `distribution`：分布数据；
8. `lhb-overview`：龙虎概览；
9. `margin-overview`：两融概览；
10. `ownership-change`：增减持概览。

`view=catalog` 不联网返回资源与字段目录；单视图支持全市场过滤和分页；
`view=security&market=sz&code=000001` 一次聚合十张表。响应统一证券标识和语义字段，
同时保留 `raw` 原始行。静态 JSN 中没有的客户端实时计算列不会被伪造。

平安银行真实聚合中，质量、资金、财务、分析师预期、分布和两融六张表命中；风险、
业绩预告、龙虎和增减持四张当前表无记录，按正常空关系返回。资金流单表命中日期
`20260806`，资源 `list/func_gx_zjlx101_1.jsn`，上游首次成功且非陈旧。证据为
`output/probes/panorama-capital-flow-000001.json` 与
`output/probes/panorama-security-000001.json`。

## 验证与部署

- 新增 `tdx-jsn-variants-tests`、`tdx-panorama-tests`，并扩展 API 契约测试；
- 全量原生 CTest 47/47；临时服务和正式服务完整契约均为 28/28；
- 正式服务 PID `37096`，命令行
  `dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`；
- 部署 EXE SHA-256：
  `77B9A1B209C7C3DE00833E8AC9D69EF5E440BA14C51766AE992DBFFE69C6C6A0`；
- 功能目录 93 项，`native_cpp=true`、`python_runtime=false`；
- 本轮未修改 Svelte UI。HTML、JS、CSS 发布哈希仍分别为
  `7C65BE586AE0744C201F857B9837A6525732B74D0F2D6A0BDDF001ACB632F534`、
  `9DD7BA36591D8D44D6B480CEB74159F08C18D8F26DB781F476198087246BF849`、
  `274DAB2086C2B8E352F4A4DA7B075F8B86D664A4DAF189ACEBDBDE60685D1F19`。

## 下一批高收益方向

~~优先处理 `cgfx` 的 17 张机构持仓/分析主表，复用现有机构与证券模型形成
全景视图。~~ 已完成，详见
[机构持仓十七视图](2026-08-06-native-institution-analysis.md)。覆盖审计当前首位
~~已变为 `hsgt`。~~ `hsgt` 已完成，详见
[沪深港通十四模板语义补齐](2026-08-06-native-stock-connect-gaps.md)；当前首位为
`zcjc`，其后是 `gdrs`、`zdtfx` 和 `qszj`。
