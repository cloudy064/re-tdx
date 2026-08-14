# 合并换股与市值管理预警类型化

日期：2026-08-07

## 结论

本轮把三个已下载但只有通用入口的客户端功能固定为纯 C++
`market special-situations`、固定 API、Svelte 数据中心页面和个股工作台页签：

| 业务 | 资源 | 行数 | 语义 |
| --- | --- | ---: | --- |
| 吸收合并 | `list/func_agtl101_1.jsn` | 4 | 吸并双方、换股价、现金选择权与进度 |
| B 转 H | `list/func_agtl102_1.jsn` | 3 | B 股、关联 A 股、币种、现金选择权与进度 |
| 市值管理预警 | `list/func_cdgc101_1.jsn` | 172 | 主要指数成分股的 20 日及一年高点回撤阈值 |

合计 179 条。市值预警覆盖 151 只证券，20 日单独触发 90 条、一年单独触发
62 条、同时触发 20 条，三类之和严格等于 172。

## 客户端证据与计算边界

字段和公式直接来自 `T0002/cloud_cfg/func_agtl101.cfg`、
`func_agtl102.cfg` 与 `func_cdgc101.cfg`：

- 吸并方换股溢价为 `(当前价-hgj1)*100/hgj1`；
- 被吸并方现金选择权溢价为 `(当前价-xjxzq)*100/xjxzq`；
- 被吸并方换股溢价为 `(当前价-hgj2)*100/hgj2`；
- B 转 H 的现金选择权按 B 股当前价和原币种选择权价格复算，不跨币种汇总；
- 市值管理预警的客户端原始列为 20 个交易日累计收盘跌幅、近一年最高收盘价
  及相对高点跌幅。公开 L1 只补充当前价和当前相对高点变化，不改写原始阈值。

`SH600449` 吸收 `834082` 的原始 `$SC1` 为空。工具通过证券目录恢复为
`BJ834082`，没有根据首位数字把它误分到沪市。该边界同时进入单元测试和固定
API 契约。

## 使用

```text
tdx-tool market special-situations --input-dir output/tdx-jsn --no-quotes
tdx-tool market special-situations --view mergers
tdx-tool market special-situations --market sh --code 600449

GET /api/v1/market/special-situations?view=all&include_quotes=0
GET /api/v1/market/special-situations?view=market-cap-risk&q=创业板指
GET /api/v1/market/special-situations?market=sh&code=600449
```

默认本地优先；只有显式 `refresh=1` 才刷新三张 JSN。`include_quotes=0` 可完全
离线返回主表，在线行情失败也只进入 `quote_errors`，不会把完整主表降成 HTTP
错误。数据中心提供三类切换、筛选、排序和明细；个股工作台会同时按主证券和
关联证券反查。

## 验证

- 离线主表：4 + 3 + 172 = 179；预警触发 90 + 62 + 20 = 172；
- 公开 L1 样本：4 条吸收合并涉及 7 只唯一证券，7/7 取得行情、0 错误；
  `SH600095` 当前价 8.12，吸并方换股溢价 8.1225033%，现金选择权溢价
  -13.116474%，与客户端公式一致；
- C++ 全量测试 70/70；Svelte 检查 0 错误、0 警告且生产构建通过；
- 临时服务和正式服务 `special-situations-live` 均为 1/1；完整 API 契约
  均为 105/105；
- JSN 覆盖 349/616；384 个下载文件全部匹配、解析错误 0，已下载通用独占
  剩余 16 个。

证据文件：

- `output/probes/tdx-market-special-situations-offline-20260807.json`
- `output/probes/tdx-market-special-situations-mergers-live-20260807.json`
- `output/probes/api-contract-special-situations-temp.json`
- `output/probes/api-contract-special-situations-formal.json`
- `output/probes/api-contracts-temp-current.json`
- `output/probes/api-contracts-formal-current.json`
- `output/probes/tdx-jsn-variants-current.json`

## 正式部署

- 服务：`http://127.0.0.1:8765`，PID `37892`；117 个功能、384 个 JSN 资源、
  11,100 个证券索引键；
- `tdx-tool.exe`：35,357,449 字节，SHA256
  `D06E948CE1A641B3BCD6355CC0EF388204638BF68CC6F8F90AE3CFC45966CCF0`；
- Svelte：`index-DwpI0Z7r.js` / `index-DhZgNhwi.css`；HTML/JS/CSS SHA256 分别为
  `14525BC3CA1673F80F8C6480BEF876AA967203ECFBC64591FC239E8FAD29E5F3`、
  `44F407CD3376AB10A7F36BD1033CBBE4C8125ADD2D0BC81F2904B7F2315D1C8E`、
  `2042791946C8478FC68AA3B7CAA8AE53A6282E872284E91F4FF2C7AE2261EDDB`。
