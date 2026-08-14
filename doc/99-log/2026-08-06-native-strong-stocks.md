# 强势股生命周期、逐日涨停原因与市场温度

## 客户端页面与字段语义

`T0002/cloud_dax/QSGFX.sp` 的页面名是“强势股分析”。其中：

- `func_ygzl101` 是“历史强势股”主表；
- `func_ygzl102` 是随主表选择变化的“强势股明细”；
- 分时、K 线和龙虎榜网页属于客户端联动展示，不是两张 JSN 自带字段。

`func_ygzl101.cfg` 绑定 `list/func_ygzl101_1.jsn`，真实字段为：

| 字段 | 语义 |
| --- | --- |
| `$ZQDM1/$SC1` | 股票代码与市场 |
| `sj1/sj2` | 强势区间开始、结束日 |
| `jtjb` | “N天M板”区间统计 |
| `zf1` | 区间个股涨幅百分比 |
| `zf2` | 同区间上证指数涨幅百分比 |
| `$ZQDM` | 18 位区间 ID：股票代码 + 两个六位日期 |

`func_ygzl102.cfg` 绑定动态资源 `ygzl/<区间ID>.jsn`，逐日字段为个股涨幅、
成交额、涨停原因、沪深京涨停数、炸板数、跌停数和上证涨幅。客户端封板率公式
明确为 `ztjs/(ztjs+zbjs)*100`。名称、现价、当日涨幅和行业来自宿主 `syscol`；
固定接口只从本地证券目录补名称，不伪造行情或行业字段。

## 旧结论纠正与在线证据

早期记录曾根据当时的局部样本写成“613 条且停在 2023 年”。2026-08-06 重新
通过 7709 `709/1721` 在线下载后，该结论已被证伪：当前主表为 615 行，覆盖
2023-08-08 至 2026-08-06，显然仍在滚动维护，而不是停更历史表。

当前主表统计：

- 615 个区间，562 只去重股票，49 只曾多次入选；
- 深市 319、沪市 237、北交所 59 行；
- 证券名称解析 615/615；
- 614 行区间收益已结算，最新 `SZ002827` 一行收益为空，规范化为 `null`；
- 最新区间 ID 为 `002827260729260806`，区间统计为“6天4板”。

远端资源证据：

| 资源 | 字节 | MD5 |
| --- | ---: | --- |
| `list/func_ygzl101_1.jsn` | 51,364 | `81663c34d444f15191dab4f0ad550014` |
| `ygzl/002827260729260806.jsn` | 3,195 | `e7c12be86bc2aa8d01722ace4d58eb67` |

最新高争民爆区间的详情为 6/6 个交易日完整命中，6 天都有涨停原因；成交额
合计 5,610,616,640 元，全市场涨停/炸板/跌停累计 570/267/96，逐日封板率
平均 71.57%。这些数值是区间研究数据，不等于实时行情或原始 L2 逐笔。

## 纯 C++ 工具与 API

新增统一子命令：

```powershell
tdx-tool market strong-stocks --view intervals --min-limit-up-days 5
tdx-tool market strong-stocks --view security --market sz --code 002272
tdx-tool market strong-stocks --view detail --interval-id 002827260729260806
tdx-tool market strong-stocks --view catalog
```

主表支持市场、股票、区间 ID、日期重叠、最少交易日、最少连板数、文本、分页
和九种排序；单票视图保留同一股票的多次生命周期。详情视图先在当前主表严格
校验区间 ID，再读取动态资源；市场/代码若同时提供还会与区间身份交叉检查。
返回结构保留原始行、精确资源、尝试次数、陈旧缓存状态和单位说明。

固定 API 为 `/api/v1/market/strong-stocks`。运行时为纯 C++，不调用或转发
Python；数据来自公开静态资源，不需要 L2 账号。

## Svelte 接入

数据中心新增“强势股生命周期”页面，以主从布局展示历史区间和逐日原因；可按
市场、最少连板数和股票检索，点击区间加载明细，也可进入对应个股工作台。

个股工作台“盘口交易”组新增“强势生命周期”页签，展示一只股票所有历史入选
区间，并按区间加载每日原因、成交额和市场温度。未命中返回 HTTP 200 和正常
空关系，不显示为接口错误。

## 覆盖、测试与部署

主表与动态模板类型化后：

- JSN 覆盖由 250/616 提升为 252/616；
- 通用独占由 366 降为 364；
- 当前 339 个下载文件全部匹配模板；
- 已下载但通用独占由 78 降为 76；
- 解析错误保持 0。

证据文件：

- `output/probes/strong-stocks-intervals-current.json`；
- `output/probes/strong-stocks-detail-002827-current.json`；
- `output/probes/strong-stocks-security-002272-current.json`；
- `output/probes/jsn-variants-current.json`；
- `output/probes/api-contracts-strong-stocks-current.json`；
- `output/probes/api-contracts-official-current.json`。

新增规范化、封板率公式、空收益、重复区间和排序单元测试，以及主表/详情两项
在线契约。全量 CTest 56/56，正式 API 契约 53/53，Svelte 生产构建通过。

正式服务为 `http://127.0.0.1:8765`，PID `19996`，功能目录 101 项，
`native_cpp=true`、`python_runtime=false`。部署哈希：

- `tdx-tool.exe`：`8AEAF16226F0B4569FC0554F2B08399A61E65B0C63AAE75EC6F515B4E916E257`；
- `index.html`：`F74D20547ADA843F55B19B2E8029DAAC3745E90CF52F6CCC1A6C5905331202BD`；
- `index-BOqJQAO2.js`：`825508B56779559B844FFC54966DB1670AFA4E714347D65D4223DFE165B58142`；
- `index-DLhsGfib.css`：`CF69B871D0162A6316473F769824C58E47FF154931806758449C656FA7F3F02E`。

下一批高收益公开缺口是 `zjtc103`“商股联动”221 行和 `zxjx101`“公告精选”
216 行；两者均已下载且关联股票，不依赖 L2。
