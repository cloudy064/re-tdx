# 研究行业与概念连板天梯纯 C++ 固定化

## 客户端语义证据

`T0002/cloud_dax/LBTT.sp` 的页面名为“连板天梯”。页面左侧行业单元明确绑定
`func_lbtt101`，对应 `T0002/cloud_cfg/func_lbtt101.cfg` 和远端资源
`list/func_lbtt101_1.jsn`。CFG 的客户端列为：

- `ztjs`：涨停家数—封板；
- `zbs`：涨停家数—炸板；
- `zrzt`：涨停家数—昨板；
- `lbs`：涨停家数—连板；
- `lbgd`：涨停家数—高度，即板块最高连板高度；
- `hzgd`：涨停家数—总高，即板块内涨停股板数之和；
- `lbjjl`：晋级率。

`$ZAF` 等宿主行情列不在资源 `colheader` 中，因此接口没有伪造当前涨幅。
客户端默认按 `hzgd` 排序。对当前所有同时具备昨板分母和晋级率的 207 行逐行
复核，`lbjjl = lbs / zrzt * 100`，四舍五入到两位，偏差为 0。

页面中的浏览器单元是逐股连板天梯；本阶段固定的是旁边有独立 CFG/JSN 证据的
板块截面，两者没有混成同一种数据。

## 当前实测

2026-08-06 实时下载返回 246 个板块、统计日 `20260806`：

- 45 个研究行业，封板 79、炸板 23、昨板 86、连板 22，最高 10 板；
- 201 个概念，封板 781、炸板 189、昨板 945、连板 204，最高 5 板；
- 合计封板 860、炸板 212；246/246 个代码均一对一映射到本地板块，名称和
  成分股入口全部可用；
- 按客户端默认总高降序，当前第一为 `880952 芯片`，总高 37、成分 911 只。

该资源是公开 7709 静态快照，不要求 L2 登录。它不是实时盘口，也不是五档或
逐笔数据；接口以统计日和来源健康显式表达这一边界。

## 原生接口

新增纯 C++ 命令：

```text
tdx-tool market limit-ladder --category all --sort total-height
tdx-tool market limit-ladder --category industry --activity consecutive --sort advancement-rate
tdx-tool market limit-ladder --code 880952
```

固定 API 为 `/api/v1/market/limit-ladder`，支持：

- `category=all|industry|concept`；
- `sort=date|sealed|broken|prior-limit|consecutive|max-height|total-height|advancement-rate`；
- `activity=all|sealed|broken|consecutive|advanced|inactive`；
- `code/q/offset/limit/refresh/cache_ttl_seconds/timeout_ms`。

响应 schema 为 `tdx-market-limit-ladder-native-v1`。每行包含规范化板块身份、
层级、本地成分数与 `/api/v1/blocks?q=<代码>` 入口、七个客户端统计字段、计算
晋级率与匹配标志、原始行；顶层保留分类聚合、来源、缓存及上游健康。

## 产物与验收

- `output/probes/func_lbtt101-current.jsn`：当前原始 GBK JSN；
- `output/probes/limit-ladder-all-current.json`；
- `output/probes/limit-ladder-industry.json`；
- `output/probes/limit-ladder-concept.json`；
- `output/probes/limit-ladder-880952.json`；
- `output/probes/limit-ladder-contracts-temp.json`；
- `output/probes/api-contracts-official-current.json`。

类型化覆盖由 237/616 提升到 238/616，通用独占由 379 降到 378，已下载通用
独占由 90 降到 89。新增 `tdx-limit-ladder-tests`，并为真实 API 增加排序负例、
精确来源、本地成分与公式零偏差契约；全量 CTest 53/53，通过正式 API 契约
43/43。

正式服务位于 `127.0.0.1:8765`，PID `21192`，功能目录 98 项，EXE SHA-256：
`D59CC91EBE212919A1417E06738B784BF7609B3F31D67F92F2633902E81D8672`。
运行时保持 `native_cpp=true`、`python_runtime=false`，临时端口 `8879` 已关闭。

## 下一批边界

`ldph101` 已在后续阶段并入市场情报与单票工作台，见
[安全亮点排行](2026-08-06-native-security-highlights.md)。下一批非 L2 高收益
缺口优先为 `bygtj102` 百元股历史统计、`tbgz108/tzcg104` 股东与机构持仓榜，
以及 `ygzl101` 关联证券事件。
`qszj101..105` 的 CFG 已明确写明 DDX 大单资金公式，属于 L2 衍生榜，按当前
“除 L2 外继续推进”的约束暂缓。
