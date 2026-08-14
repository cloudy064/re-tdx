# 原生 C++ 市场资料库 Handler 模块化

日期：2026-08-10

## 拆分结果

`server_market.cpp` 从约 3,000 行降至 2,224 行。新增：

- `server_market_catalog.cpp`（777 行）：集中承载 33 个资料库、专题和公司事件接口；
- `server_market_catalog_internal.hpp`（38 行）：声明统一的
  `Json(const ApiState&, const RequestTarget&)` Handler 边界；
- CMake 将 catalog 作为独立翻译单元编译，原 `ApiJsonRoute` 注册表继续引用相同
  函数名，不改变路径、错误策略或响应 schema。

迁出的接口包括证券目录、ETF 流向、可转债/债券资料、经济指标、战略主题、统一
主题库、财经日历、员工与港股事件、特殊事项、场内基金、精选数据、基金统计、
专项经营指标、公司变更、财务筛选/洞察、GDR、股价表现、重大合同、事件影响、
全球表现、股东信号、近期关注、专利统计、大盘影响因素和基准分析等。

同时修复 `server_state_internal.hpp` 的 include-order 隐患：该头文件直接持有
`FormulaIconSprite`，现在显式包含其定义所在的 `tdx/formulas.hpp`，不再依赖调用方
碰巧先包含该头。

## 验证

- `tdx-tool` 和 `tdx-recon-contract-tests` 增量编译、链接通过；
- `tdx-recon-contract-tests` 通过；
- 独立候选服务运行于 `127.0.0.1:18767`，正式 8765 服务未触碰；
- 三个迁移接口实际路由成功：
  - `/api/v1/market/securities`：HTTP 200；
  - `/api/v1/market/calendar`：HTTP 200；
  - `/api/v1/market/overview-factors`：HTTP 200；
- 三个响应均为 JSON，候选服务已停止，端口无残留监听，stderr 为空；
- 启动日志：`output/server-market-catalog-v41.out.log`；
- 未运行完整 CTest 或全量 API 契约，因为公共 schema、传输、缓存与路由路径未修改。

## 后续边界

`server_market.cpp` 仍包含实时行情/衍生品、资金与机构、量化分析三类 Handler 及路由表。
后续应继续按这三类拆分，并让 `server_market.cpp` 最终只保留共享错误分类和
`ApiJsonRoute` 注册表。
