# 原生 C++ 股份回购链路模块化与本地 JSN 配置

日期：2026-08-11

## 目标

拆解 676 行的 `repurchases.cpp`。原文件混合核心资源、年度段、四类归一化、三级缓存、
视图分派、动态明细和 CLI；视图仍依赖字符串集合与条件链。服务端虽有全局 `--jsn-root`，
回购服务此前没有接入，已有本地下载数据仍会强制访问 7709。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `repurchases_catalog.cpp` | 80 | 核心资源、年度段和视图类型目录 |
| `repurchases_support.cpp` | 231 | 市场/数值支持、本地 JSN、筛选与汇总 |
| `repurchases_normalize_plans.cpp` | 57 | A/B 股回购计划归一化 |
| `repurchases_normalize_statistics.cpp` | 62 | 月度与年度统计归一化 |
| `repurchases_normalize_hk.cpp` | 42 | 港股回购归一化 |
| `repurchases_service_fetch.cpp` | 119 | 主资源、明细及失败缓存与本地/远端选择 |
| `repurchases_service_query.cpp` | 199 | 查询校验、枚举视图策略和响应组合 |
| `repurchases_command.cpp` | 69 | CLI 参数与输出 |

三项核心资源、三类年度段、四种视图和三个动态明细命名空间具有编译期唯一性检查。查询先将
view 解析为 `ViewKind`，抓取先将资源解析为 `CoreResourceRole`，不再在控制流中重复比较
资源路径。

`RepurchaseService` 新增向后兼容的可选 JSN 根参数；服务端全局 `--jsn-root` 和回购 CLI 的
新 `--jsn-root` 均会传入该配置。存在本地文件时直接解析，缺失时仍可回退远端。每项本地来源
明确返回 `attempts=0`、`stale=false`、`age_seconds=0` 和空 `upstream_error`，与现有来源韧性
契约兼容。

## 增量验证

- `tdx-repurchases-tests` 与 `tdx-tool` 构建、测试通过；
- 六项本地真实资源返回 1,220 条计划、13 个月度点、5 个 A 股年度点、167 只港股；
- 样本六个来源全部为 `local-jsn:`，availability 为 `live`，无明细错误；
- 临时服务上的 `stock-repurchases-live` 单项契约 13/13 断言通过，响应约 349 ms；
- 临时服务按单请求上限退出，正式 8765 服务未触碰；
- 初次远端样例记录到三个 7709 端点同时断连，随后使用新本地配置完成确定性验证；
- 未改变共享 JSN 解析器、远端传输或公开 schema，未运行完整 CTest。

## 后续候选

`level2.cpp` 继续搁置，共享 `jsn.cpp` 留待宽回归；下一低风险候选为 671 行的
`native/src/equity_valuation.cpp`。
