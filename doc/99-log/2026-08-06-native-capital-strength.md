# 五周期 DDX 资金强势榜与跨周期共振

## 客户端页面与语义

本轮固定化 `QSZJ.sp`“个股资金流向”页面中的五个资金强势列表：

| 周期 | 页面名称 | 资源 |
| --- | --- | --- |
| 5日 | 5日资金强势 | `list/func_qszj101_1.jsn` |
| 10日 | 10日资金强势 | `list/func_qszj102_1.jsn` |
| 20日 | 20日资金强势 | `list/func_qszj103_1.jsn` |
| 30日 | 30日资金强势 | `list/func_qszj104_1.jsn` |
| 近3月 | 近3月资金强势 | `list/func_qszj105_1.jsn` |

五张 CFG 的标题一致声明：筛选所选周期 DDX 累计值排名前100的股票，DDX 为
“逐笔买入大单成交量减逐笔卖出大单成交量，再除以流通股本”。JSN 提供流通
股本、周期涨幅、总净流入、主力净流入、DDX 和统计日期。客户端现价、当日涨幅、
总市值和行业来自宿主 `syscol`，固定接口不伪造这些列。

DDX 是服务端预计算的大单成交量衍生结果，本轮读取的是公开 `709/1721` 静态榜，
不是原始 L2 逐笔，也不具备本地重算 DDX 所需的大单成交量输入。规范字段将 DDX
原值解释为占流通股本百分比点，不额外乘除；总/主力净流入原值为元。二者口径
不同，金额为负不能用于改写 DDX。

## 当前资源与数据

2026-08-06 当前五张表各100行，统计日期均为 `20260806`，源顺序全部按 DDX
严格降序，证券名称解析 500/500。远端大小和 MD5：

| 资源 | 字节 | MD5 |
| --- | ---: | --- |
| `func_qszj101_1` | 8,907 | `cf8d231e8701588adb7532d72b06b314` |
| `func_qszj102_1` | 8,917 | `0f248191f4700b02e316923366175236` |
| `func_qszj103_1` | 8,986 | `6feb73508dc9e4dcdf4df6293ae2bc19` |
| `func_qszj104_1` | 8,999 | `2c19fc98dc8a2ecb7efa667e362954ae` |
| `func_qszj105_1` | 9,026 | `0d27b756c07c1540ea94c7023faf56ac` |

五表合并后的真实统计：

- 500 行归并为 276 只股票；
- 128 只至少命中两个周期；
- 9 只同时命中全部五个周期；
- 5日榜 DDX 范围 2.6540%—22.2583%，100 行全部为正；
- 5日榜有 71 行总净流入为负，但主力净流入与 DDX 仍为正，证明不能混淆
  金额流向和大单成交量占流通股本口径。

## 纯 C++ 工具与固定 API

新增统一工具子命令：

```powershell
tdx-tool market capital-strength --view ranking --period 5d
tdx-tool market capital-strength --view confluence --min-periods 2
tdx-tool market capital-strength --view security --market sz --code 001232
tdx-tool market capital-strength --view catalog
```

`ranking` 可按 DDX、周期涨幅、总/主力净流入、流通股本或源排名排序；
`confluence` 保留每个周期的完整 observation，并输出命中周期数、平均/最低/最高
DDX 和最佳源排名；`security` 遍历五表反查，未命中返回 HTTP 200、
`availability=empty`，不再把正常空关系变成报错。

固定 API 为 `/api/v1/market/capital-strength`。共享服务缓存、三次上游重试、
陈旧缓存回退、精确资源元数据和原始行均保留，运行时不调用 Python。

## 网页、覆盖与验证

Svelte 数据中心新增“DDX资金强势”，可切换单周期榜和跨周期共振；个股工作台
“盘口交易”组新增“DDX强势”页签，展示该股票命中的 0—5 个周期。点击市场榜
股票会直接进入对应单票页签。

五张模板类型化后，JSN 覆盖由 245/616 提升为 250/616，通用独占由 371 降为
366；本地 338 个下载文件仍全部匹配模板，已下载但通用独占从 83 降为 78，
解析错误为 0。证据：

- `output/probes/capital-strength-5d-current.json`；
- `output/probes/capital-strength-confluence-current.json`；
- `output/probes/capital-strength-security-001232-current.json`；
- `output/probes/jsn-variants-current.json`；
- `output/probes/api-contracts-capital-strength-current.json`；
- `output/probes/api-contracts-official-current.json`。

新增规范化/跨周期聚合测试及两项线上契约，检查100/500行、日期、名称、DDX
降序、单位字段、原始行、共振排序、observation 数量、五条精确来源和上游健康。
全量 CTest 55/55，正式 API 契约 51/51。

正式服务为 `http://127.0.0.1:8765`，PID `15876`，功能目录 100 项，
`native_cpp=true`、`python_runtime=false`。部署哈希：

- `tdx-tool.exe`：`E790BA7525ED3C3E32F26DB9D077110376A4A82D0594EA96E44A23964BF0AB18`；
- `index.html`：`0CE4DFDF04B57CE15ED3F83EA6D19A79724CADBBE738AE183B08719E3DEE5E61`；
- `index-CMZFcMM3.js`：`114F660029BC2C3528F2F855C239C9F17AE6498CCB28E03B14817342990B2211`；
- `index-DFu_EnmM.css`：`7766D9E6E843A218A3489F8AD4AFCC7133D5E6B3D15934B172C4584FAADB1EDD`。

后续 `ygzl101/ygzl` 已在同日固定为 `market strong-stocks`；当前审计首位继续
转为 `zjtc103`“商股联动”和 `zxjx101`“公告精选”。
