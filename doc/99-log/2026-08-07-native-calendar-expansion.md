# 重大事项、新股与板块资讯日历扩展

日期：2026-08-07

## 结论

本轮没有新增一个语义重叠的命令，而是把既有纯 C++ `market calendar`
从 4 个来源扩展为 10 个来源，并同步接入固定 API、Svelte 数据中心和个股工作台。
默认从已下载 JSN 离线读取，只有显式刷新才访问远端。

| 业务 | 资源 | 规范行 | 主要时间范围/快照 |
| --- | --- | ---: | --- |
| 财经日历 | `list/func_cjrl101_1.jsn` | 159 | 来源当前窗口 |
| 会议日历 | `list/func_cjrl105_1.jsn` | 8 | 来源当前窗口 |
| 公司事件 | `list/func_gsrl206_1.jsn` | 2,363 | 来源当前窗口 |
| 上市安排 | `list/func_ggrl101_1.jsn` | 167 | 来源当前窗口 |
| 沪深重大事项 | `list/func_dsjtx101_1.jsn` | 279 | 2026-08-06 至 2026-08-07 |
| 新股公告 | `list/func_xgrl101_1.jsn` | 100 | 2026-07-30 至 2026-08-06 |
| 近期新股 | `list/func_xgrl102_1.jsn` | 146 | 上市日 2025-08-08 至 2026-08-07 |
| 科创板资讯 | `list/func_xgrl103_1.jsn` | 100 | 来源当前窗口 |
| 创业板资讯 | `list/func_xgrl104_1.jsn` | 100 | 来源当前窗口 |
| 新三板资讯 | `list/func_xgrl105_1.jsn` | 100 | 来源当前窗口 |

合计规范化 3,522 条记录。其中板块资讯 300 条，278 条可关联到本地证券或由
标题中的六位代码推断出的证券；单证券过滤会同时检查主证券和关联证券。

## 字段、公式和单位边界

- 重大事项表中 `$SC1/$ZQDM1` 才是证券市场和代码；`$ZQDM` 是事件序号，不能
  当作股票代码；当前 279 行对应 278 只唯一证券。
- 新股公告的 `title` 内嵌 `TXT:` 正文和 PDF 链接。工具把正文转换为安全纯文本，
  单独提取来源链接，不把上游 HTML 直接交给浏览器执行。
- 近期新股的 `FXJ` 为发行价；`mjzj/cmzj` 已经是元，不再放大一万倍；负的
  `cmzj` 原样保留。`gps` 为每个中签号对应的 100/500 股，`ZQL` 为中签率百分比，
  `ZF1/ZF2/ZF3` 分别为上市至今、上市后 5 日和 10 日涨跌幅百分比，
  `HPE/FXPE/PE` 分别保留对应市盈率字段。
- 近期新股来源市场值 0/1/2 分别路由深/沪/北；对尚未进入本地证券目录的新股，
  工具保留推断市场并设置 `name_resolved=false`，不会错误丢弃。
- 科创板、创业板、新三板资讯根据资源边界和标题代码双重约束关联证券：样本代码
  分别集中于 68、30，以及 83/87/43/92 前缀。名称匹配也只在相应板块证券集合内
  进行，降低同名误关联。
- 响应中的列表正文使用摘要，完整清洗正文、原始行、关联证券和来源链接保留在详情，
  避免 100 条公告把主表响应无谓放大。

## 使用

```text
tdx-tool market calendar --root C:\new_tdx --input-dir output/tdx-jsn
tdx-tool market calendar --view major-events --market sh --code 688783
tdx-tool market calendar --view recent-ipos
tdx-tool market calendar --view ipo-announcements --query 绿控传动
tdx-tool market calendar --view board-news --refresh

GET /api/v1/market/calendar?view=all&limit=5000
GET /api/v1/market/calendar?view=major-events&market=sh&code=688783
GET /api/v1/market/calendar?view=recent-ipos
GET /api/v1/market/calendar?view=ipo-announcements&q=绿控传动
GET /api/v1/market/calendar?view=board-news
```

页面提供宏观、会议、公司、重大事项、上市安排、近期新股、新股公告和板块资讯
8 个用户视图。个股工作台同时展示与当前证券有关的重大事项、近期新股和板块资讯。

## 验证

- 离线主表返回 3,522 条：159 + 8 + 2,363 + 167 + 279 + 100 + 146 + 300；
- `SH688783` 返回 6 条关联记录：重大事项 1、近期新股 1、科创板资讯 4；
- 板块资讯返回 300 条，其中科创板 91/100、创业板 97/100、新三板 90/100
  成功关联证券；
- 新股公告查询“绿控传动”返回 17 条，并能提取 PDF 来源链接；
- C++ 全量测试 72/72；Svelte 检查 0 错误、0 警告，生产构建通过；
- 临时服务与正式服务的 `calendar-expanded-live` 均为 1/1；完整 API 契约均为
  107/107；
- JSN 类型化覆盖 360/616；387 个下载文件全部匹配、解析错误 0，已下载但仅有
  通用入口的模板剩余 8 个。

证据文件：

- `output/probes/tdx-market-calendar-expanded-offline-20260807.json`
- `output/probes/tdx-market-calendar-expanded-security-20260807.json`
- `output/probes/api-contract-calendar-expanded-temp.json`
- `output/probes/api-contract-calendar-expanded-formal.json`
- `output/probes/api-contracts-temp-current.json`
- `output/probes/api-contracts-formal-current.json`
- `output/probes/tdx-jsn-variants-current.json`

## 正式部署

- 服务：`http://127.0.0.1:8765`，PID `37328`；118 个功能、387 个 JSN 资源、
  11,100 个证券索引键；标准错误日志为空；
- `tdx-tool.exe`：35,582,447 字节，SHA256
  `9EDB9DA4694C2132848039DF4459BE75B24E8F8F3A1D2827DA78276094319E5C`；
- Svelte：`index-DlPpcsN0.js` / `index-BPE4OqkV.css`；HTML/JS/CSS SHA256 分别为
  `E2E54ED06529F878084BAE633558ADAA709BB6E4A3C07AD8E855C003A04ECBF4`、
  `3675E54B827647A065238DB8CCEECCDB4F9C915638984CEC2ACF25D3C4B52DA2`、
  `62C9057F7F079532048D62B354616B4609699151C6DB64E9961647A35BB55BB0`。
