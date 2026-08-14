# 原生 C++ 实时行情核心模块化

日期：2026-08-11

## 目标

拆分原先同时承担协议解码、JSON 投影、批量下载、持久连接和 CLI 的
`native/src/market.cpp`，保持公开函数、L1 会话行为和所有 JSON schema 不变。

## 结构调整

原文件共 1,212 行，现已移除并拆为八个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `market_protocol.cpp` | 491 | 0x054C/0x053E/0x0547 请求、解码和公开 fixture 解码器 |
| `market_document.cpp` | 193 | snapshot/speed/depth JSON 投影与文档组装 |
| `market_session.cpp` | 175 | 可复用的公开 7709 L1 会话、切换端点和状态统计 |
| `market_watch_command.cpp` | 164 | JSONL 变更流、心跳和退避重连命令 |
| `market_support.cpp` | 106 | 参数边界、证券目录加载和会话参数转换 |
| `market_command.cpp` | 93 | snapshot/speed/depth 子命令入口 |
| `market_transport.cpp` | 87 | 批量请求、端点失败转移与传输统计 |
| `market_service.cpp` | 71 | 三种公开行情服务函数 |

`market_internal.hpp` 提供内部契约。三种行情面由 `QuoteSurfaceDefinition` 类型目录统一保存
命令号、命令文本和 schema；13 条证券前缀价格缩放规则也从解码控制流移入只读目录。
重复的五档投影、批量文档组装和会话选项转换均归并为共享函数，没有引入只为拆文件服务的
继承关系。

## 兼容性

- `native/include/tdx/market.hpp` 与 `native/include/tdx/market_stream.hpp` 未改变；
- snapshot、speed、depth 和 watch 四个子命令名称及参数保持不变；
- `tdx-market-snapshot-native-v1`、`tdx-market-speed-native-v1`、
  `tdx-market-depth-native-v1` 和 watch v2 契约保持不变；
- 基金 IOPV、逆回购价格精度和 SH999997 市场总委托金额哨兵语义继续保留。

## 增量验证

- `tdx-native-tests`、`tdx-market-stream-tests` 与 `tdx-tool` 编译链接通过；
- 两个直接相关测试可执行文件均通过；
- `market snapshot --security sz:000001` 返回平安银行 1/1 条记录；
- 样例保存为 `output/market-refactor-snapshot-sample.json`，schema 为
  `tdx-market-snapshot-native-v1`，命令号为 `0x054C`；
- 没有执行完整 CTest。

## 后续候选

生产代码当前最大的单文件为 1,137 行的 `native/src/formula_context_relations.cpp`；测试代码
最大文件仍为 1,564 行的 `native/tests/native_tests.cpp`。
