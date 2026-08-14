# 原生 C++ 机构分析模块化

日期：2026-08-11

## 调整

原 879 行 `institution_analysis.cpp` 同时包含 25 个视图、12 种布局、字段契约、归一化、
特殊排序/汇总、文档组合、缓存服务和 CLI。现拆分为：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `institution_analysis_internal.hpp` | 25 项编译期视图目录、12 种布局和内部端口 | 90 |
| `institution_analysis_catalog.cpp` | 视图查找、布局名称、字段契约和目录文档 | 171 |
| `institution_analysis_support.cpp` | JSON、数值、市场与来源支持 | 80 |
| `institution_analysis_normalize.cpp` | 12 类布局数据归一化 | 193 |
| `institution_analysis_summary.cpp` | 筛选、特殊排序、摘要和分页 section | 191 |
| `institution_analysis_compose.cpp` | 公开 schema 文档组合 | 116 |
| `institution_analysis_service.cpp` | 批量资源缓存与查询服务 | 64 |
| `institution_analysis_command.cpp` | CLI 参数、区块目录和输出 | 76 |

原根文件已移除；公开头文件、25 个视图和 `tdx-institution-analysis-native-v1` schema 未变。

## 增量验证

- `tdx-institution-analysis-tests` 与 `tdx-tool` 构建通过，专项测试通过。
- `national-team` 真实样本匹配 191 条、返回 3 条；191 项组合比例公式均通过、0 项不一致，
  来源非陈旧。
- 样本：`output/institution-analysis-refactor-national-team.json`。

未修改共享 JSN 解析器、传输或 schema，因此未运行完整 CTest。
