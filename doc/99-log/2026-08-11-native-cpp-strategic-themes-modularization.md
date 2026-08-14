# 原生 C++ 战略主题层级链路模块化

日期：2026-08-11

## 目标

拆解 663 行的 `strategic_themes.cpp`。原文件混合 26 类资源常量、JSN 字段转换、证券成员
解析、主表与明细归一化、跨分类主题合并、两类缓存、五种 view、排序、筛选、响应组合和
CLI；固定目录使用运行期字符串对象，缺少重复约束。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `strategic_themes_catalog.cpp` | 140 | 26 类资源、五 view、三排序目录与排序策略 |
| `strategic_themes_support.cpp` | 179 | JSN 字段、证券成员、内容检索、时间与参数转换 |
| `strategic_themes_normalize_master.cpp` | 53 | 分类主题及主表成员归一化 |
| `strategic_themes_normalize_detail.cpp` | 39 | 逐股入选逻辑和参考价格归一化 |
| `strategic_themes_query_plan.cpp` | 42 | view、排序、证券条件和查询范围校验 |
| `strategic_themes_fetch_master.cpp` | 106 | 26 路主资源、跨分类合并与主缓存 |
| `strategic_themes_fetch_detail.cpp` | 30 | 动态主题明细资源与独立缓存 |
| `strategic_themes_service.cpp` | 190 | 视图投影、筛选、分页和响应组合 |
| `strategic_themes_command.cpp` | 67 | CLI 参数与输出 |

26 个类别定义成为编译期唯一事实来源，block id、配置名和资源路径均检查唯一；五种 view 与
三种排序也采用枚举目录并检查重复。公开 `StrategicThemeCategory`、服务构造函数、命令参数和
JSON schema 保持不变。主目录与动态 `zttzty/<id>.jsn` 明细继续使用独立 TTL 缓存。

## 增量验证

- `tdx-strategic-themes-tests` 与 `tdx-tool` 构建通过；
- 专项测试覆盖 26 类目录、主题 id、原始/去重成员、互联网主题命名空间、逐股逻辑和重复拒绝；
- 真实 catalog 返回 26 类、598 个唯一主题、655 条分类归属和 26 路来源；
- 主表声明计数不一致为 0，重复主表成员为 9，抽取 3 个主题且无错误；
- 样本保存在 `output/strategic-themes-refactor-sample.json`，schema 保持
  `tdx-market-strategic-themes-native-v1`；
- 未修改共享 JSN 解析、HTTP 路由或 schema，按增量验证规则未运行完整 CTest。

## 后续候选

`level2.cpp` 继续搁置，共享 `jsn.cpp` 需要宽回归；下一低风险候选为 661 行的
`native/src/shareholder_signals.cpp`。
