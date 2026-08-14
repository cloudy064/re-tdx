# 原生 C++ 限售解禁链路模块化

日期：2026-08-11

## 问题

原 909 行的 `unlocks.cpp` 同时包含 DBLJJ 事件、股东明细、近期大比例解禁、月度压力四类
归一化，三路主资源和明细资源缓存、筛选汇总、查询文档以及 CLI。资源生命周期与业务投影
混在一个类实现文件中，月度市场聚合的修改也会触及个股明细流程。

## 调整

| 文件 | 职责 | 行数 |
|---|---|---:|
| `unlocks_internal.hpp` | 三项资源常量和共享内部端口 | 45 |
| `unlocks_support.cpp` | 时间、证券、JSN、本地镜像、筛选和事件汇总 | 274 |
| `unlocks_normalize.cpp` | 事件、股东、近期大额与月度压力归一化 | 217 |
| `unlocks_fetch.cpp` | 三类主缓存、明细缓存与失败负缓存 | 123 |
| `unlocks_query.cpp` | 月度/事件查询、筛选、明细装配和结果 schema | 270 |
| `unlocks_command.cpp` | CLI 参数、区块目录和输出 | 81 |

原根文件已移除。三项固定资源不再散落在抓取和归一化代码中；主资源缓存、明细缓存与负
缓存仍保留既有 TTL 和刷新行为，但与查询投影分离。

## 兼容性

- `tdx/unlocks.hpp` 的公开查询结构、四个归一化函数和服务接口未改变。
- `calendar`、`recent-large`、`monthly-pressure` 三视图及 CLI 参数未改变。
- 输出 schema 保持 `tdx-market-unlocks-native-v1`。

## 增量验证

1. 构建 `tdx-unlock-monthly-tests` 与 `tdx-tool`：通过。
2. 月度压力专项测试：通过。
3. 月度压力代表样本返回 3 个月 `live` 数据；3 项金额换算公式全部通过，0 项不一致，
   schema 为 `tdx-market-unlocks-native-v1`。

样本位于 `output/unlocks-refactor-monthly-pressure.json`。本轮未修改共享 JSN 解析器、传输层
或 schema，因此依照增量验证规则未运行完整 CTest。
