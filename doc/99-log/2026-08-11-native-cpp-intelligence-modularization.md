# 原生 C++ 市场情报链路模块化

日期：2026-08-11

## 目标

原 `native/src/intelligence.cpp` 约 1,225 行，同时包含资源常量、字段解析、数据归一化、
关系图构建、缓存与查询编排以及 CLI。公开 API 虽然稳定，但新增视图时需要在同一文件多处
追加字符串判断，职责边界不清晰。

## 拆分结果

| 单元 | 职责 | 行数 |
|---|---|---:|
| `intelligence_internal.hpp` | 12 个视图的类型化目录、资源常量、内部端口 | 101 |
| `intelligence_support.cpp` | 通用解析、证券键、筛选、资源选择策略 | 401 |
| `intelligence_normalize.cpp` | 各 JSN 资源到公开 JSON 的归一化 | 477 |
| `intelligence_graph.cpp` | 事件—证券关系图 | 68 |
| `intelligence_service.cpp` | 缓存、视图编排和返回文档组装 | 392 |
| `intelligence_command.cpp` | 命令行参数与输出 | 62 |

原 `intelligence.cpp` 已移除。公开 `tdx/intelligence.hpp`、命令名称和
`tdx-market-intelligence-native-v1` schema 均未改变。

## 结构选择

- 固定视图采用 `enum class IntelligenceView` 与 `constexpr std::array<ViewDefinition>`，
  统一完成名称校验，避免在查询函数中维护一串重复字符串条件。
- 资源地址集中在内部目录，`resources_for_view()` 是唯一的视图到资源策略入口。
- 未引入无状态的继承树。这里使用类型化注册表和小函数策略比抽象基类更直接，也更容易
  在编译期检查遗漏。
- CLI、服务、归一化和图构建各自独立翻译单元，后续变更可以只重新编译相关部分。

## 精简验证

- 增量构建：`tdx-intelligence-tests`、`tdx-tool` 通过；
- 直测：`tdx-intelligence-tests` 通过；
- CLI 契约：`market intelligence --help` 通过；
- 真实样例：`attention --limit 3` 返回 3 条、`availability=live`；
- 未触碰共享传输、解析器或 schema，因此没有运行完整 CTest。

样例保存在 `output/intelligence-refactor-attention-sample.json`，汇总数据见
`output/native-cpp-maintainability-audit.json`。
