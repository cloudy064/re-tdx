# 原生 C++ 服务状态、市场控制器与路由目录拆分

日期：2026-08-10

## 拆分结果

本批把 `server.cpp` 从 4,834 行降到 1,463 行，并按职责形成以下边界：

- `server.cpp`：HTTP 请求解析、静态资源、顶层路由和监听循环；
- `server_state.cpp`：唯一的服务组合根，负责加载板块/公式索引并构造业务服务；
- `server_market.cpp`：市场 API 参数适配、业务控制器和固定路由目录；
- `server_formula.cpp`：公式、扫描、策略和回测控制器；
- `server_catalog.cpp`：功能目录与 OpenAPI 文档。

共享状态、HTTP 响应和市场路由接口分别放入内部头文件，生产头文件没有暴露
这些实现细节。CMake 使用 `TDX_SERVER_SOURCES` 集中管理服务器翻译单元。

## 路由与错误策略

102 个固定市场 GET 路由改为 `ApiJsonRoute` 类型化目录。编译期断言固定条目数并
检查路径唯一性；运行时只构造一次 `unordered_map<string_view, route*>` 哈希索引。
新增接口不再需要修改数百行 `if` 链。

上游失败处理使用 `UpstreamErrorPolicy` 组合标志，统一表达：

- 已知瞬时网络错误；
- TQLEX 错误；
- PBRPC 错误；
- `RpcID -1`。

原先十处重复的错误识别、503 响应和 `retryable` 判断已合并，接口状态码和 JSON
字段保持不变。

## 增量验证

- `tdx-tool` 受影响目标重新配置、编译和链接通过；
- 路由目录静态审计为 102/102 唯一路径；
- 候选服务在 `127.0.0.1:8875` 验证 health、features、配股日历、基金日历，均
  返回 200；无效员工视图仍按参数契约返回 400；
- 哈希索引落地后再次以配股日历做代表请求，返回
  `tdx-market-calendar-native-v1`、1 条记录；
- 候选服务按 `--max-requests` 自动退出，正式 8765 服务进程未替换；
- 没有运行完整 CTest 或完整 API 巡检。

当前生产 `.cpp` 超过 5,000 行的数量为 0。下一轮优先拆分
`formula_engine.cpp` 的词法语法、静态分析、执行与 CLI，以及
`formula_context.cpp` 的板块、财务时点、横向统计和市场汇总绑定。

