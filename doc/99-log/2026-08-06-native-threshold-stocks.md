# 百元股与千亿市值历史、日期名单和入围跌出

## 客户端链路

本轮继续处理通达信非 L2 高收益功能，目标页为 `QYSZ.sp`“千亿百元股”。页面
不是一个普通股票池：千亿市值和百元股是两个独立分支，各自由历史统计、日期
成员和家数走势图组成。

已确认四个资源模板：

- 百元股历史：`list/func_bygtj102_1.jsn`；
- 千亿市值历史：`list/func_qyjlb102_1.jsn`；
- 日期成员：`bygtj1/<历史行隐藏键>.jsn`；
- 家数走势：`bygtj3/<历史行隐藏键>.jsn`。

隐藏键不能按日期臆造。百元股使用 `2YYYYMMDD`，千亿市值使用 `1YYYYMMDD`，
接口始终先从历史主表取得 `$ZQDM`，再访问动态资源。千亿走势图的数量列实际为
`dbsl`，百元股为 `zjs`，两者已分别解析。

## 成员语义复核

日期成员资源包含“当日仍在名单中的股票 + 当日跌出的股票”，因此：

```text
历史总家数 = 不变 + 入围
明细总行数 = 历史总家数 + 跌出
```

使用两个有跌出记录的历史日期交叉验证：

- 2026-07-30 百元股：历史总数 169；明细 185 = 不变 166 + 入围 3 + 跌出 16；
- 2026-07-30 千亿市值：历史总数 188；明细 195 = 不变 186 + 入围 2 + 跌出 7。

当前 2026-08-06 数据：

- 百元股 212 家，其中百元至千元段 208、千元以上 4；入围 6、跌出 0；
- 千亿市值 196 家，其中万亿市值 14、千亿至万亿 182；入围 0、跌出 2，
  动态明细共 198 行；
- 两个分支的动态走势图各 119 个交易日，选定日期家数均与历史主表一致；
- 当前两组明细的本地证券名称全部解析成功。

`spj/gjjz` 是分支重载字段：百元股中是收盘价/股价净增，千亿市值中是总市值/
市值净增（亿元）。纯 C++ 模型按分支分别输出明确单位，原始行仍完整保留。
客户端用空单元格表示零入围/跌出，规范字段转换为数值 0，原始空串不改写。

## 纯 C++ 工具和 API

新增子命令：

```powershell
tdx-tool market threshold-stocks --view catalog
tdx-tool market threshold-stocks --view history --universe high-price
tdx-tool market threshold-stocks --view members --universe mega-cap --date 20260806
tdx-tool market threshold-stocks --view security --universe high-price --market sh --code 600519
```

固定 API 为 `/api/v1/market/threshold-stocks`，支持：

- `view=history|members|security|catalog`；
- `universe=high-price|mega-cap`；
- `date`、`status=all|continuing|entered|exited`、`market+code`、`q`；
- 历史和成员各自的排序、分页、刷新、超时与服务缓存。

响应 schema 为 `tdx-market-threshold-stocks-native-v1`。成员响应同时返回历史期、
完整趋势、三项成员对账结果、趋势对账结果和三条精确来源健康信息。实现不转发
Python，也不伪造客户端宿主提供的实时行情或行业列。

## 网页

Svelte 数据区新增“千亿百元股”页面：主表按日期展示股票名单，支持口径、日期、
名单状态和股票/地区/控股股东检索；侧栏展示 118/119 个历史交易日的家数、入围、
跌出和市值占比。点击股票可进入个股工作台。生产构建通过，部署资产：

- `index.html` SHA-256
  `7AC03956C53B7F2E485268042A62CC0445926A6F935BFB8C78A6095DF9EC10D8`；
- `index-RsI6Numq.js` SHA-256
  `3DD035E76F46B9D15DC08DBA533A229FA854DB18AB565B2C9E1910E4612DEC40`；
- `index-C_Itkb2P.css` SHA-256
  `274DAB2086C2B8E352F4A4DA7B075F8B86D664A4DAF189ACEBDBDE60685D1F19`。

## 验证与部署

证据文件：

- `output/probes/threshold-stocks-catalog-current.json`；
- `output/probes/threshold-stocks-high-price-history-current.json`；
- `output/probes/threshold-stocks-high-price-exited-20260730.json`；
- `output/probes/threshold-stocks-mega-cap-current.json`；
- `output/probes/jsn-variants-after-threshold-stocks.json`；
- `output/probes/api-contracts-official-current.json`。

四个模板加入类型化覆盖后，覆盖由 239/616 提升到 243/616，通用独占由 377
降到 373；补齐当前资源后本地下载文件 338 个、涉及 295 个模板，已下载但通用
独占降到 85，解析错误为 0。新增规范化、分支重载字段、京市身份、排序负例及
正式 API 契约；全量 CTest 54/54。

正式巡检增加百元股、千亿市值两项，结果 47/47。服务位于
`http://127.0.0.1:8765`，PID `30732`，功能目录 99 项；EXE SHA-256：
`BA1257CDB4F13F72FCDFCC529ECFCCEEDE639FFD81F9B5597D20F3210E328187`。
服务保持 `native_cpp=true`、`python_runtime=false`，旧开发端口 8879 关闭。
