# 股东增减持六类聚合榜

## 页面语义恢复

JSN 覆盖审计中的 `zcjc` 剩余 6 个模板全部已下载，每张 100 行、合计 600 行。
客户端 `ZCJC.sp` 和对应 GBK CFG 明确表明它们不是新的持仓数据源，而是现有
股东增减持页面的六个全市场聚合榜：

- `func_zcjc102/103/104`：增持比例最高、增持市值最高、增持次数最多；
- `func_zcjc108/109/110`：减持比例最高、减持市值最高、减持次数最多。

各表保留统计起止日、增减股数/市值/次数、净变化、占流通比例及成交价格区间。
CFG 中的“截至收益率”依赖实时 `$NOW`，静态 JSN 不含该列，因此本轮没有伪造
收益率或行情字段。

## 纯 C++ 类型化实现

扩展已有 `market ownership` 与 `/api/v1/market/ownership`，新增
`view=rankings`，支持：

- `increase-ratio / increase-value / increase-count`；
- `decrease-ratio / decrease-value / decrease-count`；
- `all` 聚合六榜，或继续使用市场、代码、名称搜索和分页过滤；
- 万股统一换算为股、万元统一换算为元；减持净股数/市值保留上游负号，
  减持占流通比例额外规范为负方向；
- 保留上游榜内顺序和 `rank`，并保留完整 `raw` 原始行；
- 六榜使用独立 `rankings_cache_`，只有请求该视图时才下载，不增加原有个股
  股权页的 15 张主表请求量；共享 JSN 三次重试与陈旧缓存仍然生效。

## 真实验证

- `output/probes/ownership-rankings-decrease-ratio.json`：减持比例榜前五，榜首
  怡合达，上游 `22.44%` 规范为 `signed_float_change_pct=-22.44`，净变动股数
  为 `-103,626,300` 股；
- `output/probes/ownership-rankings-bj920119.json`：北交所 `920119` 正确解析为
  美德乐，并命中增持比例榜；
- 六个来源首次读取均非陈旧，汇总行数为 600；
- `output/probes/tdx-jsn-variants-current.json`：`zcjc` 整族退出缺口；
- `output/probes/api-contracts-official-current.json`：正式服务 32/32 契约通过。

覆盖审计由 208/616 提升为 214/616，通用独占从 408 降为 402，已下载但
通用独占从 115 降为 109，解析错误保持 0。当前最高收益缺口变为 `gdrs`：
6 个模板中 5 个已下载，共 5,416 行。

## 验证与部署

- `tdx-ownership-tests` 新增单位、符号、北交所与原始行的确定性覆盖；
- 新增 `ownership-rankings-live` 正式契约，拒绝把减持比例输出成正方向；
- 全量原生 CTest 48/48，临时及正式完整 API 契约 32/32；
- 正式服务 PID `34664`，命令行
  `dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`；
- 部署 EXE SHA-256：
  `3933698D3502CCFDF7318722CA6CA93DCBC63F2EF7AD43E57D6F6110A5776C1F`；
- 功能目录仍为 94 项，`native_cpp=true`、`python_runtime=false`；本轮未修改
  Svelte UI。

## 下一批高收益方向

~~优先审计 `gdrs` 股东人数/户均持股六模板，并与已有
`market industry-profile` 中的行业股东结构及个股 F10 股东链对账。~~ 已完成，
详见[五市场股东人数全景](2026-08-06-native-shareholder-counts.md)。下一组为
`zdtfx`，其后是 `qszj` 和 `gqgg`。
