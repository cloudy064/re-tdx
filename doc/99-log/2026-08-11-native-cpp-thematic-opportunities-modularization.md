# 原生 C++ 题材机会链路模块化

日期：2026-08-11

## 目标

拆解 731 行的 `thematic_opportunities.cpp`。原文件同时维护五项 JSN 资源、分组字段差异、
证券身份投影、两类题材归一化、缓存抓取、六种查询视图、排序/分页和 CLI。

## 落地结果

原根文件已移除，公开 `tdx/thematic_opportunities.hpp` API 与
`tdx-market-thematic-opportunities-native-v1` schema 保持不变：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `thematic_opportunities_catalog.cpp` | 93 | 五项资源及视图、类型、排序、顺序目录 |
| `thematic_opportunities_support.cpp` | 215 | 字段、证券身份、成员解析、搜索、排序、分页和路径支持 |
| `thematic_opportunities_normalize_groups.cpp` | 122 | 行业/区域/遗留主题主表与明细归一化 |
| `thematic_opportunities_normalize_hype.cpp` | 70 | 已完成与进行中热点归一化 |
| `thematic_opportunities_service_fetch.cpp` | 102 | 多资源抓取、摘要、主/明细缓存 |
| `thematic_opportunities_service_query.cpp` | 174 | 六视图筛选、详情容错和响应组合 |
| `thematic_opportunities_command.cpp` | 89 | CLI 参数和输出 |

`ResourceDefinition` 将角色、资源、分组类型、显示名、主表字段和明细路径绑定，并在编译期
检查角色、路径和分组类型唯一性。服务抓取与分组归一化均从目录派生；视图、类型、分组排序
和顺序也由固定目录验证。内部 API 隔离在 `tdx::detail::thematic_opportunities`，CLI 的九项
默认值直接复用公开查询对象，消除第二套默认配置。

## 增量验证

- `tdx-thematic-opportunities-tests` 与 `tdx-tool` 增量构建通过；
- 专项测试覆盖行业/区域主表、成员去重、遗留主题语义错配、两类热点以及查询错误边界；
- 真实上游目录返回 15 个分组（行业 6、区域 8、遗留主题 1）和五项来源，抽取三条且
  availability 为 `live`；
- `research-signals` 定向 API 契约通过；
- 本轮未改变公开 schema、共享解析或传输，未运行完整 CTest。

## 后续候选

物理最大生产实现为已明确搁置的 727 行 `level2.cpp`。排除 L2 后，下一候选是 715 行的
`native/src/recon_contract_formula_foundation.cpp`。
