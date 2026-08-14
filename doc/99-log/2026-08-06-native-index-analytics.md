# 指数波动率与全收益指数差纯 C++ 固定化

## 已实现波动率主从链

`T0002/cloud_cfg/zs_zsbdl.xml` 定义同一
`HQServ.hq_nlp_indexCorr64 / mod_indexCorr64.dll` 下的两条普通 JSON 请求：

- `200004`：按 `Window` 返回指数目录、两年/一年/半年/季度已实现波动率均值，
  以及当前季度均值在两年/一年/半年历史中的分位；
- `200003`：按起止日期、指数代码、市场号和 `Window` 返回每日
  `RealizedVolatility`。

新增纯 C++ `market index-volatility` 和固定只读
`/api/v1/market/index-volatility`，提供 `catalog/history/security` 三个视图。
`Window` 是滚动交易日数，波动率和分位保持服务端百分点单位；默认历史区间为
最近两个日历年。目录可搜索或按市场/代码过滤，单指数视图将目录统计与历史
序列连接；选中代码不存在时返回 `availability=empty` 和显式空关系。

窗口 5 的真实目录当前为 100 个指数。上证指数 `SH999999` 的两年均值
0.843964%，2024-08-06 至 2026-08-06 共 485 个日点，最新值 0.857203%，
目录与历史序列的量级一致。

## 价格指数与全收益指数

`T0002/cloud_cfg/sc_qsyzs.xml` 使用
`HQServ.hq_nlp_copilot / mod_copilot.dll / 200770`。年份和月份决定区间，月份
0 表示当年截至当前；服务端返回价格指数和对应全收益指数的区间前值、区间
末值、各自收益率和 `zoneZDFGap`。

新增纯 C++ `market total-return-gap` 和固定
`/api/v1/market/total-return-gap`。`total_return_advantage_pct` 严格等于全收益
指数区间收益减价格指数区间收益，用百分点表示。2026 年当前返回 68 组，
全部为正，均值约 2.229 个百分点；深证成指为 4.578%，深成指R 为 5.534%，
差 0.956 个百分点。最大差当前是上民红利与上民红利全收益的 4.223 个百分点。

真实集合揭示身份边界：39 个价格指数、62 个全收益指数使用通达信内部市场号
62，部分代码还是 `H00009` 或 `000688CNY01`。工具使用 `62:<code>` 保留它们，
没有猜成上交所或深交所代码。XML 中“当日收益差”由两侧实时行情 `syscol`
在客户端计算，不是 `200770` 字段，因此接口显式返回
`daily_gap_available=false`。

## 证据

- `output/probes/tqlex-200004-window5-current.json`；
- `output/probes/tqlex-200003-sh999999-window5-current.json`；
- `output/probes/index-volatility-catalog-current.json`；
- `output/probes/index-volatility-sh999999-current.json`；
- `output/probes/tqlex-200770-2026-current.json`；
- `output/probes/total-return-gap-2026-current.json`；
- `output/probes/total-return-gap-sz399001-current.json`。

新增单元测试覆盖目录统计、市场和名称解析、历史日期排序、内部市场号 62、
百分比剥离及价格/全收益指数配对。

## 回归与部署

全量 CTest 为 36/36 通过。临时 API 功能目录增至 85；上证指数返回 485 个
历史点，深证成指配对返回 0.956 个百分点，缺失指数返回
`availability=empty`、`selected_index_found=false` 和 `matched_rows=0`。

该里程碑的正式 `8765` 服务曾完成纯 C++ EXE 热替换，PID 为 `27280`，EXE SHA-256 为
`5E8E2AC7A6D524D2F966B3705BD329B201B5B3BBEBB89F23600FDFAC3E48DEF2`。
健康检查 HTTP 200，正式指数波动率请求约 632 ms，全收益差请求约 334 ms。
Svelte HTML/JS/CSS 哈希仍为 `0F1FBB...5777`、`B03C28...4011`、
`733085...30A`，本轮没有覆盖 UI。此部署后来已被基金分析里程碑的正式版本
取代，当前 PID 和哈希以对应日志为准。
