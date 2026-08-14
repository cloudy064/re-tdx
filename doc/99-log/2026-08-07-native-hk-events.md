# 港股四类事件资源类型化

日期：2026-08-07

## 结论

本轮从 274 个未类型化静态模板中筛选 25 个高价值候选做有界下载探测，23 个
返回真实非空数据。优先将同属 `GGRL.sp`、字段稳定且能形成完整业务闭环的
`func_ggrl102/103/104/105` 固定为纯 C++ `market hk-events`、固定 API 和
Svelte“港股事件”页。

当前四族共 5,674 条：

| 类型 | 资源 | 行数 | 业务语义 |
| --- | --- | ---: | --- |
| 分红派息 | `list/func_ggrl102_1.jsn` | 1,355 | 公告、财政年度、派息、除净及登记期 |
| 权益披露 | `list/func_ggrl103_1.jsn` | 1,554 | 投资者、变动股数、变动后持股/比例、好淡仓和披露原因 |
| 沽空统计 | `list/func_ggrl104_1.jsn` | 2,565 | 每日沽空股数、金额、成交额、占比和统计时点 |
| 上市申请 | `list/func_ggrl105_1.jsn` | 200 | 申报进度、板块、保荐人、上年财务、控股股东和主营业务 |

去重后覆盖 1,636 个证券 ID。接口不把上市申请伪造成已经分配代码的证券。

## 客户端配置证据与单位

四张表的 `cloud_cfg/func_ggrl10*.cfg` 给出了明确列名和客户端计算式：

- `ggrl103` 的 `bdgs/bdhgs` 是万股，接口同时输出原始 `*_10k` 和乘 10,000
  后的单股值；`bdhcgl` 是百分比点。
- `ggrl104` 的 `gksl/gkje/cjje` 分别是万股、万元、万元；接口换算为单股和
  币种单位，并按客户端 `gkje/cjje*100` 复算 `short_turnover_pct`。
- `ggrl105` 的 `sr/lr` 是千币种单位，客户端列公式为 `sr*1000/lr*1000`；
  接口保留千单位和值换算结果，币种 `bz` 原样返回，不跨币种聚合。
- 所有记录保留 `event_id`、`source_resource` 和 `raw`，日期按业务主日期倒序。

最初只接受港股市场号 31/48 时，沽空表仅得到 2,252 条。字段审计确认遗漏的
313 条全部是市场号 49、五位港股基金/产品代码；加入 49 后，原表 2,565 条无损
对账。该修正已进入单元测试与在线契约。

## CLI、API 与界面

```text
tdx-tool market hk-events --input-dir output/tdx-jsn --view all
tdx-tool market hk-events --view short-selling --code 00700 --from 20260801

GET /api/v1/market/hk-events?view=all&limit=20
GET /api/v1/market/hk-events?view=dividends&q=腾讯
GET /api/v1/market/hk-events?view=applications
```

CLI 默认直接读取 `output/tdx-jsn`，`--refresh` 才显式访问公开上游；服务复用
`--jsn-root`。页面支持全部、分红派息、权益披露、沽空统计和上市申请五个视图，
提供全文检索、动态列、本地排序和右栏完整详情。港股工作台目前没有稳定的统一
证券路由，因此本轮只进入全市场数据中心，没有伪接 A 股工作台。

## 验证

- C++：69/69 测试通过；新增四类规范化、万/千单位换算及沽空占比测试。
- Svelte：`svelte-check` 0 错误、0 警告，生产构建通过。
- `hk-events-live` 校验 5,600+ 总量、四族汇总对账、日期倒序、换算公式和四个
  精确来源；临时服务与正式服务均为 1/1。
- 全量固定 API 契约为 104/104。
- JSN 覆盖为 346/616；384 个已下载文件全部匹配、解析错误 0，尚有 19 个
  本轮新下载的高价值模板等待业务类型化。

证据文件：

- `output/probes/tdx-market-hk-events-native-20260807.json`
- `output/probes/api-contract-hk-events-20260807.json`
- `output/probes/api-contract-hk-events-formal-20260807.json`
- `output/probes/api-contract-full-after-hk-events-20260807.json`
- `output/probes/jsn-variants-after-hk-events-20260807.json`

## 正式部署

- 服务：`http://127.0.0.1:8765`，PID `29528`；116 个功能、384 个 JSN 资源、
  11,100 个证券索引键。
- `tdx-tool.exe`：35,191,024 字节，SHA256
  `7612434C248CCF2552FFBF6962E28E744516A1A6028308D32ADD0E48784F0FF0`。
- Svelte：`index-n7uR301F.js` / `index-DR2A-DPF.css`；HTML/JS/CSS SHA256 分别为
  `3B0CFA07DF098C6AD7B91876992D2BD9F0E2A701C8CAFF26D7D272521E5A1539`、
  `7086DC775AB0665CE835D9994668C5E54DDF40B600DBFEB9C96F83AC07444624`、
  `937C19DDB48B1F1725995137682845FA56F427BB538F7A02E57AB34B435C8355`。
