# 并购重组、三板转板与公式上下文网页闭环

> 日期：2026-08-08。范围：公开 7709 JSN、客户端 CFG/SP、纯 C++
> CLI/API、Svelte 数据中心和个股工作台；不使用 Python、登录账号或 Level2。

## 候选复核与取舍

先对客户端遗留页面做了多源复核。`CJRL102/103/104`、`JQGZ105-109`、
`FJJJ101-106`、`FJTL101-102` 共 16 个资源，在三个已验证 7709 主站
`81.71.32.47`、`110.41.147.114`、`116.205.183.150` 以及 `bi/bib/bi_diy`
三个命名空间的九种组合中均为零长度。它们记录为“客户端遗留空资源”，没有
仅凭 CFG 页面名制造接口。

随后只探测当时 116 个通用候选模板：81 个远端非空、35 个缺失。本轮从中选择
业务边界最完整的两组，不把 81 个资源一次性下载成通用兜底：

- `QXFA105/106/110/111`：重大资产重组预案、审核、实施与普通并购预案；
- `XSBTJ101/102 + YZB101`：三板拟转 A 股、自律监管和已转板。

## 下载、MD5 与规模

下载由 `tdx-tool jsn download` 的纯 C++ `709→1721` 路径完成，并按远端 MD5
校验后原子替换：

| 资源 | 字节 | MD5 | 行数 |
| --- | ---: | --- | ---: |
| `list/func_qxfa105_1.jsn` | 191,345 | `70d67d4b955682406f0bf7dfc46a7f5e` | 311 |
| `list/func_qxfa106_1.jsn` | 6,167 | `1dfc18a26fb302e1b361a4f5d918744a` | 11 |
| `list/func_qxfa110_1.jsn` | 74,563 | `4b1728d52cd6d6a0535b523b23eba0f4` | 142 |
| `list/func_qxfa111_1.jsn` | 1,459,554 | `2e1ab15436d371f50f420ea87aef94fd` | 2,501 |
| `list/func_xsbtj101_1.jsn` | 151,939 | `c88432902bc30842350f31010bd74d1c` | 206 |
| `list/func_xsbtj102_1.jsn` | 774,437 | `bbd03ca1b532bbaee844cdbafdde45be` | 683 |
| `list/func_yzb101_1.jsn` | 3,637 | `13cbd8ff5448aae2131529b0eb18608b` | 3 |

七表共 3,857 行。加上原有吸收合并、B 转 H 和市值管理预警，统一特殊事项接口
当前为 4,036 条。

## 业务语义

并购四表共享同一字段结构，但阶段不能合并丢失：标的获得方 `bdhdf`、出让方
`bdcrf`、类型 `bdlx`、涉及额 `sjje`、进度 `xmjd`、行业、公告日和交易简介均
保留。`sjje` 已由真实样本确认直接为元，例如中芯国际 40,600,910,000 元；接口
同时输出 `/1e8` 的亿元投影，契约逐行对账。

三板拟转 A 股保留报告期、拟上市板块、净资产、当期/上期净利润、营业收入、
当前进度、辅导机构和详情。真实样本确认财务字段直接为元。自律监管保留监管原因、
案情、措施、行业和公告日；已转板保留受理、注册、上市三日以及转板前后上市地。
原始市场号 44 只在规范化层映射为 `bj`，不会误报为沪深市场。
同证券、同日、同交易类型仍可能存在多条原始并购记录，因此 `event_id` 纳入
来源内稳定行号；4,036 条记录的事件 ID 已验证 4,036/4,036 唯一。

`market special-situations` 新增：

- `corporate-actions`；
- `neeq-transfers`；
- `neeq-regulation`；
- `legacy`，供原三类定价/预警契约独立巡检。

Svelte 数据中心和个股工作台已展示新字段。全目录和新增视图默认不加载行情，
吸收合并、B 转 H、市值预警及单票查询才按需请求公开 L1，避免 4,036 条目录造成
不必要的批量行情延迟。

## 公式解释器边界与网页闭环

最新公式审计仍为 379/379 源码可恢复、语法支持和数值信号安全。剩余 20 条不是
普通解释器语法缺口：14 条需要调用方合法持有的 L2 逐笔/大单序列，6 条需要
券商宿主私有 `SIGNALS_QS`，共 36 个精确绑定键。CLI 已支持 `--context-file` 和
`formulas context-template`；本轮新增只读 API：

```text
GET /api/v1/formulas/context-template?formula=ZJLX&stamp=2026-08-08%7C15%3A00
```

Svelte 公式详情对显式上下文公式显示模板生成器。返回值只包含精确键和 `null`
占位，不下载、不推导、不填零，也不绕过券商或 L2 权限。`ZJLX` 样本精确生成
8 个 `L2_AMO#i#j` 键，并由固定契约校验。

## 覆盖、验证与部署

- JSN：509/618 个模板已类型化，528 个本地文件全部匹配，解析错误 0，已下载
  通用独占 0；
- CTest：96/96；
- Svelte：`check` 0 错误/0 警告，生产构建通过；
- 隔离服务重点契约 3/3、full 143/143；
- 正式服务重点契约 3/3、full 143/143；
- 正式服务：`127.0.0.1:8765`，PID `44988`，528 份 JSN，
  `native_cpp=true`、`python_runtime=false`；
- 发行版 SHA-256：
  `7C08200EE06DC757417E834D80CAB0070703D45A60841565923F600D5D13BEC5`。

证据文件：

- `output/probes/special-situations-expanded-all.json`；
- `output/tdx-jsn-variants-current.json`；
- `output/probes/api-contract-expanded-temp.json`；
- `output/api-contracts-corporate-formula-temp-full.json`；
- `output/api-contracts-corporate-formula-formal-selected.json`；
- `output/api-contracts-corporate-formula-formal-full.json`。

下一批优先从已探测非空资源中选择独立且可闭合的业务域。候选包括 ETF 份额与
净流入、财务经营筛选、主题目录等；债券分类资源虽然体量大，但要先确认是否与
现有证券目录/可转债/场内基金能力重复，再决定是否下载。
