# 原生 C++ 经济指标链路模块化

日期：2026-08-11

## 动机

`native/src/economic_indicators.cpp` 是审计里自带专项测试目标、尚未受控的最大文件：584 行
中 `EconomicIndicatorService::query` 独占 199 行（34%）。集中度不像因子链路那样极端，但这个
函数的形状更麻烦——它不是分派器，而是一条**带三段可选支线的主干**：

| 原始区间 | 行数 | 内容 |
| --- | --- | --- |
| 22-180 | 159 | 文件级 helper：字段读取、标量校验、证券身份、行情查找、全文检索 |
| 182-289 | 108 | 三个 normalize：母表目录、历史序列、关联证券 |
| 291-330 | 40 | 构造函数与两个缓存域（JSN 资源、L1 快照） |
| 332-530 | 199 | `query`：校验 → 母表 → 摘要 → 目录/单指标二选一 → 17 键响应组装 |
| 532-582 | 51 | CLI 适配 |

`query` 里，`--view catalog` 只需过滤、排序、分页；`--view indicator` 则要定位选中行，再按
三个开关分别拉取历史序列（`jjzb1/<id>.jsn`）、关联证券（`jjzb2/<id>.jsn`）和这批证券的实时
行情，每一段都可能失败并降级为 `partial`。三段支线各自要往外写 3 到 5 个字段，于是函数体里
平铺了 13 个出参变量，读到末尾的响应组装时已经很难确认某个键的值是哪一段写的。

## 拆分结果

三段可选细节成为 `EconomicIndicatorService` 的私有成员函数，因为它们都需要 `this`：调用
`fetch_resource` / `fetch_quotes` 两个缓存域，并读 `blocks_.securities`。13 个出参归入一个
私有 `QueryState`，声明在公开头 `tdx/economic_indicators.hpp` 的 private 段。

| 单元 | 行数 | 职责 |
| --- | --- | --- |
| `economic_indicators_support.cpp` | 172 | 19 个文件级 helper：`text_value`、`number_value`、`iso_date`、`valid_indicator_id`、`market_id`、`security_document`、`find_quote`、`quote_metric`、`json_contains` 等 |
| `economic_indicators_normalize.cpp` | 124 | `normalize_economic_indicator_rows` / `_history` / `_relations` 与排序 |
| `economic_indicators_fetch.cpp` | 61 | 构造函数、`fetch_resource`（按资源路径缓存）、`fetch_quotes`（按去重证券表缓存） |
| `economic_indicators_query_support.cpp` | 92 | `validate_indicator_query`、`build_indicator_summary`、`filter_sort_page_catalog`、`page_array`、`record_failure` |
| `economic_indicators_service.cpp` | 173 | `load_history`、`load_related`、`attach_quotes` 三段可选细节，以及收敛到 80 行的 `query` |
| `economic_indicators_command.cpp` | 68 | CLI 适配 |

新建内部头 `economic_indicators_internal.hpp`（73 行）收纳母表常量与 24 个声明（19 个文件级
helper 加 5 个查询阶段 helper），命名空间为 `tdx::economic_indicator_detail`。

原 584 行 / 1 文件变为 690 行 / 6 文件，最大单元 173 行，`query` 从 199 行降到 80 行。净增
106 行来自六份 include 与命名空间样板、三个成员函数签名和 `QueryState` 的字段声明。

## 顺带消掉的重复

| 构造 | 原有份数 | 拆分后 |
| --- | --- | --- |
| `{resource, message}` 失败记录 | 3 | 1 个 `record_failure`，4 处调用 |
| offset/limit 分页循环 | 2 | 1 个 `page_array`，4 处引用 |

字面量核对（字符扫描器，不用正则）显示仅 `resource` 与 `message` 各少 2 份，正是失败记录
归并掉的两处；新增的 14 处全是 include 路径。没有任何一句 methodology、警告或错误消息在
搬迁中丢失或改写。

## 保持不变的语义

- **17 个响应键的插入顺序即输出顺序**，逐字保留。其中 `summary` 内部的
  `related_security_count` 必须排在 `history_points` 之前（原文第 443 行由关联段写入，
  第 483 行才补 `history_points`）；拆分后 `history_points` 的赋值刻意放在关联段之后，
  并在该行加注说明。
