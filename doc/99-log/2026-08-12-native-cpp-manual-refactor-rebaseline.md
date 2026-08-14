# 手工重构后的原生 C++ 代码基线

## 目的

本记录把 2026-08-12 手工重构后的工作区视为唯一代码基线。后续功能补齐不再按
旧的大型单文件结构定位，也不会把 `.removed` 文件当作可执行源码。

## 规模与构建闭合

- `native/src` 与 `native/include` 当前共有 740 个生产 `.cpp`、257 个头文件；其中
  726 个来自手工重构基线，后续按相同边界新增独立 `tpool_callbacks.cpp`、
  `tpool_history.cpp`、`tpool_history_export.cpp`、`tpool_history_text.cpp` 和
  `tpool_history_command.cpp`，以及 `runtime_topology.cpp`、
  `runtime_topology_config.cpp`、`runtime_topology_watch.cpp`、
  `runtime_topology_command.cpp`，以及证据分级服务器角色目录
  `session_endpoint_roles.cpp`，以及独立的历史证券解析与命令单元
  `historical_securities.cpp`、`historical_securities_command.cpp`，以及独立的
  本地信号解析/命令单元 `local_signals.cpp`、`local_signals_command.cpp`。生产 `.cpp`
  共 142,362 行，最大单元为 `market/level2.cpp` 的 727 行，没有超过
  5,000 行软阈值的生产单元。
- `native/tests` 当前有 131 个 C++/头文件，共 24,387 行；最大测试单元为 1,530 行，
  没有超过 2,500 行软阈值的测试单元。
- 从 `native/CMakeLists.txt` 提取的 739 个静态库生产源与磁盘逐一相等；另一个
  `server/main.cpp` 由 `tdx-tool` 可执行目标直接编译。未编译源为 0，缺失源为 0。
- 十个 `.removed` 文件仅作为旧实现证据保留；CMake、生产源码和公共头文件均不引用
  它们。

## 当前调用边界

### 命令

`server/main.cpp` 只负责参数入口和错误边界。`registry/registry.cpp` 按固定顺序调用
11 个领域 appender，组装 `CommandSpec` 表，再把命令交给各领域 handler。当前共有
158 个唯一命令；相较旧 `registry.cpp.removed` 的 149 个命令没有删除，只新增：

- `market fund-reference`
- `market hk-actions`
- `market hk-finance`
- `market hot-history`
- `market historical-securities`
- `market index-events`
- `pool history`
- `recon runtime-topology`
- `formulas local-signals`

`--help`、`/api/v1/features` 和 OpenAPI 路径种子共用该注册表，固定 appender 顺序是
公共展示顺序的一部分。

### HTTP 服务

`server.cpp` 负责套接字生命周期，request/response/query/state 各自独立；
`server_route.cpp` 只做顶层公式、TPool、云端和静态资源分派，市场功能继续进入
`route_registered_market_api`，再由 catalog 与 research/institution/events/realtime
等领域单元处理。OpenAPI 先从命令注册表播种，再补充非命令资源和 POST 契约。

### 公式解释器

当前公式链路分为：

1. `formula_language`：词法、语法树和上下文调用收集；
2. `formula_analysis_*`：源码、语义、库和上下文能力审计；
3. `formula_context_*` / `formula_environment_*`：自动依赖计划以及本地、行情、财务、
   港股、板块和跨证券上下文；
4. `formula_runtime` / `formula_functions_*`：AST 数值与字符串执行；
5. `formula_render_*`：展示语句到稀疏绘图 IR；
6. scan、watch、strategy、backtest 作为独立编排层复用同一解释器。

### TPool

TPool 当前按 XML 检查、内置字段、规则、历史窗口、跨证券排名、求值、flow 调度、
投影、状态、差分、动作策略和命令拆分。本轮新增 `tpool_callbacks.cpp`，专门描述
`TPool_RegisterCallBack` 的三个宿主槽，不把静态协议常量重新塞回解析或动作控制流。
`tpool_history.cpp`、`tpool_history_export.cpp`、`tpool_history_text.cpp` 与
`tpool_history_command.cpp` 又把每日 XML 解析/发现、原版文本布局、原版文本反向解析
和 CLI 参数编排分开。文本解析保留进入时间的分钟精度与最高日期的月份日期精度，
不会猜测原格式未提供的秒或年份。所有宿主 UI、文件和回调副作用仍保持只读/计划态。

