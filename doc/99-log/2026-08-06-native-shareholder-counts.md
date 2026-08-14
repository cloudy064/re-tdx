# 五市场股东人数全景与遗留空表

## 页面与资源语义

客户端 `GDRS.sp` 当前只展示五个市场分支：

- `func_gdrs101`：沪市主板；
- `func_gdrs102`：深市主板；
- `func_gdrs104`：创业板；
- `func_gdrs106`：科创板；
- `func_gdrs107`：北证 A 股。

审计还发现 `func_gdrs103`。它对应深市中小板合并前的遗留配置，已经不在
当前 SP 页面中，服务器报告零字节。因而它被建模为正常空关系，不作为下载错误，
也不与已经合并后的深市主板重复统计。

五张已下载样本共 5,416 行；正式服务实时读取时增加到 5,417 行，说明该主表
仍在持续更新。

## 纯 C++ 类型化实现

在现有 `market ownership` 与 `/api/v1/market/ownership` 中新增
`view=shareholder-counts`：

- 支持 `sh-main / sz-main / chinext / star / bj / sz-sme-legacy / all`；
- 可按市场、代码或名称过滤，北交所 `44/2` 市场号统一解析；
- 输出股东户数、起止日、区间天数、户数变化/比例，并复现客户端
  `gdrs3/date3` 的日均变化；
- 户均自由流通股、十大流通/十大股东和机构持股关系同时输出；
- CFG 标为“万股”的三组持股量统一换算为股，原始行保留；
- CFG 中依赖 `$NOW` 的户均市值没有静态现价，因此不伪造；
- 五张当前主表使用独立缓存，仅访问此视图时加载，不影响已有股权页面请求；
- 遗留中小板返回 `availability=empty`、空数组和带 `missing=true` 的确切来源。

## 真实证据

- `output/probes/ownership-shareholder-counts-bj.json`：北证股东人数前五，正式
  汇总 5,417 只证券，股东户数、区间/日均变化和名称均已解析；
- `output/probes/ownership-shareholder-counts-sme-empty.json`：遗留中小板为
  正常空关系，来源锁定 `list/func_gdrs103_1.jsn`；
- `output/probes/tdx-jsn-variants-current.json`：六个 `gdrs` 模板全部退出缺口；
- `output/probes/api-contracts-official-current.json`：正式服务 34/34 契约通过。

覆盖审计由 214/616 提升为 220/616，通用独占从 402 降为 396，已下载但
通用独占从 109 降为 104，解析错误保持 0。当前最高收益缺口变为 `zdtfx`：
5/5 模板已下载，共 1,353 行。

## 验证与部署

- `tdx-ownership-tests` 覆盖北交所、日均派生、万股换算和原始行；
- 新增实时北证股东人数与遗留中小板正常空表两项正式契约；
- 全量原生 CTest 48/48，临时及正式完整 API 契约 34/34；
- 正式服务 PID `36584`，命令行
  `dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`；
- 部署 EXE SHA-256：
  `6B0D45B29B8A7A78C5A7D65FE61E3E1BBB804A30514386F09880610E159F70E3`；
- 功能目录仍为 94 项，`native_cpp=true`、`python_runtime=false`；本轮未修改
  Svelte UI。

## 下一批高收益方向

审计 `zdtfx` 五张涨跌停行为分析主表，并优先复用 `market limit-quality`、
`market abnormal-moves` 或 `market technical-signals` 的现有证券/信号模型；其后
处理 `qszj` 和 `gqgg`。

该方向已完成，见[原生涨跌停复盘](2026-08-06-native-limit-review.md)。最终采用
独立 `market limit-review`，避免把客户端明确标注的非实时复盘数据混入实时
`limit-quality`。
