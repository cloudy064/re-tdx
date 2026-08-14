# native C++ 可维护性重构：bond_reference_service_query.cpp 拆分

日期：2026-08-11
范围：`native/src/bonds/bond_reference_service_query*`、`native/src/bonds/bond_reference_internal.hpp`、
`native/include/tdx/bond_reference.hpp`、`native/CMakeLists.txt`、`native/tests/bond_reference_tests.cpp`

## 动机

全树按单函数集中度排序后，`bond_reference_service_query.cpp` 是**非黑名单**文件中最高的一个：
304 行，`BondReferenceService::query()` 单函数 297 行，集中度 98%。

该函数把一条完整查询流水线塞在一处：入参归一化 → 六组校验 → 数据源选择 → 抓取 →
分市场投影稽核（含客户端合并表比对，约 100 行）→ 过滤与分面统计 → 排序 → 分页 →
摘要 → 结果组装。

排序结果发现的另一类形态（本次未动）：`recon/recon_contract_*.cpp` 共 14 个文件约 5700 行，
集中度 95–97%，全部是 `validate_*_contract()` 里一条按 `contract_id` 分发的长 `if/else` 链，
每个分支跑一个契约的断言。机械但量大，留作后续批次。

## 关键风险：拆分前没有任何测试覆盖

`bond_reference_tests.cpp` 原有 10 条断言**只覆盖 `normalize_bond_reference_rows` 与数据源目录**，
`query()` 全流水线零直接覆盖。对 297 行函数做拆分而无行为基线是本次真正的风险点。

因此先建**黄金输出基线**再动代码。`fetch_source` 在未加 `--refresh` 时优先读 `--jsn-root`
（默认 `output/tdx-jsn`）下的本地 JSN，因此整条查询可离线跑通。本地语料覆盖 9 个 bucket，
包含 40MB 的 42k 行全债券档案、两条客户端合并表比对路径，以及无需任何开关即自动触发稽核的
`policy-financial`。

46 个用例覆盖：5 条无稽核基线路径、6 条稽核路径（含 `policy-financial` 自动触发、
`private`/`asset-backed` 客户端合并表、无投影计划的 bucket 必须保持未稽核）、
10 条排序（5 个字段 × 升降序）、8 条过滤（市场名/数字 id、代码、文本、未命中、组合）、
4 条分页边界、2 条 42k 档案、9 条校验错误、2 条大小写归一化。
仅 `generated_at` 做归一化，其余逐字节比对。

## 拆分策略

`query()` 拆成 21 个单元、4 个翻译单元，命名沿用同目录 `convertible_bonds_service_query*`
既有约定（编排文件 + 若干 `_service_query_<phase>.cpp`）：

| 文件 | 行数 | 内容 |
| --- | --- | --- |
| `bond_reference_service_query.cpp` | 54 | `query()` 阶段编排 |
| `bond_reference_service_query_records.cpp` | 117 | `resolve_query`、`collect_matching_rows`、`sort_matching_rows`、`paginate_rows` |
| `bond_reference_service_query_projections.cpp` | 158 | `audit_category_projections` + 5 个文件内静态辅助 |
| `bond_reference_service_query_result.cpp` | 87 | `build_summary`、`source_option_list`、`build_query_filters`、`query_semantics` |

只有投影稽核需要 `this`（要调 `fetch_source`），因此它是唯一新增的私有成员函数；
其余阶段都是 `bond_reference_detail` 里的自由函数，声明在内部头。
公有头只增加一个私有成员声明，公有 API（构造函数 + `query()`）完全不变。

## 引入的共享抽象

- `RowFacets` —— 把过滤循环里累积的 6 个统计量（3 张分面表、去重证券集、最早/最晚到期日）
  收成一个结构体，摘要无需二次遍历。
- `difference(left, right)` —— 合并 4 处"在 A 不在 B"的集合差集循环。
- `text_or_null(value)` —— 合并 4 处"空串报 null"（市场、代码、最早/最晚到期日）。
- `projection_market(source)` —— 抽出 `-sh` 后缀判定。
- `security_ids(document, source)` —— 原为函数内 lambda，提为文件内静态函数。
- `query_semantics()` —— 15 行 `semantics` 字面量移入结果组装 TU，编排函数只留阶段列表。

`audit_category_projections` 首版仍有 90 行，二次拆成 `reconcile_projections` +
`compare_client_master` + 稽核编排，全部为文件内静态函数，不进内部头。

## 保持不变的语义

`Json::Object` 是 `std::map<std::string, Json, std::less<>>`（`json.hpp:16`），**对象键插入顺序
不可观测**，序列化按键排序——这是允许跨阶段重组键写入的前提。但 `Json` 数组是 vector，
**追加顺序可观测**，因此以下数组顺序逐一保持原样：`sources`（选定源 → 各投影 → 客户端合并表）、
`source_options`、`records`、`master_only`/`projection_only`/`projections`/
`matching_projection_resources`。

其他承重细节：

- **校验顺序可观测**（决定错误请求报哪条消息），`resolve_query` 严格保持
  group → bucket → sort → order → paging → cache/timeout 六步顺序。
