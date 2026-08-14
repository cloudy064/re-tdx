# 精选数据链路模块化

日期：2026-08-11

## 背景

`native/src/curated_data.cpp` 原为 608 行单体，承担八项客户端 BigData 精选表的资源
常量、港股名称解析、八套字段归一化、缓存抓取、九种 view 过滤与 CLI 适配。所有职责集中
在同一翻译单元，任何一类归一化字段调整都要重新编译整块，且八项资源路径与其 kind/中文
标签通过三处平行的 `if` 链维护，容易漂移。

## 拆分结果

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `curated_data_normalize.cpp` | 162 | 八类字段归一化与派生公式（PEG、分红募资比、回购市值占比等） |
| `curated_data_support.cpp` | 130 | 字段读取、数值解析、缩放、市场号解析/命名、UTF-8 摘要、时间戳、区间校验 |
| `curated_data_service.cpp` | 126 | 参数校验、九种 view 过滤、市场/代码匹配、汇总统计与响应组合 |
| `curated_data_fetch.cpp` | 76 | 构造函数、TTL 缓存、本地优先/远端回退、逐资源归一化与来源汇总 |
| `curated_data_command.cpp` | 69 | CLI 帮助文本、参数解析、根目录定位与输出落盘 |
| `curated_data_security.cpp` | 67 | 证券身份文档、港股 `tdxhkag.cfg` 名称解析 |
| `curated_data_resources.cpp` | 61 | 本地 JSN 表加载、文档定位、来源摘要 |
| `curated_data_catalog.cpp` | 45 | 八项资源的路径/kind/标签编译期目录 |
| **合计** | **736** | |

原 `src/curated_data.cpp` 重命名为 `.removed`，最大单元由 608 行降至 162 行（−73%）。

## 资源目录

八项资源的路径、kind 与中文标签合并为单张类型化目录，`kind_for()` 与 `label_for()`
改为线性查表，替换原先三处平行 `if` 链：

```cpp
struct ResourceEntry {
    const char* path;
    const char* kind;
    const char* label;
};

const ResourceEntry resources[] = {
    {kMedia, "media-entertainment", "传媒娱乐"},
    {kLowValuation, "low-valuation-smallcap", "低估值袖珍股"},
    {kDividendFundraising, "dividend-fundraising", "分红募资统计"},
    {kBuybackStatistics, "buyback-statistics", "拟回购统计"},
    {kHighDividend, "high-dividend", "高分红"},
    {kHongKongPerformance, "hk-performance", "港股多周期表现"},
    {kHighRefinancingLending, "high-refinancing-lending", "转融券余额高"},
    {kBelowBookSoe, "below-book-soe", "破净国企股"}
};
```

原实现里资源列表是 `const std::vector<std::string>`（运行期堆分配），现由目录直接遍历，
`fetch_master()` 仅在需要远端抓取时构造一次 `std::vector<std::string>` 传给
`fetch_jsn_resources_rows()`。

## 内部接口

新增 `native/include/tdx/curated_data_internal.hpp`，把原匿名命名空间内的辅助函数提升到
`tdx::detail::curated_data`，供归一化、抓取、服务和 CLI 四层共享。公开头文件
`tdx/curated_data.hpp` 未改动。

`native_path()` 在 `curated_data_security.cpp` 中定义、在 `curated_data_resources.cpp`
中前向声明复用；CLI 单元保留自己的匿名命名空间副本，避免把平台路径转换暴露到内部头。

## 保持不变的语义

- **公开 schema**：`tdx-market-curated-data-native-v1`，字段与顺序未改；
- **单位双出**：CFG 的 万元/万股/亿元 字段继续同时以 yuan/share 基准单位输出；
- **聚合行**：`buyback-statistics` 不绑定证券，`security` 保持 `null`，`event_id`
  以 `market` 作为身份段；
- **港股市场号**：31/48/49 三个市场号在 `--market hk` 下均匹配，48/49 的原始市场号
  在 `market_id` 中保留；
