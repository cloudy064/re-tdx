# 原生 C++ JSN 资源探索链路模块化

日期：2026-08-10

## 结果

生产文件 `jsn_variants.cpp` 从 2,286 行降至 313 行。固定资源目录、扫描器、报告生成
和命令编排不再共享一个匿名命名空间，按职责拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `jsn_variants.cpp` | 313 | CFG/XML 配置解析与资源模板清单 |
| `jsn_variants_catalog.cpp` | 490 | 固定明细模板映射与类型化命令覆盖目录 |
| `jsn_variants_scan.cpp` | 287 | 已下载 JSN 扫描、字段画像、指纹及模板匹配 |
| `jsn_variants_coverage.cpp` | 276 | 类型化覆盖率、缺口家族和价值排序报告 |
| `jsn_variants_discovery.cpp` | 333 | 快照基线、资源/字段增量发现报告 |
| `jsn_variants_candidates.cpp` | 480 | `refunit` 关系、动态路径候选和受限探测队列 |
| `jsn_variants_commands.cpp` | 268 | 三个 CLI 子命令的参数与输出编排 |
| `jsn_variants_internal.hpp` | 104 | 跨模块内部类型和最小共享函数边界 |

固定的 `detail_templates` 和 `coverage_bindings` 集中在 catalog 模块，新增资源或命令时
不必进入扫描、发现或候选生成实现。共享数据位于 `tdx::jsn_variant_detail`，公开
`tdx/jsn_variants.hpp`、CLI 参数、JSON schema 和业务算法均未改变。

这里没有为纯函数强行增加类层级：资源目录相当于配置映射，扫描、覆盖、发现和候选
生成保持独立策略模块，CLI 只负责适配输入输出。

## 等价性与验证

拆分期间保留了工作区内临时源快照，校验完成后已删除。目录、覆盖报告、发现报告、
候选生成和命令段的 7 个关键代码区段逐字符一致；另外只发生了内部结构体定义迁移和
跨翻译单元声明显式化。

- `tdx-jsn-variants-tests` 与 `tdx-tool` 增量编译、链接通过；
- `tdx-jsn-variants-tests` 通过；
- 代表性真实执行 `recon jsn-variants` 成功：622 个资源模板中 588 个已有类型化覆盖，
  扫描到 601 个下载文件；输出 schema 为
  `tdx-jsn-variant-coverage-native-v1`，证据保存在
  `output/jsn-variants-modularization.json`；
- 当前目录不是 Git 工作树，因此无法执行 `git diff --check`；机械区段校验、编译和
  专项运行均已通过。

未运行完整 CTest 或全量 API 契约；本轮没有修改传输、缓存、服务路由或公开 schema，
专项测试和一个真实数据样本足以覆盖此次纯结构调整。
