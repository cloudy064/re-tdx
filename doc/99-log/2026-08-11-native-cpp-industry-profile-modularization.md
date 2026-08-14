# 原生 C++ 行业画像链路模块化

日期：2026-08-11

## 目标

拆解 744 行的 `industry_profile.cpp`。原文件同时负责四季度资源目录、JSON 字段换算、行业/
证券归一化、研究行业拓扑、主表和详情缓存、证券/行业查询以及 CLI。三个核心归一化断言也藏在
通用 `native_tests.cpp` 中，验证成本偏高。

## 落地结果

原根文件已移除，公开 `tdx/industry_profile.hpp` API 和
`tdx-industry-profile-native-v1` schema 保持不变：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `industry_profile_catalog.cpp` | 50 | 四个持仓周期与股东主资源类型目录 |
| `industry_profile_support.cpp` | 168 | JSON、数值、市场、证券、行业与来源投影 |
| `industry_profile_normalize_holdings.cpp` | 91 | 持仓主表和历史明细归一化 |
| `industry_profile_normalize_shareholders.cpp` | 89 | 行业股东画像和逐股画像归一化 |
| `industry_profile_topology.cpp` | 43 | 研究行业父子关系与证券成员索引 |
| `industry_profile_service_fetch.cpp` | 113 | 五项主资源、详情缓存和树文档构建 |
| `industry_profile_service_query.cpp` | 208 | 行业/证券选择、详情容错、汇总和缓存健康 |
| `industry_profile_command.cpp` | 80 | CLI 参数和输出 |

`IndustryHoldingPeriod` 将周期键、中文标签和资源绑定在一起并编译期检查重复项。所有内部
辅助 API 放入 `tdx::detail::industry_profile`，避免与港股事件等模块的同名 JSON/路径
工具发生链接冲突。三个命令默认值集中管理，没有增加继承层。

## 测试提速

行业持仓、行业股东和逐股股东画像断言已从通用测试迁到 114 行的
`tdx-industry-profile-tests`；`native_tests.cpp` 从 1,396 行降至 1,318 行。后续行业
画像变更只需运行该专项测试。

## 增量验证

- `tdx-industry-profile-tests`、`tdx-native-tests` 和 `tdx-tool` 增量构建通过；
- 专项测试覆盖持仓环比与持股比例、股东画像同比、北京市场 44 归一化及日均变化；
- 平安银行真实样本为 `live`：467 个行业、三级行业路径、10 个持仓历史点、1 个股东行业、
  1 条逐股画像、7 个来源且 0 个详情错误；
- `calendar-resilience-web` 定向 API 契约通过；
- 本轮未运行完整 CTest。

## 后续候选

当前最大生产实现为 742 行的 `native/src/financial_insights.cpp`。
