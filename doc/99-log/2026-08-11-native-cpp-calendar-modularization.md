# 原生 C++ 市场日历模块化

日期：2026-08-11

## 目标

将原先集中在 `native/src/calendar.cpp` 的资源常量、行归一化、查询编排和命令行处理拆开，
降低单文件修改冲突，同时保持 `CalendarService`、命令参数和 JSON 契约不变。

## 结构调整

原文件共 1,212 行，现已移除，并拆为四个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `calendar_support.cpp` | 463 | 字段解析、证券关联、富文本和视图匹配支持 |
| `calendar_normalize.cpp` | 407 | 28 类 JSN 资源的统一日历记录归一化 |
| `calendar_service.cpp` | 310 | 资源抓取、缓存、过滤、排序和结果组装 |
| `calendar_command.cpp` | 60 | `market calendar` 参数解析与输出 |

内部共享契约位于 `calendar_internal.hpp`。24 个公开视图使用 `CalendarView` 与
`ViewDefinition` 的编译期目录统一管理，28 个资源路径也集中为具名常量，查询控制流不再
维护重复的字符串集合。这里采用类型目录和函数组合，没有引入无状态的继承层级。

## 兼容性

- 公开头文件 `native/include/tdx/calendar.hpp` 未改变；
- 原有 view 参数名称和非法 view 的错误文本保持不变；
- 输出 schema 继续为 `tdx-market-calendar-native-v1`；
- 资源数量保持 28，过滤、证券关联和排序规则沿用原实现。

## 增量验证

- `tdx-calendar-tests` 与 `tdx-tool` 编译、链接通过；
- `tdx-calendar-tests.exe` 执行通过；
- `market calendar --view futures --limit 3` 返回 3 条记录；
- 样例输出为 `output/calendar-refactor-futures-sample.json`，schema 与资源目录数量符合预期；
- 未修改共享解析器、传输层或 schema，因此没有运行完整 CTest。

## 后续候选

生产代码当前最大的单文件为 1,212 行的 `native/src/market.cpp`；测试代码最大的单文件为
1,564 行的 `native/tests/native_tests.cpp`。后续仍按单模块目标、专项测试和一个代表样例推进。
