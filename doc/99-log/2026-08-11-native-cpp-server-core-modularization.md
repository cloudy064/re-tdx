# 原生 C++ 服务核心模块化

日期：2026-08-11

## 结果

`server.cpp` 从 1,472 行和上百个业务头文件依赖缩减为 448 行的 Socket 生命周期与
`serve` 命令实现。服务核心按职责拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `server_core_internal.hpp` | 56 | 请求、查询、响应和路由之间的最小内部端口 |
| `server_request.cpp` | 318 | URL/查询/JSON 参数解析、证券标识和公共请求合并 |
| `server_queries.cpp` | 307 | 本地目录、K 线、JSN、板块和云工作流查询 |
| `server_response.cpp` | 120 | JSON/503 响应、静态资源和公式图片 |
| `server_route.cpp` | 232 | API、公式、市场注册路由及静态页面分派 |
| `server.cpp` | 448 | 路径定位、Socket 收发、SSE 和服务生命周期 |

路由继续优先委托现有公式、市场和目录控制器；没有复制业务 Handler。公开 API、请求
确认头、HTTP 状态和 JSON schema 均未改变。

## 等价性与验证

- 五个实现区段核对通过；唯一差异是 `route` 默认参数只保留在内部声明；
- 拆分前后 57/57 个直接路由路径比较保持一致；
- `tdx-tool` 编译链接通过；
- 内置 `serve --self-test` 通过：149 个功能、银行板块 1 个/成员 42 个、单票板块
  34 个、MACD 匹配 5 个、网页资源 3 个；
- 新二进制在临时端口 18766 启动，health/features/openapi/homepage/invalid-market
  五项契约 5/5 通过并按 `--max-requests` 自动退出；正式 8765 服务未改动；
- 契约报告保存在 `output/server-modularization-contracts.json`，结构证据保存在
  `output/server-modularization.json`。

本轮是等价重组，没有修改共享解析语义、传输协议或 API schema，因此依据增量验证规则
未运行完整 CTest 或完整 API 契约套件。当前目录不是 Git 工作树，未执行
`git diff --check`。
