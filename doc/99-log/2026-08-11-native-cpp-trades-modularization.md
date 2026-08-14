# 原生 C++ 逐笔成交链路模块化

日期：2026-08-11

## 背景

`native/src/trades.cpp` 累积到 605 行，把 7709 公开行情协议的三个命令常量、证券代码解析、
带符号 varint 记录解码、分页下载、三个时间窗口的聚合、JSON 投影和 CLI 适配全部压在同一个
翻译单元里。协议层和聚合层混在一起后，改动任一侧都要重读整个文件，且匿名命名空间里的
17 个 helper 无法被单独审阅。

本轮按既有责任分层模式（support → records → protocol → aggregate → project → fetch →
command）拆分，公开 API 与 JSON 契约保持不变。

## 拆分结果

| 实现单元 | 行数 | 职责 |
| --- | --- | --- |
| `src/trades_support.cpp` | 133 | 字节写入、参数与证券代码解析、日期归一化、时间标签、买卖方向、价格除数 |
| `src/trades_records.cpp` | 78 | 带符号 varint 解码与逐笔记录解析（累积价格 + 溢出保护） |
| `src/trades_protocol.cpp` | 78 | `build_today_trades_request_data` / `build_history_trades_request_data` 与两个载荷解析入口 |
| `src/trades_aggregate.cpp` | 116 | 分组统计与 `aggregate_trade_ticks` 的三个窗口 |
| `src/trades_project.cpp` | 52 | `tick_json` / `series_json` 投影 |
| `src/trades_fetch.cpp` | 181 | 单票分页下载、端点轮询降级、`fetch_market_trades_document` 组装 |
| `src/trades_command.cpp` | 59 | `command_market_trades` CLI 适配 |

原文件重命名为 `src/trades.cpp.removed`，`native/CMakeLists.txt` 的单条目替换为上述七条。

## 内部接口

新增 `native/include/tdx/trades_internal.hpp`，暴露 `tdx::detail::trades` 命名空间，
把原匿名命名空间的 helper 提升为跨单元可见声明：

- 协议常量固定为编译期值：`type_heartbeat = 0x0004`、`type_today_trades = 0x0FC5`、
  `type_history_trades = 0x0FC6`、`maximum_pages = 100`
- `struct SecurityCode`（`market_id` + `code` + `key()`）保留原有的 `std::pair` 排序键语义
- 按段落分组声明：字节组装、参数/标识解析、字段格式化、记录解码、聚合、投影、下载

公开符号仍定义在 `tdx` 命名空间中，头文件 `native/include/tdx/trades.hpp` 未改动，
`block_trades`、`registry`、`server_market_realtime`、`server_market_research`、
`server_state` 以及两个测试文件的引用路径不变。

## 保持不变的语义

- 输出 schema 仍为 `tdx-trades-native-v1`，`command` 字段仍按 `trading_date` 是否为空
  在 `"0x0FC5"` / `"0x0FC6"` 间切换
- 证券代码解析的三条启发式（`市场:代码` 前缀、8 字符带市场前缀、裸 6 位代码）顺序不变
- `normalize_date` 的闰年校验分支不变
- varint 解码的位布局不变：首字节 6 位有效载荷（`& 0x3F`）+ 符号位 `0x40`，后续字节
  7 位续传（`& 0x7F`）；累积价格的 int64 溢出保护保留
- 价格除数仍对 `10/11/12/15/16/50/51/52/53/56/58` 前缀返回 1000，其余返回 100
- 聚合输出三个窗口：`auction_0925`（9\*60+25）、`closing_1500`（15\*60）、
  `post_close_status_5`（`status_raw == 5` 且 `time_minutes > 15*60`），另含逐分钟序列
- 分页下载的反向页面装配、心跳保活和端点轮询失败聚合行为不变

## 模式一致性

| 关注点 | 本轮落点 | 同期模块对应单元 |
| --- | --- | --- |
| 公共支持 | `trades_support.cpp` | `curated_data_support.cpp` / `announcement_signals_support.cpp` |
| 编译期常量/目录 | `trades_internal.hpp` 常量段 | `curated_data_catalog.cpp` 的 `ResourceEntry` |
| 归一化/解码 | `trades_records.cpp` + `trades_protocol.cpp` | `curated_data_normalize.cpp` |
| 抓取与降级 | `trades_fetch.cpp` | `curated_data_fetch.cpp` |
| 响应组装 | `trades_project.cpp` + `trades_aggregate.cpp` | `curated_data_service.cpp` |
| CLI 适配 | `trades_command.cpp` | `curated_data_command.cpp` |

协议常量本轮直接固定在内部头文件里，而没有像资源型链路那样建独立 catalog 单元：这里只有
四个标量常量、没有需要绑定的并行属性（kind/标签/字段后缀），单独建一个 `.cpp` 只会增加
一个空壳翻译单元。

## 规模对比

原 605 行 → 七个单元共 697 行（+15.2%），增量来自各单元的 include 头和命名空间壳。
最大单元 181 行，相对原文件下降 70%。

## 已完成的静态校验

- 七个公开符号（两个 request builder、两个 payload parser、`aggregate_trade_ticks`、
  `fetch_market_trades_document`、`command_market_trades`）各只有一处定义
- 内部头文件的 18 条声明与各单元定义逐条对应
- 命名空间开闭平衡（`namespace fs = std::filesystem;` 别名不计入）
- `trades_protocol.cpp` 的文件级 `using detail::trades::...` 声明与所含头文件中
  `tdx` 命名空间作用域的名字无冲突
- 字符串字面量全量比对：原文件 171 个去重字面量在新单元中零丢失，新增的三项均为
  include 路径
- `CMakeLists.txt` 中不再残留 `src/trades.cpp`

## 未执行的验证

当前环境没有可用的 C++ 编译器（无 MSVC / MinGW，`cmake --build` 无法配置），以下项目
仍需在具备工具链的环境中补做：

- `tdx-native-tests`（覆盖请求字节相等、逐笔解析的绝对序号/时间标签/价差/买卖方向、
  历史成交价格基准、`auction_0925` 与 `closing_1500` 聚合、基金代码 `510300` 的 1000 除数）
- `tdx-block-trades-tests`（依赖 `trades.hpp`）
- `tdx-tool` 链接
- 一次真实 CLI 采样：`tdx-tool market-trades`
