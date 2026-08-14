# 原生 C++ TQLEX 协议层模块化

日期：2026-08-11

## 目标

拆解 757 行的 `tqlex.cpp`。原文件同时负责 XML 配置解码、模板替换、请求字段访问、
TQLEX HTTP 传输、响应分页合并、配置执行和 CLI，任一协议细节调整都会触碰整个单体。

## 落地结果

原根文件已移除，公开 `tdx/tqlex.hpp` API 与两个 JSON schema 保持不变：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `tqlex_config_text.cpp` | 221 | 编码识别、XML/HTML 解码、占位符和单引号 JSON |
| `tqlex_response.cpp` | 134 | 请求字段访问、响应校验、结果集和分页合并 |
| `tqlex_catalog.cpp` | 89 | cloud_cfg 扫描与配置目录文档 |
| `tqlex_request.cpp` | 85 | 模板实例化、候选选择和字段覆盖 |
| `tqlex_transport.cpp` | 100 | HTTP 查询、错误处理和安全分页 |
| `tqlex_execute.cpp` | 46 | 配置查询应用编排 |
| `tqlex_command.cpp` | 161 | 参数解析、重试与输出 |

内部协议边界集中在 `tqlex_internal.hpp`。分页约束由类型化 `PaginationPolicy` 校验，
六项 CLI 默认值集中在 `TqlexCommandDefaults`，避免继续把限制和常量散落在请求控制流中。

## 测试提速

TQLEX 的模板、传输、分页和业务错误断言已从 1,471 行的通用
`native_tests.cpp` 迁到 118 行的 `tdx-tqlex-tests`，通用文件降至 1,396 行。
后续协议层修改默认只构建并运行该专项测试，不再运行整套通用测试。

## 增量验证

- `tdx-tqlex-tests` 与 `tdx-tool` 增量构建通过；
- 四组专项测试覆盖单引号模板、分页宏、请求 URL/负载、大小写分页键、多结果集合并和
  `ErrorCode` 拒绝；
- 真实 `--list` 样本读取 73 项配置、11 个 Entry 和 34 个 ReqId，schema 保持
  `tdx-tqlex-configs-native-v1`；
- `formula-evaluation` 定向 API 契约通过；
- 本轮仅移动协议边界并增加类型约束，未运行完整 CTest。

## 后续候选

当前最大生产实现为 749 行的 `native/src/hk_events.cpp`；最大测试实现为 1,417 行的
`native/tests/formula_engine_context_library_tests.cpp`。
