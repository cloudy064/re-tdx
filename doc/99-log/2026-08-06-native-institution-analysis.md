# CGFX 十七视图机构持仓全景

## 资源与页面语义

JSN 覆盖审计的首位缺口 `cgfx` 包含 17 张已下载主表，共 26,948 行。客户端
`JGCC.sp` 和对应 CFG 将它们分为：

- 14 张同构机构持仓：全部、券商、保险、社保、私募、公募、银行、财务公司、
  年金、一般法人、QFII、信托、特殊法人和北向资金；
- `func_cgfx114_1.jsn`：养老金持仓、占流通、市值及利润增长；
- `func_cgfx115_1.jsn`：报告期总/流通股本、机构与大股东持股及浮筹结构；
- `func_cgfx116_1.jsn`：前期抱团股从一年高点的跌幅与机构持仓变化。

这 17 张是当前报告期横向主表。现有 `market institution` 的 `cgfxmx1/2` 仍负责
单票历期机构分类、十大流通股东和同股东跨股票链，两者没有被错误合并。

## 纯 C++ 类型化实现

新增 `market institution-analysis` 与
`/api/v1/market/institution-analysis`：

- `view=catalog` 无网络列出 17 个视图、资源、布局、标准字段和单位；
- 单视图支持市场、代码、名称/字段搜索、偏移和分页；
- `view=security&market=sz&code=000001` 一次聚合全部 17 张表；
- 持股环比按客户端 `gs_bd/(gs-gs_bd)` 计算；
- 浮筹按 `报告期流通股本-机构持股-机构外大股东持股` 计算，并同时输出占流通、
  占总股本比例；
- 北向表原始 `zzgbb/zzgbb_bd` 的量纲与其他表不同，严格复现 CFG 中
  `zzgbb1=zzgbb/100` 的规则后再输出统一比例；
- 静态资源没有的现价、涨幅和最新股本不伪造；标准字段之外保留 `raw` 原始行；
- 复用共享 JSN 三次重试、陈旧缓存和来源健康结构，服务层另有 300 秒资源缓存。

## 真实验证

平安银行 `0/000001` 的十七视图实测：

- 命中全部机构、保险、公募、一般法人、特殊法人、浮筹结构和北向，共 7 类；
- 全部机构报告期 `20260331`，机构数 95，持股 `12,765,210,134` 股；
- 浮筹结构已出现 `20260630`；
- 北向持股 `570,772,048` 股，归一后的流通占比 `0.0294`、总股本占比
  `0.029412`，来源首次成功且非陈旧；
- 17 个来源均 `attempts=1/stale=false`，无上游错误。

证据为 `output/probes/institution-analysis-all-000001.json` 和
`output/probes/institution-analysis-security-000001.json`。

JSN 审计随之从 177/616 提升为 194/616；通用独占从 439 降为 422，已下载但
通用独占从 139 降为 122。`cgfx` 整族从缺口清单消失，当前首位为 `hsgt`。

## 验证与部署

- 新增 `tdx-institution-analysis-tests`，覆盖十七视图目录、证券过滤、名称解析、
  派生环比、原始行保留、北向 `/100` 和参数拒绝；
- 全量原生 CTest 48/48；临时与正式完整 API 契约 29/29；
- 正式服务 PID `41952`，命令行
  `dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`；
- 部署 EXE SHA-256：
  `20A98F3B9C9E3555C64E04CA10E7AC0B5DA78B5AE1D9EB58ACDF83BFC2ABBD18`；
- 功能目录 94 项，`native_cpp=true`、`python_runtime=false`；
- 本轮未修改 Svelte UI，HTML/JS/CSS 哈希保持不变。

## 下一批高收益方向

~~审计当前首位是 `hsgt` 的 14 个剩余模板，其中 7 个已下载、1,405 行；应先与
已有 `market stock-connect` 对账，区分真正未覆盖的榜单/港股分支与重复语义。~~
已完成，详见[沪深港通十四模板语义补齐](2026-08-06-native-stock-connect-gaps.md)。
随后处理 `zcjc` 6/6、`gdrs` 5 个已下载模板、`zdtfx` 和 `qszj`。

机构持仓目录后来继续扩展为 19 个视图，详见
[基金独门与汇金证金十九视图固定化](2026-08-06-native-special-institution-holdings.md)。
