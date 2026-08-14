# 本地历史证券兼容名称闭环

## 目标

把客户端 `T0002/hq_cache/pttab.dat` 恢复为独立、可审计的纯 C++ 数据源，并用于
补齐已经不在当前 TNF 证券目录中的历史证券名称。该文件不能仅凭名称推断成
“退市表”；实现会明确区分兼容名称、当前名称和当前目录是否存在。

## 原生证据

`TDXRun.dll.i64` 中已定位到完整生产链：

- `sub_10043E10 @ 0x10043E10` 以 `rb` 打开 `hq_cache\pttab.dat`；
- 每行按逗号拆成三列，依次读取市场、六位代码和名称；
- 原对话框把代码强制截到 6 字节、名称强制截到 8 字节，再把市场标签、代码和
  名称插入列表；
- `sub_10044230 @ 0x10044230` 是对话框初始化消费者并调用上述 loader。

因此磁盘格式固定为：

```text
market,code,name
```

市场值只接受 `0/1/2`。8 字节是原生对话框的显示限制，不是资源文件的数据边界；
纯 C++ 解析器保留 GB18030 解码后的完整名称。

机器可读/可复核证据：

- `output/ida_probe_tdxrun_pttab_20260812.py`；
- `output/ida-tdxrun-pttab-20260812.log`；
- `output/ida_probe_tdxrun_pttab_callers_20260812.py`；
- `output/ida-tdxrun-pttab-callers-20260812.log`。

## 实现

新增的职责保持独立：

- `native/include/tdx/historical_securities.hpp`：查询模型和公共入口；
- `native/src/research/historical_securities.cpp`：严格读取、校验、过滤、排序、
  分页和当前目录对账；
- `native/src/research/historical_securities_command.cpp`：CLI 编排；
- `native/tests/historical_securities_tests.cpp`：格式、筛选、改名和损坏输入测试。

公开入口：

```text
tdx-tool market historical-securities --presence absent --limit 100
GET /api/v1/market/historical-securities?presence=absent&limit=100
```

schema 为 `tdx-market-historical-securities-native-v1`。每条记录同时提供
`compatibility_name`、`current_name`、`current_directory_present` 和
`name_differs_from_current`；传输固定为 `local-files`，网络请求数为 0。

`market hot-history` 也会在当前 TNF 名称缺失时读取同一目录。当前名称始终优先，
兼容表只作为 `historical-compatibility` 回退；回退证券不会被误标成可向当前原生
行情主站请求的证券。

## 真实安装结果

当前 `pttab.dat` 共 1,781 行，全部恰好三列、市场和代码有效、主键唯一：

- 深市 1,072、沪市 707、北交所 2；
- 当前目录仍存在 2 条，缺席 1,779 条；
- 当前存在的两条中有 1 条兼容名称与当前名称不同：`SZ166007` 的兼容名称为
  `中欧300`，当前名称为 `中欧互通`；
- 它为 210 条热点历史记录中的 8 条（7 只证券）补齐名称，未解析名称从 8 降为 0。

真实输出：

- `output/local-historical-securities-live-20260812.json`；
- `output/local-hot-history-names-live-20260812.json`。

## 验证

- 增量构建：`tdx-historical-securities-tests`、`tdx-hot-history-tests`、
  `tdx-recon-contract-tests`、`tdx-tool`；
- 聚焦 CTest：3/3 通过，1.81 秒；
- 真实 CLI：1,781/1,781 条兼容名称和 210/210 条热点历史成功；
- 新增 `historical-securities-local` HTTP 契约，快速组 9/9 通过；
- 按 API 边界规则运行 full 契约：211/225；新增契约全部断言通过，14 个失败均为
  既有实时数据基数、日期漂移或上游参数项，不涉及本地路由、解析器或 schema；
- 临时服务只监听 `127.0.0.1:18883`，验证后已停止；正式 `8765` 始终保持
  PID 24096；临时服务 stderr 为 0。

实现完全为 C++，没有 Python 运行时或转发层。
