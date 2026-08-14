# 300 只安全亮点排行、单票反查与网页接入

## 语义审计

`T0002/cloud_cfg/func_ldph101.cfg` 的 `style=list` 单元明确绑定
`list/func_ldph101_1.jsn`，客户端默认按 `lds` 排序。资源只有七个真实字段：

- `$SC/$ZQDM`：市场与证券代码；
- `lds`：亮点数；
- `ldlx`：主亮点类型；
- `ldxq`：主亮点详情；
- `zxfs`：安全分；
- `ldfz`：亮点分值。

CFG 中的名称、涨幅、现价、成交额、总市值和所属行业均为宿主行情/引用列，不在
JSN `colheader` 中；接口没有伪造这些字段。`SLC.sp` 的扫雷避险页面引用该 CFG，
但资源内容是正向亮点榜，不是风险黑名单，因此它作为 `market intelligence` 的
独立 `highlights` 视图，与现有 `risks` 风险观察/潜在爆雷并列。

## 当前数据

2026-08-06 远端资源返回 300 行：

- 深市 136、沪市 162、北交所 2，300/300 名称解析；
- 亮点数范围 5–16；安全分范围 47–100，平均 91.50；亮点分范围 33–84；
- 12 类主亮点：净利率较高 81、利润多年连增 57、高ROE 49、高质量发展 31、
  盈利质量高 30、300ESG 18、营收飙升 18、营收多年连增 11，另有毛利率较高、
  低负债率、高股息和摘帽；
- 默认亮点数降序第一为 `SH603986 兆易创新`，亮点数 16，主亮点为净利率较高，
  详情“净利率高达35.16%”。

这是公开 7709 静态榜，不要求 L2 会话；响应保留来源、尝试次数、陈旧状态、缓存
年龄和原始行。资源没有统计日期，因此没有猜测生成日期。

## 原生能力

扩展已有纯 C++ 命令：

```text
tdx-tool market intelligence --view highlights --sort highlight-count
tdx-tool market intelligence --view highlights --type 高ROE --sort safety-score
tdx-tool market intelligence --view security --market sh --code 600519
```

固定 API `/api/v1/market/intelligence` 新增：

- `view=highlights`；
- `type/q/offset/limit` 过滤和分页；
- `sort=highlight-count|safety-score|highlight-score|code`、`order=asc|desc`；
- `view=security` 同时查询关注、风险、安全亮点和事件，单票未进前 300 时返回
  `highlights=[]`，不报错。

顶层新增 `highlight_summary`，汇总证券、名称解析、市场、十二类亮点及安全分区间；
`sources/upstream_health/cache.upstream` 统一使用共享 JSN 健康结构。

## 网页

市场情报页新增“安全亮点”视图与六列排行，可点击进入个股；个股工作台“关注与
事件”页签新增独立安全亮点面板。原有关注、风险、事件和关系图布局未重写。
Svelte 生产构建通过，当前入口资源为 `index-BW6W3Oi3.js`。

## 验收与部署

- 原始证据：`output/probes/func_ldph101-current.jsn`；
- 全市场：`output/probes/intelligence-highlights-all-current.json`；
- 高ROE：`output/probes/intelligence-highlights-high-roe.json`；
- 单票贵州茅台：`output/probes/intelligence-security-600519.json`；
- 临时契约：`output/probes/intelligence-highlights-contracts-temp.json`；
- 正式契约：`output/probes/api-contracts-official-current.json`。

类型化覆盖由 238/616 提升到 239/616，通用独占由 378 降到 377，已下载通用
独占由 89 降到 88。`tdx-intelligence-tests` 增加字段、深沪京身份、原始行、排序
与非法排序测试；全量 CTest 53/53。新增全市场排序和贵州茅台单票关联两项真实
契约，正式巡检 45/45。

正式服务 PID `4624`，功能目录 98 项，EXE SHA-256：
`46BDCCA0C7D081536BDA11942F369224731D980328CF62EB46DDE42899467DE7`；
`native_cpp=true`、`python_runtime=false`，临时端口 `8879` 已关闭。

## 后续

`bygtj102/qyjlb102/bygtj1/bygtj3` 已继续闭合为
[百元股与千亿市值](2026-08-06-native-threshold-stocks.md)。剩余高收益缺口优先
考虑 `tbgz108/tzcg104` 股东与机构持仓榜、`ygzl101` 关联证券事件；五张 `qszj`
为 DDX 大单资金衍生榜，继续暂缓。
