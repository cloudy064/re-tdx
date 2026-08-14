# 原生 C++ 因子查询分派模块化

日期：2026-08-11

## 动机

`native/src/factors.cpp` 是审计里单函数集中度最高的生产文件：595 行中
`FactorService::query` 独占 571 行（96%），与治理前的 `formula_render_events.cpp`
（572/589）是同一种病灶。因子族在早前一轮已经拆出 support / normalize / breadth /
snapshot / commands 五个兄弟单元，剩下的根文件几乎就是这一个函数。

该函数并非线性流程，而是一个**视图分派器**：先做参数校验与缓存探测，然后按视图落入四条
互不相干的分支，每条分支自行完成抓取、归一、过滤和 20 余个字段的响应组装。十个视图共用
这一条函数体，读任何一条分支都要跳过其余三条。

| 原始区间 | 行数 | 内容 |
| --- | --- | --- |
| 24-63 | 40 | 参数归一与校验 |
| 65-77 | 13 | 新鲜缓存探测 |
| 79-195 | 117 | `pattern-matrix` / `standard-matrix` 关系矩阵 |
| 197-278 | 82 | `overview` 市场广度 |
| 280-436 | 157 | `security` 个股反查 |
| 438-592 | 155 | 表格直取（catalog / patterns / members / pattern-members / dashboard / intraday-radar） |

## 拆分结果

四条分支成为 `FactorService` 的私有成员函数，因为它们都需要 `this`：递归调用公开的
`query()` 收集完整来源表，并读写 `root_`、`blocks_`、`cache_`。签名声明在公开头
`tdx/factors.hpp` 的 private 段，`ViewDefinition` 用前置声明引入，定义仍只在内部头可见。

| 单元 | 行数 | 职责 |
| --- | --- | --- |
| `factors.cpp` | 49 | 构造函数、`query()`：校验 → 缓存探测 → 四路分派 |
| `factors_query_support.cpp` | 147 | `validate_factor_query`、`fresh_cache_document`、`append_sources`、`apply_limit`、`filter_by_factor_field`、两个重试封装与共用的 `execute_once` |
| `factors_query_matrix.cpp` | 126 | `query_relation_matrix`：逐因子完整展开，失败降级为 partial |
| `factors_query_overview.cpp` | 99 | `query_overview`：三个来源视图折叠为广度文档并对账 |
| `factors_query_security.cpp` | 188 | `query_security`：个股反查，另分 `build_security_record` 与 `security_methodology` |
| `factors_query_table.cpp` | 171 | `query_table`：单请求路径，另分 `select_factor_from_catalog` 与 `table_methodology`，也是唯一能回退陈旧缓存的分支 |

内部头 `factors_internal.hpp` 从 68 行增到 120 行，新增三个常量
（`kTqlexEndpoint`、`kTqlexSourceFile`、`kFetchAttempts`）与七个流水线声明。

原 595 行 / 1 文件变为 780 行 / 6 文件，最大单元 188 行。净增 185 行来自六份 include 与
命名空间样板、四个成员函数签名，以及把内联片段提为具名 helper 的声明成本。

## 顺带消掉的重复

原函数把同样的片段抄了多份，拆分时归并为共享 helper，数量逐一核对：

| 构造 | 原有份数 | 拆分后 |
| --- | --- | --- |
| 四键 cache 片段 | 4 | 1 个 `fresh_cache_document`，4 处调用 |
| `sources` 追加循环 | 7 | 1 个 `append_sources`，7 处调用 |
| 分页截断 resize | 3 | 1 个 `apply_limit`，3 处调用 |
| `field(record, "factor")` 过滤循环 | 2 | 1 个 `filter_by_factor_field` |
| 250ms 倍增重试循环 | 3 | 2 个封装共用 1 个 `execute_once` |
| `gp_gz_fsld.xml` 字面量 | 3 | 1 个常量 |
| TQLEX 端点字面量 | 3 | 1 个常量 |

三条重试循环合并为两个封装而不是一个，因为失败策略本就不同：矩阵展开与成分表前置抓取在
最后一次瞬时失败时抛出，主请求则要回退陈旧缓存。`fetch_factor_rows` 抛出，
`try_fetch_factor_rows` 返回 false 并交出错误文本，两者共用同一个请求形状。

## 保持不变的语义

- `Error` 只携带消息（继承 `std::runtime_error` 构造函数），因此
  `try_fetch_factor_rows` 返回 false 后由调用方 `throw Error(upstream_error)` 重建，
  类型与 `what()` 都与原先的裸 `throw` 一致。
