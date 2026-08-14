# 公告精选、风险提示与单票公告前后表现

## 目标与客户端证据

本阶段继续处理公开、非 L2 的高收益 JSN 缺口 `zxjx101`。客户端页面和 CFG 给出
完整的主从关系：

```text
ZXJX.sp   “资讯精选”       -> func_zxjx101 -> list/func_zxjx101_1.jsn
SJQD4.sp  “公告精选”       -> func_zxjx101
                              选中股票 -> func_zxjx102 -> ggjx/<市场><代码>.jsn
JYFX.sp   “风险提示公告”   -> func_zxjx103 -> list/func_zxjx103_1.jsn
```

`func_zxjx102.cfg` 的 `single=1`、`refunit=21501,3` 证明动态文件由主表所选证券
驱动；资源名是市场号与六位代码直接拼接，例如深市海信家电为
`ggjx/0000921.jsn`、沪市江南新材为 `ggjx/1603124.jsn`。

## 最新在线数据

2026-08-07 通过 709/1721 重新下载并校验：

| 资源 | MD5 | 行数 | 去重股票 | 日期范围 |
|---|---|---:|---:|---|
| `func_zxjx101_1` | `7b226b51d8686b6f5b84f8116146321c` | 214 | 197 | 2026-08-04—08-07 |
| `func_zxjx103_1` | `c9a8b199418a099f38f9ab9ced241216` | 113 | 107 | 2026-08-05—08-07 |

公告精选中利好 166 条、利空 48 条，共 9 类公告；风险提示 113 条全部为利空，
其中 101 条为股票交易异常波动风险提示。当前主表只含深沪证券，没有北交所行，
但接口仍保留 `bj/2` 市场校验以兼容后续上游变化。

动态样本也已在线刷新：

- `ggjx/0000921.jsn`：海信家电 8 条，2026-07-09—08-07；
- `ggjx/1603124.jsn`：江南新材 19 条，2026-05-28—08-07；
- `ggjx/0000078.jsn`：海王生物 20 条；
- `ggjx/0000820.jsn`：神雾节能 10 条。

## 两个容易误判的字段边界

第一，原始 `title` 形如：

```text
2025年度A股权益分派实施公告TXT:http://static.cninfo.com.cn/.../1225461446.PDF
```

`TXT:` 后面是公告原文地址，不属于标题。规范模型拆成 `title` 和 `pdf_url`，
网页可以直接打开 PDF。

第二，主表和动态历史都使用原始字段 `zf1/zf2`，但客户端 CFG 的列名证明语义
不同：

- `func_zxjx101/103`：近 3 日、近 10 日涨幅；
- `func_zxjx102/ggjx`：公告前 3 日、公告后 3 日涨幅。

因此 C++ 分别输出 `recent_3d_return_pct/recent_10d_return_pct` 与
`pre_3d_return_pct/post_3d_return_pct`，不会按同名字段误合并。最近公告尚未形成
后 3 日区间时，上游空字符串保持 `null`，不解释为零收益。

客户端表格中的现价、换手率、成交金额、市值和行业属于宿主 `syscol`，静态 JSN
并不提供，本能力不伪造这些列。

## 纯 C++ 产品化

新增命令：

```powershell
tdx-tool market announcement-signals --view selected
tdx-tool market announcement-signals --view risks --direction bearish
tdx-tool market announcement-signals --view history --market sz --code 000921
tdx-tool market announcement-signals --view security --market sz --code 000921
tdx-tool market announcement-signals --view catalog
```

固定 API 为 `/api/v1/market/announcement-signals`，支持市场、代码、全文、方向、
公告类型、日期、排序、分页、刷新与缓存参数。`security` 同时合并该票在两个当前
主榜中的命中和可选动态历史；未进入当前榜不再抛出“selected security is absent”
类错误。动态历史缺失时保留结构化 `warnings` 并返回空关系。

Svelte 数据中心新增“公告精选”页：可切换公告精选/风险提示、筛选多空、搜索股票
或标题，点击行后展示原文和该票历史。个股工作台新增“公告信号”页签，分开展示
当前主榜和历史，PDF 标题可直接打开。宽表使用显式最小宽度，小窗口横向滚动，
避免中文标题被压成逐字竖排。

## 覆盖与验证

- `func_zxjx101_1`、`func_zxjx103_1`、`ggjx/*` 三类模板绑定类型化命令；
- JSN 覆盖由 259/616 提升为 262/616，通用独占降至 354；
- 339 个已下载文件全部匹配模板，已下载通用独占由 69 降至 66，解析错误为 0；
- CTest 58/58 通过；
- 新增公告精选、风险提示、单票历史三项固定在线契约；全量 API 契约 59/59；
- Svelte 检查 0 错误、0 警告，生产构建通过；
- 海信家电正式 API 实测返回 1 条当前公告、8 条历史、9 个 PDF，3 个来源且
  `warnings=0`；
- 正式服务为 `http://127.0.0.1:8765`，PID `31048`，功能目录 103 项；
- 可执行文件 SHA-256：
  `ECEFFC4A5374ADDC083306E49672F0926452EA3C6F0D628718658A9664257054`。

证据文件：

- `output/probes/announcement-selection-current/`；
- `output/probes/announcement-signals-cli.json`；
- `output/probes/announcement-contracts-live.json`；
- `output/probes/api-contracts-announcement-signals-current.json`；
- `output/probes/jsn-variants-announcement-signals-current.json`；
- `output/probes/announcement-signals-ui-current.png`。

## 下一步

最新覆盖审计的高分公开缺口变为 `func_gznhg100_1`（国债逆回购状态、可用资金、
手续费和资金解冻/可取日）以及 `func_gqgg101..104`（国企改革分组与证券集合）。
下一阶段应先验证这些表是否仍实时更新，并与现有市场行情和板块关系去重，再决定
是增加独立业务能力还是并入已有证券/板块接口。
