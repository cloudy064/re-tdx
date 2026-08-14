# 原生 C++ 市场研究链路模块化

日期：2026-08-11

## 目标

拆解 685 行的 `research.cpp`。原文件同时维护九类研究资源、市场/证券映射、三类字段归一化、
两级缓存、动态明细分派、查询响应和 CLI；研究断言还夹在通用 `native_tests.cpp` 中。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `research_catalog.cpp` | 111 | 九类主资源和四种明细命名空间类型目录 |
| `research_support.cpp` | 208 | 市场、证券、JSON、文本和筛选公共支持 |
| `research_normalize_master.cpp` | 98 | 证券、行业、机构主表归一化 |
| `research_normalize_detail.cpp` | 83 | 活动、监管和关联证券明细归一化 |
| `research_service_fetch.cpp` | 76 | 主目录与动态明细缓存抓取 |
| `research_service_query.cpp` | 184 | 查询校验、关系组合、明细策略和响应汇总 |
| `research_command.cpp` | 70 | CLI 参数、安装上下文和输出 |

固定研究分类不再由运行时字符串条件散落控制：九个分类 ID/资源以及四个动态命名空间具有
编译期唯一性检查，明细分派先解析为 `DetailKind`，再进入三种明确策略。公开
`ResearchCategorySpec`、`ResearchQuery`、`ResearchService` 和 schema 均保持不变。

测试方面新增 77 行的 `tdx-research-tests`，从 `native_tests.cpp` 迁出原三组断言并补充分类
目录唯一性校验；通用测试文件从 1,318 行降到 1,271 行。

## 增量验证

- `tdx-research-tests`、`tdx-native-tests` 和 `tdx-tool` 构建通过；
- 研究专项测试通过；
- 真实主目录样本返回 9 类、5,598 行、9 个来源，抽取 3 行，availability 为 `live`；
- 临时服务上的 `stock-research-live` 单项 HTTP 契约通过，耗时约 1.4 秒；
- 临时服务按单请求上限自动退出，正式 8765 服务未触碰；
- 未改变共享 JSN 解析/传输或公开 schema，未运行完整 CTest。

## 后续候选

`level2.cpp` 继续搁置，共享 `jsn.cpp` 留待宽回归门禁；下一低风险候选为 676 行的
`native/src/repurchases.cpp`。
