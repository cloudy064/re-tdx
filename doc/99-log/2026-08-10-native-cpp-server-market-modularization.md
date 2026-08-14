# 原生 C++ 市场 API 控制器完整模块化

日期：2026-08-10

## 目标

消除 `server_market.cpp` 同时承担百余条路由、实时行情、资料库、研究、事件、
量化分析和机构数据实现的单文件控制器，按稳定业务边界拆分，同时保持 URL、响应
schema、缓存语义和错误分类不变。

## 结果

`server_market.cpp` 从拆分前约 3,000 行降至 325 行；它现在只保留：

- TQLEX/PBRPC 通用请求适配；
- `UpstreamErrorPolicy` 错误分类策略；
- 102 条 `constexpr ApiJsonRoute` 路由注册表；
- 编译期路由数量/路径唯一性检查和运行期 `unordered_map` 索引。

业务实现按统一 `Json(const ApiState&, const RequestTarget&)` Handler 契约拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `server_market_catalog.cpp` | 814 | 资料库、专题、公司事件目录 |
| `server_market_realtime.cpp` | 539 | 实时快照、深度、涨速、扩展市场、期权、竞价和成交 |
| `server_market_events.cpp` | 562 | 市场全景、涨停/轮动信号、公告和公司行为 |
| `server_market_analytics.cpp` | 464 | 杠杆、因子、技术信号、估值、异常和基金分析 |
| `server_market_research.cpp` | 264 | 资金、龙虎榜、研究、路演、解禁和大宗交易 |
| `server_market_institution.cpp` | 191 | 机构龙虎、评级、外资预警和持有人 |

每个模块都有对应 `*_internal.hpp`，路由主文件只依赖声明，不再依赖各业务实现头。
通用布尔查询参数解析 `query_bool` 同时移到基础请求实现 `server.cpp`，避免实时模块
反向承载共享能力。主文件直接包含的头文件由近百个缩减到实际需要的 14 个。

这里没有为了“使用设计模式”而引入无状态子类：这些 Handler 没有对象生命周期或
可替换状态，类型化函数指针注册表就是更轻量的 Strategy/Registry 边界。真正有状态的
业务仍由 `ApiState` 中的 Service 对象负责。

## 验证

- `tdx-tool` 与 `tdx-recon-contract-tests` 增量编译、链接通过；
- `tdx-recon-contract-tests` 通过；
- 实时/衍生品候选服务运行于 `127.0.0.1:18768`，以下接口均为 HTTP 200 JSON：
  - `/api/v1/market/snapshot`；
  - `/api/v1/market/instruments`；
  - `/api/v1/market/options`；
- 完整模块候选服务运行于 `127.0.0.1:18769`，四类迁移接口均为 HTTP 200 JSON：
  - `/api/v1/market/consensus`；
  - `/api/v1/market/panorama`；
  - `/api/v1/market/fund-analytics`；
  - `/api/v1/market/institution-lhb`；
- 两个候选服务均由 `--max-requests` 自动退出，无端口残留，stderr 为空；正式 8765
  服务未触碰；
- 证据日志：`output/server-market-realtime-v42.out.log`、
  `output/server-market-realtime-v42.err.log`、
  `output/server-market-modules-v43.out.log`、
  `output/server-market-modules-v43.err.log`。

本轮未运行完整 CTest/全量 API 巡检：公共路由、schema、传输和缓存协议没有变化，
聚焦契约测试与 7 个代表性实时请求已覆盖迁移风险。

## 后续热点

当前市场服务入口已无千行控制器。剩余较大的文件主要是独立领域实现或契约目录，
其中 `recon_contract_market_corporate.cpp`、`disclosures.cpp`、`jsn_variants.cpp` 和
`recon_contract_market_data.cpp` 仍超过 2,000 行，可继续按“契约目录/解析/组合/输出”
边界拆分，而不再向路由层堆积逻辑。
