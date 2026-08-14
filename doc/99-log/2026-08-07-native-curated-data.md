# 客户端八类精选数据类型化

日期：2026-08-07

## 结论

最后 8 个已经下载、但仍只有通用 JSN 入口的模板已经全部固定为纯 C++
`market curated-data`，并同步接入固定 API、Svelte 数据中心和个股工作台。
这一批合计规范化 1,934 行、1,754 只去重证券；至此 387 个本地文件全部有
业务类型化入口，下载目录中的通用独占模板从 8 个降为 0。

| 业务视图 | 资源 | 规范行 | 客户端语义 |
| --- | --- | ---: | --- |
| `media-entertainment` | `list/func_cmyl101_1.jsn` | 73 | 传媒娱乐 |
| `low-valuation-smallcap` | `list/func_dgzxz101.jsn` | 49 | 低估值袖珍股 |
| `dividend-fundraising` | `list/func_fhmz101_1.jsn` | 500 | 分红募资统计 |
| `buyback-statistics` | `list/func_gfhgtj101_1.jsn` | 14 | 拟回购月度统计 |
| `high-dividend` | `list/func_gfhl101_1.jsn` | 3 | 高分红 |
| `hk-performance` | `list/func_ggthq101_1.jsn` | 651 | 港股多周期表现 |
| `high-refinancing-lending` | `list/func_ggzrt101_1.jsn` | 300 | 转融券余额高 |
| `below-book-soe` | `list/func_gqpjg101_1.jsn` | 344 | 破净国企股 |

## 语义证据与数据边界

- 八张 `T0002/cloud_cfg/func_*.cfg` 给出列名、显示单位和客户端计算公式；
  `CMYL.sp`、`FXGZ.sp`、`FHMZ.sp`、`JQGZ.sp` 与 `TZJH.sp` 又确认了入口名称和
  所属功能。比如 `FXGZ.sp` 将 `func_ggzrt101` 标为“转融券余额高”，
  `JQGZ.sp` 将两张筛选表标为“低估值袖珍股”和“破净国企股”，没有凭文件名猜义。
- 分红募资表的累计分红/募资按亿元展示，分红募资比和股息率按客户端公式复算；
  低估值袖珍股的 PEG 使用 `PE / 预测 EPS 增长`，预测增长字段保留百分比口径。
- 拟回购统计把万股转换为股、亿元转换为元，再复算回购股数/总股本和回购市值/
  总市值比例；高分红表保留每股分红的元口径和股息率百分比。
- 转融券表把余额万元转换为元、余量万股转换为股，余额占比保持百分比点。
  当前 300 行中只有 3 行余额非空，日期范围为 2013-07-25 至 2024-09-30；接口
  明确报告 `refinancing_lending_latest_date=20240930`，不会把它包装成实时余额榜。
- 港股表现表包含市场号 31 和 49。最初只接受市场 31 会丢失 31 行，现已把
  31/48/49 都作为合法港股身份保留；本地 `tdxhkag.cfg` 能解析的名称优先复用，
  其余记录仍保留来源市场、代码和原始名称。5/20/60 日、月初至今和年初至今
  表现保持独立字段。
- 破净国企股同时保留 PB、实际控制人及其属性，不把“国企”和“破净”拆成两个
  无法追溯来源的推断条件。

## 使用

```text
tdx-tool market curated-data --root C:\new_tdx --input-dir output\tdx-jsn
tdx-tool market curated-data --view high-dividend
tdx-tool market curated-data --view hk-performance --query 腾讯
tdx-tool market curated-data --market sz --code 000001
tdx-tool market curated-data --view below-book-soe --refresh

GET /api/v1/market/curated-data?view=all&limit=5000
GET /api/v1/market/curated-data?view=high-dividend
GET /api/v1/market/curated-data?view=hk-performance&q=腾讯
GET /api/v1/market/curated-data?market=sz&code=000001
```

网页入口为 `/data/curated-data`。数据中心可在八类视图间切换、搜索和按证券过滤；
个股工作台的“精选数据”页签直接查询当前证券关联，不需要浏览器读取原始 JSN。

## 验证

- 离线全量返回 1,934 行：73 + 49 + 500 + 14 + 3 + 651 + 300 + 344；
- 证券去重后为 1,754 只；港股表现 651 行，其中市场号 49 为 31 行；
- C++ 全量测试 73/73；Svelte 检查 0 错误、0 警告，生产构建通过；
- 临时和正式服务的 `curated-data-live` 都为 1/1；完整 API 契约均为 108/108；
- JSN 类型化覆盖 368/616；387/387 个下载文件匹配，解析错误 0，下载目录中的
  通用独占模板为 0。

证据文件：

- `output/probes/tdx-market-curated-data-offline-20260807.json`
- `output/probes/tdx-market-curated-data-security-600519-20260807.json`
- `output/probes/api-contract-curated-data-temp.json`
- `output/probes/api-contract-curated-data-formal.json`
- `output/probes/api-contracts-temp-current.json`
- `output/probes/api-contracts-formal-current.json`
- `output/probes/tdx-jsn-variants-current.json`
- `output/probes/tdx-jsn-catalog-after-shareholder-refresh.json`

## 正式部署

- 服务：`http://127.0.0.1:8765`，PID `29816`；119 个功能、387 个 JSN 资源、
  11,100 个证券索引键；`/data/curated-data` 返回 HTTP 200，标准错误日志为空；
- `tdx-tool.exe`：35,733,531 字节，SHA256
  `546FB8AF3C3AE36054B5D97E7F6A1CC804631210B905726DF4AD9C987FD7BCBB`；
- Svelte：`index-BZcw4L88.js` 914,657 字节，`index-Cp2syUVx.css` 80,504 字节；
  HTML/JS/CSS SHA256 分别为
  `087F73C1B50B759EAF0730CDC04AB8CE0E755C6384D4BF070AAF4BA1F6F60DC4`、
  `C01651C228EF7859443A0D39F4A46D36D9F073F8DD1B8A28AAE8867A311F12DC`、
  `35B3F687AE882868CEC3671714FFC42BC290D97BC26D7413D9CD770AF620A4C8`。
