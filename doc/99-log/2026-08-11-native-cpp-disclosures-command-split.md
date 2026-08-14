# disclosures_command.cpp 拆分

日期：2026-08-11

## 动机

`native/src/data/disclosures_command.cpp` 共 531 行，其中 `command_market_disclosures` 单个函数占 507 行（集中度 95%）。该函数承担了 `tdx-tool market disclosures` 的全部职责：

1. 帮助文本输出（32 行）
2. 30 余个命令行选项的解析与 8 条互斥规则校验（66 行）
3. 证券universe 解析（--security / --securities / --backfill-input / --backfill-watchlist / --backfill-block）
4. 五种执行模式的完整实现：覆盖率审计、增量维护、批量回填预览、批量回填、主查询

五种模式之间存在大量重复代码：archive 路径解析在 4 处重复，state 路径解析在 2 处重复，listing-date DBF 加载在 2 处逐字重复（14 行 × 2），backfill state 初始化在 2 处逐字重复（15 行 × 2），fetcher/checkpoint lambda 在 2 处近乎重复（仅进度前缀不同）。

由于所有逻辑挤在一个函数内，选项校验规则无法独立测试——验证任一互斥规则都必须构造完整的命令行并触发真实的文件系统与网络访问。

## 拆分策略

按执行阶段与模式边界切分为 8 个编译单元，前端退化为纯调度器。

| 单元 | 行数 | 最大函数 | 集中度 | 职责 |
| --- | ---: | ---: | ---: | --- |
| `disclosures_command.cpp` | 44 | 29 | 65% | 帮助分流、调用解析、加载 blocks、按模式调度 |
| `disclosures_command_options.cpp` | 156 | 66 | 42% | 帮助文本、选项解析、互斥规则校验 |
| `disclosures_command_support.cpp` | 111 | 19 | 17% | 路径解析、周期合并、listing-date 加载、state 加载、fetcher/checkpoint 构造、options 文档 |
| `disclosures_command_universe.cpp` | 60 | 36 | 60% | 证券universe 解析与去重、`--max-securities` 上限 |
| `disclosures_command_audit.cpp` | 41 | 28 | 68% | `--audit-coverage` 模式 |
| `disclosures_command_maintain.cpp` | 139 | 85 | 61% | `--maintain` 模式（刷新归档 → 审计 → 回填 → 复审） |
| `disclosures_command_backfill.cpp` | 93 | 50 | 53% | `--dry-run` 预览与批量回填 |
| `disclosures_command_query.cpp` | 70 | 35 | 50% | 主查询与 `--archive` 观测合并 |

最大单函数从 507 行降至 85 行（`run_disclosure_maintenance`），集中度从 95% 降至 61%。

### 引入的共享抽象

`DisclosureCommandOptions` 结构体（声明于 `disclosures_internal.hpp`）承载全部解析结果。`parse_disclosure_command_options` 消费 `Args` 并在返回前完成校验，因此所有模式函数接收的都是已验证的选项。

去重后的共享函数：

- `resolve_disclosure_archive_path` / `resolve_disclosure_state_path` — 消除 4 处 + 2 处路径解析重复
- `collect_disclosure_audit_periods` — 合并 `--report-period`、重复 `--audit-period`、逗号分隔 `--audit-periods`，消除 2 处重复
- `load_disclosure_listing_dates` — 消除 2 处 14 行重复；缺失默认文件时返回 null Json，显式路径缺失时抛错
- `load_disclosure_backfill_state` — 消除 2 处 15 行重复，含 archive 身份绑定校验
- `make_disclosure_fetcher` / `make_disclosure_checkpoint` — 两处 lambda 合并，进度前缀参数化为 `"request"` / `"maintenance request"`
- `disclosure_backfill_options_document` — 消除 2 处 options 文档构造重复；维护模式额外附加 `completed_ttl_hours`

模式内的私有辅助函数收在各单元的匿名命名空间：`unfiltered_refresh_query`、`due_securities`、`empty_backfill_summary`（maintain），`archive_observation_query`（query），`append_all`（universe），`default_output_path`、`validate_disclosure_command_options`（options）。

