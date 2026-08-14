# 本地 K 线历史热点区间闭环

## 目标

把客户端 `T0002/hq_cache/speczshot.txt` 恢复为可查询的纯 C++ 数据能力，补齐
现有云端“爆炒复盘”只保留当前少量记录、无法查询多年历史的问题。实现继续遵守
手工重构后的 `research / registry / server` 边界，不修改既有题材机会 schema。

## 原生证据

`TdxW.exe.i64` 的字符串交叉引用把该文件定位到两个函数：

- `sub_641DB0` 下载 `spec/speczshot.txt` 到本地缓存；
- `sub_5DFD00 @ 0x5DFD00` 是实际 loader。它按管道读取市场、代码、起止日期、
  交易日数、主题、两项收益和分析文本，先经 `sub_401AB0` 检查证券，再生成
  1,110 字节记录；分析文本的原生容量为 1,024 字节。

消费者 `sub_9912D0 @ 0x9912D0` 在 K 线坐标中查找起止日期，并调用
`sub_987540 @ 0x987540` 绘制区间框、主题、证券名和收益。因此这份数据是
“K 线历史热点区间叠加层”，不是 MACD/KDJ 指标、普通板块或当前涨幅榜。

字段收益语义用真实日线闭合。`SH603137` 的区间为
`20260615..20260715`，起点前收盘 `11.76`、终点收盘及最高价 `35.61`，
`35.61 / 11.76 - 1 = 202.80%`，与第 7、8 列均精确一致。另一个样本终点
收盘与区间最高价不同，可确认：

- 第 7 列是终点收盘相对起点前一交易日复权基准的区间收益；
- 第 8 列是区间最高价相对同一基准的峰值收益；
- 第 5 列是市场交易日数，不是证券实际成交 K 线数。

原 loader 只调用 `ParseMessageStr(..., 9)` 读取第 9 段。当前文件有 23 条说明
自身包含 `|`，原客户端会只显示第一段。新实现把第 9 段保留为
`native_client_analysis`，同时把后续段重新拼入 `analysis`，既可精确对照原宿主，
也不丢失本地资源内容。

机器可读反编译证据：

- `output/ida-speczs-string-xrefs-20260812.json`；
- `output/ida-speczs-consumers-20260812.json`。

## 实现

新增独立小模块：

- `native/include/tdx/hot_history.hpp`：查询模型和公共入口；
- `native/src/research/hot_history.cpp`：GB18030 解码、严格校验、筛选、排序、
  分页、证券名称关联和完整说明恢复；
- `native/src/research/hot_history_command.cpp`：薄 CLI 编排；
- `native/tests/hot_history_tests.cpp`：字段、说明恢复、日期重叠、北交所、排序分页
  与损坏输入测试。

公开入口：

```text
tdx-tool market hot-history --market sh --code 603137
GET /api/v1/market/hot-history?market=sh&code=603137
```

支持 `market/code/q/from/to/sort/order/offset/limit`。日期过滤采用区间重叠语义，
适合 K 线缩放后按可见日期请求叠加层。API 只使用服务启动时的 TDX 根目录，
不接受任意服务器文件路径，也不发网络请求。

## 真实安装结果

当前 `speczshot.txt` 为 44,429 字节、210 条记录、203 只证券，时间覆盖
`20150710..20260715`：

- 深市 127 条、沪市 82 条、北交所 1 条；
- 202 条记录由当前证券目录补齐名称，另有 8 条（7 只证券）由
  `pttab.dat` 的历史兼容名称补齐，未解析名称为 0；
- 23 条包含嵌入式管道并已恢复完整说明；
- 最大终点区间收益 `1080.37%`，最大区间峰值收益 `1090.46%`。

现有云端 `RDHS101` 在同次观察中只返回 1 条已完成爆炒记录；本地历史表因此是
明显新增的多年复盘能力，而不是重复包装。真实输出见
`output/local-hot-history-names-live-20260812.json`。

## 验证

- 增量构建：`tdx-hot-history-tests`、`tdx-historical-securities-tests`、
  `tdx-recon-contract-tests`、`tdx-tool`；
- 聚焦 CTest：3/3 通过，1.81 秒；
- 真实安装样本：210/210 条解析成功；
- 临时 `18841` 服务：全量分页、`SH603137` 单票、日期重叠和非法市场 4 个
  API 契约通过；临时监听已退出；
- 正式 8765 服务始终保持原监听；
- 新模块为纯 C++，不引用或转发 Python。

后续新增独立历史证券 API 后，按 API 边界规则运行了完整契约；结果为 211/225，
新增本地契约全部通过，失败项均属于既有实时数据基数或上游参数漂移。

## 下一步

同一下载链的 `speczsevent.txt` 和 `speczsevent_ds.txt` 已在后续
`market index-events` 中闭合。热点历史现与历史证券兼容名称形成完整本地链路。
