# 场内基金表现、套利与 REITs 类型化

日期：2026-08-07

## 结论

本轮把五张已下载但只有通用入口的客户端表固定为纯 C++
`market exchange-funds`、固定 API、Svelte 数据中心页面和个股工作台页签：

| 业务 | 资源 | 原始行 | 规范行 | 语义 |
| --- | --- | ---: | ---: | --- |
| ETF 多周期表现 | `list/func_etfhq101.jsn` | 1,619 | 1,619 | 5/20/60 日、月初至今、年初至今与成交额 |
| 货币 ETF 套利 | `list/gxjty_etfjj103.jsn` | 27 | 27 | 理论净值、溢价、申赎与买卖套利年化 |
| 货币基金收益 | `list/gxjty_etfjj104.jsn` | 13 | 13 | 万份收益、七日年化与份额变化 |
| 已发行 REITs | `list/func_reits101_1.jsn` | 93 | 93 | 项目、发行、底层资产与财务信息 |
| 待发行 REITs | `list/func_reits102_1.jsn` | 1 | 0 | 上游一行全空占位，规范化为正常空关系 |

合计返回 1,752 条记录、1,723 只唯一证券；空占位不会被伪造成一条 REITs。

## 客户端证据与计算边界

字段和公式来自对应 CFG：

- ETF 表的五日、二十日、六十日、月初至今和年初至今表现分别用当前收盘价与
  `price6/21/61/1/11` 参考价计算；参考价与五日成交额均保留原始字段；
- 货币 ETF 理论净值为 `100 + MRSHJXR * QRNH / 365`；溢价为
  `(现价-理论净值)/理论净值*100`；两种套利年化严格复现客户端公式；
- `gxjty_etfjj104` 的来源市场号全部是特殊值 34，不能整体解释为沪市。工具
  先查证券目录，再按 159/519 前缀回退，最终恢复为深市 3 只、沪市 10 只，
  同时保留 `source_market_id=34`；
- REITs 发行份额已经是单份，项目收入和净利润已经是元，不再重复乘单位；
- 静态主表默认离线读取。本地/远端刷新与公开 L1 分层，只有显式 `--quotes` 或
  API `include_quotes=1` 才为筛选后的证券请求行情；行情失败只进入软错误。

`SH511620` 在线样本取得 100.008 元现价，理论净值 100.0107260274，溢价
-0.002725735%，买入赎回年化 0.248730102%，申购卖出年化 1.05625%，均与
客户端公式一致。

## 使用

```text
tdx-tool market exchange-funds --root C:\new_tdx --input-dir output/tdx-jsn
tdx-tool market exchange-funds --view cash-arbitrage --quotes
tdx-tool market exchange-funds --view reits-pipeline
tdx-tool market exchange-funds --market sh --code 511620 --quotes

GET /api/v1/market/exchange-funds?view=all&include_quotes=0&limit=5000
GET /api/v1/market/exchange-funds?view=cash-arbitrage&include_quotes=1
GET /api/v1/market/exchange-funds?market=sh&code=511620&include_quotes=1
GET /api/v1/market/exchange-funds?view=reits-pipeline
```

数据中心默认进入 ETF 表现，避免无意请求 1,619 只行情；货币 ETF 套利视图和
个股工作台才按需取公开 L1。所有响应都保留原始行、来源健康、缓存边界与空表状态。

## 验证

- 离线主表：1,619 + 27 + 13 + 93 + 0 = 1,752；唯一证券 1,723；
- 货币 ETF：27/27 取得公开 L1、行情错误 0；市场号 34 路由为深 3 / 沪 10；
- 待发行 REITs：原始 1 行、规范 0 行、空白 1 行、`availability=empty`；
- C++ 全量测试 71/71；Svelte 检查 0 错误、0 警告且生产构建通过；
- 临时服务和正式服务 `exchange-funds-live` 均为 1/1；完整 API 契约均为
  106/106；
- JSN 覆盖 354/616；384 个下载文件全部匹配、解析错误 0，已下载通用独占
  剩余 11 个。

证据文件：

- `output/probes/tdx-market-exchange-funds-offline-20260807.json`
- `output/probes/tdx-market-exchange-funds-cash-live-20260807.json`
- `output/probes/tdx-market-exchange-funds-yield-20260807.json`
- `output/probes/api-contract-exchange-funds-temp.json`
- `output/probes/api-contract-exchange-funds-formal.json`
- `output/probes/api-contracts-temp-current.json`
- `output/probes/api-contracts-formal-current.json`
- `output/probes/tdx-jsn-variants-current.json`

## 正式部署

- 服务：`http://127.0.0.1:8765`，PID `26256`；118 个功能、384 个 JSN 资源、
  11,100 个证券索引键；标准错误日志为空；
- `tdx-tool.exe`：35,524,669 字节，SHA256
  `B6079758F89857304B0F79AD756EA00F14CB62DD85BF195FA1E8A8CD1F1BBF3B`；
- Svelte：`index-C4bpf7T0.js` / `index-ChME7mtF.css`；HTML/JS/CSS SHA256 分别为
  `A368CF76F457035829FAC6F1F99BC92979C3005BD7FEAF9C074354B74C176F86`、
  `6F553C904C1EBBF682DC0D6EBB8BDA8035201AB62A529F67E5A2C1D10CEC2325`、
  `D598D4E87B0A5DB4B322C7A52F49EBA8256CC12DA950E55D8EF8CC27B5AC30A8`。