## 保持不变的语义

- **选项解析顺序**：`Args::take_option` 会消费参数，顺序影响 `require_empty()` 的报错内容，因此逐字保留原始取值顺序。
- **校验时序**：8 条互斥规则全部在 `find_tdx_root` 之前触发，命令在非法参数下不会接触文件系统或网络。
- **JSON schema**：五种模式的 schema 字符串、字段集合与嵌套结构不变。`Json` 内部为 `std::map`，键序按字典序输出，与赋值顺序无关，因此字段赋值语句的重排不影响输出。
- **stdout 文本**：各模式的完成摘要与逐证券进度行（`[request N] SZ300503 fetch`、`[maintenance request N] ...`）逐字不变。
- **退出码**：批量回填与维护模式在 `failed > 0` 时返回 3，其余返回 0。
- **默认输出路径**：四种模式的默认 `--output` 由 `default_output_path` 集中给出，取值不变。

## 测试

`disclosures_command.cpp` 原先无法独立测试选项契约。拆分后校验全部发生在解析阶段（`find_tdx_root` 之前），因此可通过公开入口 `command_market_disclosures` 断言，无需 TDX 安装。

在 `native/tests/disclosures_tests.cpp` 中新增 `require_disclosure_rejection` 辅助与 9 条断言，覆盖全部互斥规则：

- `--audit-coverage` + `--maintain` 互斥
- 审计/维护模式拒绝 `--backfill-announcements`
- 批量输入要求 `--backfill-announcements`
- 审计/维护模式拒绝 `--dry-run`
- `--audit-as-of` 与 `--latest-periods` 要求审计或维护模式
- `--maintenance-refresh-hours` 要求 `--maintain`
- 批量回填要求 `--archive` 或 `--archive-path`
- `--backfill-announcements` 与 `--view schedule` 冲突

刻意只保留解析期断言。universe 解析阶段的三条错误（market/code 配对、非法证券、`--max-securities` 上限）发生在 `find_tdx_root` 之后，纳入测试会使套件依赖本机 TDX 安装——套件中其余 107 个目标均无此依赖。这三条路径改由 CLI 手工验证。

变异测试确认新断言有效：临时删除 `--maintenance-refresh-hours requires --maintain` 后，`tdx-disclosures-tests` 以退出码 1 失败。

## 验证

MSYS2 MinGW g++ 15.1.0，复用 `build/verify-mingw` 构建树（已缓存 ZLIB 与 make 路径）。

```
cmake -S native -B build/verify-mingw
cmake --build build/verify-mingw -j 8
ctest -j 4
```

- 8 个新单元与 7 个既有 disclosures 单元 `-fsyntax-only` 全部通过
- `libtdx_native.a` 全量编译链接通过（同时验证了 20 个模块子目录重组后的构建正确性）
- `ctest`：108/108 通过，用时 36.11 秒

CLI 行为核对：

- `--help` 输出 27 行，与拆分前逐字一致
- 8 条解析期校验错误逐条复现，文案不变
- universe 阶段 3 条错误逐条复现（market/code 配对、`invalid backfill security: nonsense`、`disclosure security universe has 37 securities; raise --max-securities explicitly`）
- `--dry-run` 预览输出 schema `tdx-disclosure-announcement-backfill-preview-native-v1`，字段与排序不变
- `--compact` 输出单行
- `--audit-coverage` 对真实归档运行成功，`listing_date_source` 正确附带 `source_path`（`C:\new_tdx\T0002\hq_cache\base.dbf`，7765 个上市日期，匹配 1 个），证明 `load_disclosure_listing_dates` 的指针传递路径正确
- 显式 `--listing-dates-path` 指向缺失文件时正确抛错
- 多周期审计（`--audit-period` + `--audit-periods` 混用）正确合并为 3 个周期，证明 `collect_disclosure_audit_periods` 的三源合并顺序正确

## 后续候选

- `formula_environment.cpp`（474 行，406 行函数，集中度 86%，无专用测试目标）