- 摘要在过滤**之前**基于完整母表计算，因此 `--query` 不影响类型/频度直方图与更新日期跨度。
- 三段支线捕获异常并写入 `state.errors` 而非抛出，缺失的详情资源仍降级为 `partial`。
- 历史序列升序排列，超出 `--history-limit` 时从**头部**截断以保留最近点。
- `fetch_quotes` 的缓存键由去重后的证券表拼成，键的构造方式未变（已加注）。
- 公开 schema `tdx-market-economic-indicators-native-v1` 未变，`--help` 未动。

## 搬迁中发现并修掉的一处悬垂引用

关联证券分页最初写成

```cpp
for (const auto& row : page_array(filtered, options.offset, options.limit).as_array())
    paged_raw.push_back(row.at("raw"));
```

`page_array` 按值返回 `Json`，`as_array()` 交出的是指向该临时量内部的引用。range-based for
只延长**区间表达式本身**是临时量时的生命期，`f().g()` 这种形式不在延长范围内，于是临时
`Json` 在初始化区间的完整表达式结束时即析构，`row` 指向已释放内存。表现为
`jjzb2/M2100030000.jsn: map::at`——`related_securities` 空掉、`availability` 变成
`partial`。等价样本一次就抓到了这个差异（基线 13,533 B 对拆分 3,373 B）。

修法是把分页结果绑到具名局部量，既保住去重又正确：

```cpp
const Json paged = page_array(filtered, options.offset, options.limit);
Json paged_raw = Json::array();
for (const auto& row : paged.as_array()) paged_raw.push_back(row.at("raw"));
```

同一模块内 `page_array` 只有另一处调用，结果按值返回，不受此影响。

## 验证

拷贝 `native/` 到 `build/equiv-src`，还原 `economic_indicators.cpp.removed`，把 CMakeLists
的六条 `src/economic_indicators_*` 条目收回一条，另建 `build/equiv-baseline` 得到拆分前的
`tdx-tool.exe`。

### 校验拒绝路径（离线、确定性）

22 条拒绝路径两侧输出逐字一致：未知视图、缺 `--indicator-id`、id 长度/字符校验、`--limit`
与 `--offset` 的边界与非数字、五个排序键与两个方向的白名单、`--timeout-ms` 上下界、三个
TTL 的负值、`--history-limit` 边界、未知参数与缺值参数。

### 文档样本

15 个真实上游样本，归一 `generated_at`、`age_seconds`、`elapsed_ms`、
`{master,detail,quote}_age_seconds` 与 `attempts` / `max_attempts` 后比对：

| 样本 | 覆盖 | 结果 |
| --- | --- | --- |
| `catalog --limit 3` | 目录路径 | 归一后一致（3,171 B） |
| `indicator --history-limit 10` | 主干 + 历史 + 关联 | 逐字节一致（13,533 B） |
| `--no-history` | 关闭历史支线 | 逐字节一致（12,509 B） |
| `--no-related` | 关闭关联支线 | 归一后一致（10,866 B） |
| `--no-quotes` | 关闭行情附着 | 归一后一致（12,484 B） |
| `indicator --query 000` | 关联证券过滤 | 归一后一致（12,465 B） |
| `indicator --offset 5 --limit 6` | 关联证券分页 | 逐字节一致（13,119 B） |
| `catalog --sort name/value/mom/yoy` | 四个排序键 + 两个方向 | 全部逐字节一致 |
| `catalog --offset 10` | 目录分页 | 逐字节一致（3,234 B） |
| `catalog --query 30000` | 目录过滤 | 归一后一致（2,078 B） |
| `indicator --refresh` | 强制刷新 | 归一后一致（21,275 B） |
| `--detail-cache-ttl 300 --quote-cache-ttl 60` | 自定义 TTL | 归一后一致（21,275 B） |

15 个样本全部等价，其中 8 个在归一前就逐字节一致。

### 运行间漂移不是重构差异

`--refresh` 样本归一后仍余两处差异：母表来源的 `attempts` 为 2 对 1，以及
`upstream_health.max_attempts` 这个由前者派生的值。用**同一个基线二进制**连跑两次即可确认
归因——基线自比时原始输出同样不等，而在同一套 `attempts` 归一下两侧都相等。瞬时失败后的
重试次数本就随网络状况在运行间变化。

### 测试

`tdx-economic-indicators-tests` 通过（`economic indicator tests passed`），`tdx_native` 与
`tdx-tool` 正常构建，`--help` 输出未变。六个单元首次编译即全部通过语法检查。

## 归档

原文件保留为 `native/src/economic_indicators.cpp.removed`（584 行），与同族归档一致。