- 陈旧缓存回退原先靠分派前取得的 `cached` 迭代器；现在表格单元在需要时自行
  `cache_.find(key)`。表格路径不递归，两点之间没有任何人改动该键，结果相同。
- 响应字段插入顺序即输出顺序，四条分支各自的键序逐字保留，包括 `overview` 独有的
  `cooccurrence` / `signal_count_distribution` / `safety_distribution` /
  `unresolved_factor_names` 四个尾部字段。
- 公开 schema `tdx-factors-native-v1` 未变，`--help` 未动。
- 组合视图必须先于表格路径分派：它们通过公开 `query()` 递归取完整来源表，顺序颠倒会让
  `standard-matrix` 落进单请求分支。根文件的分派顺序保留了这一点并加注说明。

## 验证

拷贝 `native/` 到 `build/equiv-src`，还原 `factors.cpp.removed`，从 CMakeLists 摘掉五条
`src/factors_query_*` 条目，另建 `build/equiv-baseline` 得到拆分前的 `tdx-tool.exe`。

### 校验拒绝路径（离线、确定性）

`validate_factor_query` 是从原函数搬走的部分，23 条拒绝路径两侧输出逐字一致，包含视图
大小写与空白归一（`--view '  CaTaLoG  '`）、六位数字校验、以及 `include_patterns`、
`--quotes` 的适用范围。

### 文档样本

输出文件内含上游 GBK 字节，因此比对按 latin-1（字节一一映射）解码，只归一 `generated_at`。

| 样本 | 分派分支 | 结果 |
| --- | --- | --- |
| `catalog` | table | 逐字节一致（6,347 B） |
| `patterns` | table | 逐字节一致（6,266 B） |
| `intraday-radar` | table | 逐字节一致（5,320 B） |
| `security` | security | 去掉 `generated_at` 后完全一致 |
| `members --factor-id 1` | table + 目录前置 | 仅 `price` / `change_pct` 相异 |
| `dashboard` | table | 仅 `price` / `change_pct` / `ten_day_change_pct` 相异 |
| `standard-matrix` | matrix | 3,431 处相异，全部落在 `records[].members[].{price,change_pct}` 与 3 个 `today_return_pct` |
| `overview` | overview | 38 处相异，全部为实时行情派生的均值字段 |

### 行情漂移不是重构差异

为把"相异"归因清楚，用**同一个基线二进制**连跑两次建立漂移下界：`members` 21 处、
`dashboard` 19 处相异，字段集合是基线-拆分差异的**严格超集**，还额外包含 `code`、`name`、
`security_id`、`selected_factors`、`row_count` 等成分变动字段。换言之，基线与拆分之间
出现差异的每一个字段，在基线自比时也会差；反之不成立。今日（2026-08-11）盘中报价在
两次运行的间隔内本就会变。

`overview` 一侧另行逐段核对，`schema`、`availability`、`view`、`view_title`、
`parameters`、`methodology`、`counts`、`signal_count_distribution`、
`safety_distribution`、`unresolved_factor_names`、`cache`、`warnings` 十二个结构段全等，
顶层键序一致——`counts` 全等意味着成分与覆盖分母完全对上，广度计算未受影响。

`standard-matrix` 的 152 处 "类型" 差异经查是 `19.98 → 20`、`93.2 → 93` 这类实时值恰好
落在整数上，Python 解析成 `int` 而非 `float`，并非结构差异。

### 字面量核对

`pattern-matrix` 与 `standard-matrix` 共用同一单元，仅靠三处三元表达式区分，而样本只覆盖了
`standard` 一侧。为补上这个缺口，做了两项静态核对：三处三元表达式的极性与整段
methodology 与原文逐字相同（忽略缩进）；并用字符扫描器（不能用正则——TQLEX 端点字面量里
的 `//` 会让注释剥除破坏后续所有引号配对）比对全部字符串字面量的多重集合，缺失的 25 处
全部是上表中的去重归并，数量与折叠掉的调用点一一对应，新增的 7 处是各单元重新计算自身
视图布尔量所需的视图名。没有任何一句 methodology、警告或错误消息在搬迁中丢失或改写。

### 测试

`tdx-factors-tests` 通过，`tdx-theme-library-tests` 通过，`tdx_native` 与 `tdx-tool`
正常构建。`server_state.cpp` 也持有 `FactorService`（HTTP 路径），随库一并重新编译通过。
六个单元首次编译即全部通过语法检查，无需返工。

## 归档

原文件保留为 `native/src/factors.cpp.removed`（595 行），与同族归档一致。
