# 2026-08-06 原生 A 股/ETF 开盘与盘后成交排行

## 页面与四分支语义

本轮从 `KPHPHJY.sp` 和四份 GBK CFG 恢复“开盘和盘后交易”页面，而不是只按
`phcje` 缩写猜测字段：

- `func_phcje101_1.jsn`：A 股，客户端默认按盘后成交 `phje` 排序；
- `func_phcje101_2.jsn`：A 股，客户端默认按开盘成交 `kpje` 排序；
- `func_phcje103_1.jsn`：ETF，默认按盘后成交排序；
- `func_phcje104_1.jsn`：ETF，默认按开盘成交排序。

同一证券范围的两张表字段、证券集合和统计日期相同，差异是上游主排序。四张表
均包含统计日收盘、前收、开盘、总成交、开盘成交和盘后成交。CFG 中的当前涨幅、
现价、总市值和行业属于宿主 `syscol`，不在 JSN 中，本实现没有伪造。

## 纯 C++ 类型化接口

新增 `market session-turnover` 和 `/api/v1/market/session-turnover`，支持：

- `universe=a|etf`；
- `sort=after-hours|opening|total|after-hours-share|opening-share|close-change|open-change`；
- `order=asc|desc`；
- `activity=all|after-hours|opening|both`；
- 市场、单票、文本过滤及分页；
- 从 CFG 明示价格计算统计日收盘/开盘涨幅，并计算开盘/盘后成交占全天比例；
- 每行保留 `raw`，来源保留 endpoint、重试、陈旧缓存和年龄。

金额字段按 CFG 的成交金额类型输出为元；上游空字符串保持 `null`，不会被变成
零。服务按查询排序选择对应的四个真实资源，同时仍在 C++ 中执行稳定排序，避免
依赖上游文件顺序。

## 真实证据

2026-08-06 当前数据：

- A 股 5,505 只，总成交 2.547 万亿元；开盘成交 258.05 亿元、活跃 5,495 只；
  盘后成交 12.40 亿元、活跃 3,893 只；
- ETF 1,619 只，总成交 5,069.89 亿元；开盘成交 25.28 亿元、活跃 1,148 只；
  盘后成交 1.51 亿元、活跃 656 只；
- 平安银行总成交 11.71 亿元、开盘成交 716.97 万元、盘后成交 28.74 万元。

逐分支证据：

- `output/probes/session-turnover-a-after-hours.json`；
- `output/probes/session-turnover-a-opening.json`；
- `output/probes/session-turnover-etf-after-hours.json`；
- `output/probes/session-turnover-etf-opening.json`；
- `output/probes/session-turnover-sz000001.json`。

## 覆盖、验证与部署

- 四个 `phcje` 模板全部绑定 `market session-turnover`；类型化覆盖由 228/616
  提升为 232/616，通用独占由 388 降为 384；
- 三个此前未落盘的实时分支已校验 MD5 并原子写入 `output/tdx-jsn`；下载文件
  由 330 增至 333、已下载模板由 291 增至 294，已下载但通用独占由 96 降至 95；
- 新增 `tdx-session-turnover-tests`，覆盖四资源路由、北证身份、金额/比例、空值、
  排序和原始行；全量 CTest 50/50；
- 新增 A 股盘后与 ETF 开盘两项正式契约，临时及正式服务均为 40/40；
- 正式服务 PID `33216`，命令行
  `dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`；
- EXE SHA-256：
  `F3B6A2A097DF3C2E09F5C82E74703A66D93714C02244F2D23CD2FFCE05D1C606`；
- 功能目录 96 项，`native_cpp=true`、`python_runtime=false`；本轮未修改 Svelte UI。

## 下一批高收益方向

`dfkzz201` 已在后续阶段并入现有 `market convertible-bonds`，见
[待发可转债视图](2026-08-06-native-pending-convertible-bonds.md)。当前已下载缺口
依次转为 `bkld102/104`、`bygtj102`、`lbtt101`、`ldph101` 和五张 `qszj`；
下一轮优先审计板块联动资源，继续避免按文件名重复建设已有接口。