- **本地优先**：`--refresh` 未指定且 `--input-dir` 存在时先读本地 JSN，任一资源缺失
  则整体回退远端，与原逻辑一致（`catch (...)` 清空后统一走 `fetch_jsn_resources_rows`）；
- **转融券快照**：`has_lending_data` 仍以 `zxye`/`rqyl` 任一有值为真，空余额不填充。

## 与既有模式的对齐

| 模式 | 本次落点 |
|---|---|
| 编译期类型目录 | `curated_data_catalog.cpp` 的 8 条 `ResourceEntry` |
| 公共支持下沉 | `curated_data_support.cpp` 无业务分支，仅解析/格式化 |
| 归一化独立 | `curated_data_normalize.cpp` 只依赖目录与支持层 |
| 抓取与缓存分离 | `curated_data_fetch.cpp` 持有 TTL 与本地/远端选择 |
| 响应组合独立 | `curated_data_service.cpp` 负责过滤、汇总与 schema |
| CLI 适配隔离 | `curated_data_command.cpp` 不含业务判定 |

## 规模变化

| 指标 | 拆分前 | 拆分后 |
|---|---:|---:|
| 文件数 | 1 | 8 |
| 总行数 | 608 | 736（+21.1%） |
| 最大单元 | 608 | 162（−73%） |

行数增长来自新增的 8 组 include 头、`using` 声明和内部头声明，属结构化开销。

## 审计口径修正

本次重新按 `native/src/*.cpp`（排除 `.removed`）实测，得到 641 文件 / 129,715 行 /
中位 153 行。上一版（v64）记录的 129,223 行与中位 156 行与实测不符：按本次拆分的
增量（−608 +736）回推，v64 时点应为 634 文件 / 129,587 行，说明 v64 的总行数已偏低
约 364 行。测试侧（122 文件 / 21,575 行 / 中位 101 / 最大 1,417）与记录完全一致，
故偏差仅限生产总行数与中位数两项，本版已改为实测值。

## 已执行的静态校验

- 八个新单元的命名空间闭合与 `#include` 依赖逐一核对；
- `resources[]` 的八条路径与原 `kMedia`…`kBelowBookSoe` 常量逐字比对一致；
- `kind_for()`/`label_for()` 的映射与原 `if` 链逐条比对一致，`label_for()` 的
  原“兜底返回破净国企股”改为显式 `"未知分类"`（原分支只在 kind 来自目录时可达，
  行为等价）；
- 公开 API 覆盖：`normalize_curated_data_rows`、`CuratedDataService` 构造/
  `query`/`fetch_master`、`command_market_curated_data` 均有且仅有一处定义；
- `tests/curated_data_tests.cpp` 只依赖 `normalize_curated_data_rows` 与公开头，
  未受内部拆分影响。

## 未运行的验证

当前环境（Git Bash on Windows）无可用 C++ 编译器与 make/ninja，`cmake --build` 无法
配置，因此以下未执行：

- `tdx-curated-data-tests` 与 `tdx-tool` 的编译链接；
- 专项测试的实际运行；
- `market curated-data` 真实样例的 schema 与行数复核。

需要在具备 MSVC/MinGW 工具链的环境补跑上述三项。

## CMake

`native/CMakeLists.txt` 中 `src/curated_data.cpp` 一行替换为八个新单元，保持
`TDX_CORE_SOURCES` 的字母序位置不变。

## 下一候选

| 顺序 | 文件 | 行数 | 风险评估 |
|---:|---|---:|---|
| 1 | **`trades.cpp`** | 605 | 低风险：逐笔成交链路，可按协议/归一化/服务/CLI 拆分 |
| 2 | `recon_contract_market_corporate_integration.cpp` | 610 | 中风险：对账契约集成，需确认契约断言覆盖 |
| 3 | `formula_functions_series.cpp` | 645 | 高耦合：共享解释器语义，需宽回归 |
| 4 | `tpool.cpp` | 637 | 高耦合：XML/公式兼容解析 |
| 5 | `jsn.cpp` | 713 | 高耦合：共享解析器 |
