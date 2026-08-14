# 原生 C++ 分钟线与扩展行情模块化

日期：2026-08-10

## 结果

生产文件 `minute_download.cpp` 从 1,645 行降至 467 行，并按协议与用途拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `minute_download_internal.hpp` | 71 | 11 个类型化协议常量和跨模块内部端口 |
| `minute_download_support.cpp` | 274 | 市场/周期归一化、线材数值、日期、合并与输出辅助 |
| `minute_download.cpp` | 467 | 7709/7727 K 线、报价、分时、逐笔请求与响应编解码 |
| `minute_download_kline.cpp` | 213 | 普通/扩展 K 线分页、端点切换和文档组合 |
| `minute_download_expansion.cpp` | 451 | 7727 证券目录、行情、分时和逐笔抓取 |
| `minute_download_commands.cpp` | 325 | 分钟下载及四个扩展行情 CLI 编排 |

`0x052D`、`0x23FF`、`0x23F0`、`0x23F5`、`0x23FA`、`0x240B`、
`0x240C`、`0x23FC`、`0x2406` 等协议类型和分页上限不再散落在实现文件，集中为
`minute_download_detail` 的类型化常量。公开 `tdx/minute.hpp`、wire 格式、重试语义、
命令参数和 JSON schema 均未改变。

这里按 Protocol Codec、Kline Fetch Strategy、Expansion Fetch Strategy 和 Command
Adapter 分层；没有把二进制纯函数包装成有状态类。

## 等价性与验证

拆分期间保留工作区内临时源快照，验证完成后已删除。共享辅助、协议编解码、K 线抓取、
扩展行情和命令 5 个关键区段逐字符一致；11/11 个协议常量的名称和值逐项一致。结构体
定义迁入内部头，`minute_label` 的默认参数只保留在声明处。

- `tdx-native-tests` 与 `tdx-tool` 增量编译、链接通过；
- `tdx-native-tests` 通过，其中覆盖普通及日线 wire payload 解析；
- 代表性真实执行 `market instruments --count 1` 成功，返回
  `tdx-expansion-instruments-v1` 和证券 `42:IMCI`，传输链为
  `tdx-7727-0x23f0-0x23f5`；证据保存在
  `output/minute-download-modularization-instruments.json`；
- 当前目录不是 Git 工作树，因此未执行 `git diff --check`。

未运行完整 CTest 或所有实时行情命令；本轮只移动协议实现边界，没有改变共享 transport
本身，专项原生测试、常量/代码机械校验和一个真实 7727 样例已覆盖此次结构调整。
