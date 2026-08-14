# 原生 C++ 强势股链路模块化

日期：2026-08-11

## 目标

拆解 694 行的 `strong_stocks.cpp`。原文件同时维护资源、字段工具、区间/逐日摘要、两类
归一化、两套排序规则、缓存查询和 CLI。

## 落地结果

原根文件已移除，公开 API 与 `tdx-market-strong-stocks-native-v1` schema 保持不变：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `strong_stocks_catalog.cpp` | 83 | 四视图、主资源、默认排序和两类排序目录 |
| `strong_stocks_support.cpp` | 181 | 字段、日期、证券身份、搜索、数值与路径工具 |
| `strong_stocks_summary.cpp` | 98 | 区间总体与逐日详情摘要 |
| `strong_stocks_normalize.cpp` | 105 | 生命周期区间和逐日明细归一化 |
| `strong_stocks_sort.cpp` | 51 | 目录驱动的两族排序 |
| `strong_stocks_service.cpp` | 212 | 缓存、视图装载、筛选、分页和响应 |
| `strong_stocks_command.cpp` | 79 | CLI 参数、根目录和输出 |

`ViewDefinition` 将视图、中文标签、资源、字段说明、默认排序/顺序和排序族绑定。四视图在
编译期检查枚举和名称唯一性；九项区间排序与五项详情排序集中管理。目录页直接从同一视图
目录生成，不再复制资源和字段说明。

## 增量验证

- `tdx-strong-stocks-tests` 与 `tdx-tool` 增量构建通过；
- 强势股专项测试通过；
- 真实样本返回 614 个区间、562 只证券和一项来源，限制返回三条，availability 为 `live`；
- `research-signals` 定向契约覆盖区间、详情和错误顺序并通过；
- 未改变共享解析、传输或 API schema，未运行完整 CTest。

## 后续候选

排除 L2 和共享 `jsn.cpp` 后，下一低风险候选为 692 行的
`native/src/tpool_flow.cpp`。