### Recon

Recon 已按公式 foundation/runtime/render/workflow 和市场 data/research/corporate 拆成
具名 validator 责任链。失败可定位到语义域，不再由一个巨型测试或契约分派单元承担。
新的 `runtime_topology.cpp` 只负责 Windows 进程、模块、TCP 快照及纯文档差分，
`runtime_topology_config.cpp` 负责公开服务器端点关联，`runtime_topology_watch.cpp`
负责多帧变化压缩，`runtime_topology_command.cpp` 单独负责参数、定时采样、基线和
输出路径，不把平台协议或 connect.cfg 解析塞回命令编排。

## 仍存在的结构性边界

- 生产源码仍汇入一个 `tdx_native` 静态库；模块隔离主要依赖目录、头文件和命名空间，
  还不是链接目标级隔离。
- `server_route.cpp` 的顶层非市场路由仍是线性分支；市场路由已有注册分派，后续新增
  顶层职责时应优先采用类型化路由表。
- 739 个静态库源文件仍由 CMake 显式长清单维护。它可审计但编辑成本高；若继续拆目标，需
  同时保护 Windows 静态链接和现有单可执行文件交付方式。

## 本轮聚焦验证

- CMake 静态库源清单与磁盘对应生产 `.cpp`：739/739，另有直接编译的 main，差异 0；
- 命令注册：158 个、唯一 158 个，旧命令缺失 0；
- 增量构建：`tdx-tpool-tests`、`tdx-recon-contract-tests`、`tdx-tool` 通过；
- 专项测试：TPool 与 recon `formula-evaluation` 域通过；
- 临时 18876 上 4 个 API 契约通过，正式 8765 服务未改动；
- TPool 历史增量：专项测试通过，双文件 CLI 样本为 2 文件/3 记录/3 证券；临时
  18877 上 `health/features/openapi` 3/3，正式 8765 仍为 PID 24096；
- 原版 GBK 历史文本增量：状态/入池样例均成功，输出碰撞拒绝通过；临时 18878
  `health/features/openapi` 3/3，正式服务未改动；
- 原版文本反向解析增量：GBK 状态/入池样例逐字节往返，UTF-8 样例自动识别，
  TPool 专项测试通过；
- 运行拓扑增量：专项测试、两个真实 TdxW 进程快照与零变化基线比较通过；临时
  18881 的 `health/features/openapi` 为 3/3，最终 `serve --self-test` 通过
  （156 个功能），正式 8765 仍为 PID 24096；
- 运行拓扑观察增量：一秒真实观察获得 6 帧/5 次稳定比较/0 误报，观察文档可链式
  作为基线；PID 复用、变化压缩和三类参数拒绝测试通过。该增量不改 API，未重复
  启动服务契约；
- 端点关联增量：复用现有 session-config 公开目录，当前 6 组/69 个端点；精确匹配、
  未命中、跨帧 section 保留、显式根拒绝和私密内容不泄露测试通过。该增量仍为
  CLI-only，未重复服务契约；
- 进程树增量：默认递归纳入目标的存活子进程，真实短生命周期子进程验证根 PID、
  深度、父镜像和 `--no-children`；当前两个 TdxW 都是父 PID 已退出的直接根，未发现
  存活子进程。该增量仍不读取进程内存、不改 API；
- 未运行完整 CTest 或完整 API 契约集。

历史证券增量随后新增一个隔离解析器、命令和 API：相关 3 个 CTest 通过；快速
HTTP 契约 9/9；按 API 边界规则执行 full 契约为 211/225，其中新增本地契约全部
通过，14 项失败均位于既有实时数据或数据基数契约。完整 CTest 仍未重复运行。

本地信号增量随后新增隔离解析器、命令和公式宿主绑定：受影响目标构建通过，公式
引擎与 recon 专项 CTest 2/2，快速 API 9/9；full 契约仍为 211/225，失败集合与
历史基线逐项相同。新增文件最大 339 行，没有接近生产软阈值。