- `options.code` 只 `trim` **不转小写**，保持大小写敏感。
- `raw_rows` 是 `fetched.document` 的引用；`FetchResult` 按值持有 Json，是 `cache_` 之外的独立副本，
  故稽核阶段往 `cache_` 插入投影文档不会使其失效。
- 稽核里投影 id 集合被计算两次（一次统计、一次和客户端合并表比对），**原实现如此，不做优化**，
  以免改变行为。
- 分页时才用 `retain_raw=true` 重新归一化原始行——这正是 42k 行档案只为返回页付代价的原因。
- 客户端合并表元数据仍在投影元数据之后追加。

## 测试

原 10 条断言之外，新增 **34 条**覆盖 `query()` 全流水线，全部走公有 API。
`src/bonds` 在 CMake 里是 PRIVATE，测试无法包含内部头，因此用合成的本地 JSN 资源驱动真实
`query()`：JSN 就是 GBK 解码后的普通 JSON（`[{"colheader":[...],"data":[[...]]}]`），
写成纯 ASCII 即可离线跑，不依赖已下载语料。

6 行合成夹具刻意包含：跨市场、重复票息率、缺票息率、缺到期日、缺分面字段各一，
使每个阶段都有可手算的期望值。断言覆盖分面统计（只计非空、按序）、到期日区间（跳过空值）、
5 个排序字段双向、缺数值键的 ±∞ 哨兵**在两个方向都排最后**、市场名与数字 id 两条过滤路径、
分页窗口与越界、`retain_raw` 只在分页生效、filters 回显空值为 null、大小写归一化，
以及 **CLI 无法到达的两条校验**——`bounded()` 会先拦下 offset/limit/cache-ttl/timeout-ms，
所以服务自身的 paging 与 cache/timeout 范围消息此前在任何地方都没有覆盖。

变异测试 7 项，逐一确认断言有效：哨兵符号取反、分面空值守卫移除、`retain_raw` 关闭、
offset 忽略、到期日区间守卫移除、市场数字 id 分支短路——6 项立即被捕获。

第 7 项**去掉 security_id 平局兜底最初未被捕获**：`std::stable_sort` 本就保持输入顺序，
而夹具里平局两行的文档顺序恰好与 security_id 顺序一致，断言无法区分"按 security_id 兜底"
与"stable_sort 保持原序"。为此另加一个专用夹具，三行票息率全同且**文档顺序刻意与 security_id
顺序相反**（先写 SZ200002 后写 SH200001），并对 rate/remaining/maturity 三个字段双向断言
——兜底始终升序，不随 `desc` 反向。补后该变异立即被捕获。

## 验证

- 4 个翻译单元 `-fsyntax-only` 通过
- 全量编译无错误无警告
- **黄金基线 46/46 逐字节一致**（含 941 / 11268 / 6738 / 42479 行四条规模路径、
  两条客户端合并表比对、9 条校验错误消息逐字一致）
- 稽核内容被基线深度覆盖：`policy-financial` 十行主表 = 沪 7 + 深 3 且 `exact_match` 为真
  （与 `semantics` 文案描述一致）；`asset-backed` 6738 行 → 6728 去重 id 对 1588 投影并集，
  `matches_single_market_projection` 为真
- **ctest 108/108 通过**
- HTTP 侧（`server_state.cpp:200`）构造同一服务、序列化同一 `query()` 输出，
  CLI 逐字节一致即覆盖该面
- 未触及 L2 付费功能与 8765 端口（改动文件内无相关引用）

## 集中度结果

| | 拆分前 | 拆分后 |
| --- | --- | --- |
| 最大函数 | 297 行 | 49 行 |
| 文件行数 | 304 | 54 + 117 + 158 + 87 |

拆分后各文件：`bond_reference_service_query.cpp` 87%、
`bond_reference_service_query_projections.cpp` 31%、
`bond_reference_service_query_records.cpp` 27%、
`bond_reference_service_query_result.cpp` 18%。

编排文件 87% 是**指标假象而非问题**：该 TU 只有 54 行且几乎只含 `query()` 一个函数，
比值自然趋高。集中度指标本意是找"一个函数藏住整个文件"的情况，54 行文件里 47 行的扁平阶段列表
不属于此类；此处真正的信号是函数绝对长度 297 → 49（6 倍下降）。继续拆只会把阶段列表打散。

## 后续候选

1. `recon/recon_contract_*.cpp` 共 14 个文件约 5700 行，集中度 95–97%，
   统一是按 `contract_id` 分发的长 if/else 链，每分支一个契约。适合按分支提函数，量大但风险低。
2. `bonds/bond_reference_service_query.cpp` 之后，非黑名单实逻辑文件里较高的还有
   `corporate/financial_insights_normalize.cpp`（311 行 / 301 行 / 97%）、
   `research/commodity_links_service_query.cpp`（232 行 / 225 行 / 97%）、
   `corporate/ownership.cpp`（330 行 / 316 行 / 96%）、
   `cloud/cloud_calc_builtins_execute.cpp`（216 行 / 205 行 / 95%）。

仍不在机械拆分范围内（需专门规划）：`jsn.cpp`、`formula_functions_series.cpp`、
`formula_context_dynamic.cpp`、`tpool.cpp`、`tpool_evaluate.cpp`、`pbrpc.cpp`。
