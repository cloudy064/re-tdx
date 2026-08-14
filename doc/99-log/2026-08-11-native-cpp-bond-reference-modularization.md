# 原生 C++ 债券参考资料链路模块化

日期：2026-08-11

## 目标

拆解 785 行的 `bond_reference.cpp`，集中管理评级、利率和品种资源，显式表达主表/投影身份
字段与金额单位差异，并分离归一化、缓存抓取、查询和 CLI。

## 落地结果

原根文件已移除，形成六个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `bond_reference_catalog.cpp` | 152 | 24 项主来源、20 项资源画像、8 组投影和 2 个对照表 |
| `bond_reference_support.cpp` | 127 | 字段、搜索、本地 JSN 和参数支持 |
| `bond_reference_normalize.cpp` | 191 | 债券身份、票息计划及金额单位投影 |
| `bond_reference_service_fetch.cpp` | 35 | 本地优先抓取与缓存 |
| `bond_reference_service_query.cpp` | 304 | 投影对账、过滤排序、汇总和响应 |
| `bond_reference_command.cpp` | 55 | CLI 参数与输出 |

24 项公开来源由编译期 `SourceLiteral` 目录生成，并同时检查资源路径及 group/bucket 唯一性。
原先散落在三个 `std::set` 中的资源路径改为 `ResourceProfile`，明确区分：元为单位的发行额、
亿元为单位的发行额、亿元为单位的存量余额和客户端合并表隐藏单位。

## 增量验证

- `tdx-bond-reference-tests` 与 `tdx-tool` 增量构建通过；
- 专项测试覆盖 24 项目录、评级、国债、可转债、政策性金融债和单位换算并通过；
- 政策性金融债本地样本返回 10 条主表、10 条投影且集合精确一致；
- 样本 `GM=40` 继续投影为 40 亿元与 4,000,000,000 元；
- `calendar-resilience-web` 定向契约通过，schema 保持
  `tdx-market-bond-reference-native-v1`；
- 未运行完整 CTest。

## 后续候选

当前最大生产实现为 780 行的 `native/src/commodity_links.cpp`。
