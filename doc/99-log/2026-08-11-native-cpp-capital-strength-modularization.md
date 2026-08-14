# 原生 C++ DDX 资金强度链路模块化

日期：2026-08-11

## 目标

拆解 669 行的 `capital_strength.cpp`。原文件混合五周期资源映射、JSN 字段读取、证券投影、
榜单归一化、跨周期共振、十三种排序、查询校验、缓存抓取、响应组合和 CLI；周期别名、
view 默认值与排序能力主要依赖字符串条件链。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `capital_strength_catalog.cpp` | 151 | 周期、别名、view、排序与 JSN 字段目录 |
| `capital_strength_support.cpp` | 141 | JSN 标量、市场、证券、时间和参数转换 |
| `capital_strength_normalize.cpp` | 37 | 单周期榜单归一化 |
| `capital_strength_confluence.cpp` | 99 | 跨周期证券聚合与分布摘要 |
| `capital_strength_sort.cpp` | 52 | 类型化排序指标与稳定排序 |
| `capital_strength_summary.cpp` | 60 | 单周期来源和资金方向摘要 |
| `capital_strength_query_plan.cpp` | 71 | 查询约束、默认值、周期选择与过滤 |
| `capital_strength_service.cpp` | 158 | 资源缓存、视图投影和响应组合 |
| `capital_strength_command.cpp` | 75 | CLI 参数与输出 |

`PeriodSpec` 统一管理五项资源及其收益、总净流入和主力净流入字段；`ViewSpec` 统一管理四种
view 的默认排序、顺序和周期范围；`SortSpec` 管理十三项排序及允许视图。五个周期、二十个
周期别名、四种 view 和十三项排序均有编译期唯一性检查，并额外检查别名目标及 view 默认
排序有效。公开 API、命令参数与 schema 保持不变。

## 增量验证

- `tdx-capital-strength-tests` 与 `tdx-tool` 构建通过；
- 专项测试继续覆盖五周期归一化、负资金值、证券名称解析、排序、跨周期共振和非法周期；
- 真实 5 日榜通过 `list/func_qszj101_1.jsn` 返回固定 100 行，来源 DDX 顺序为降序，抽取
  3 行，availability 为 `live`；
- 样本保存在 `output/capital-strength-refactor-sample.json`，schema 保持
  `tdx-market-capital-strength-native-v1`；
- 未修改共享 JSN 解析、HTTP 路由或 schema，按增量验证规则未运行完整 CTest。

## 后续候选

`level2.cpp` 继续搁置，共享 `jsn.cpp` 需要宽回归；下一低风险候选为 663 行的
`native/src/strategic_themes.cpp`。
