# 原生 C++ 对账契约集成层模块化

日期：2026-08-11

## 背景

`native/src/recon_contract_market_corporate_integration.cpp` 累积到 610 行，其中
`validate_market_corporate_integration_contract` 单个函数占 592 行，由一条 13 分支的
`contract_id == "..."` / `else if` 判断链组成，覆盖 22 项契约。分支之间没有共享状态，
唯一的耦合是末尾的 `else { return false; }` 兜底，但物理上必须整体阅读才能定位任一契约的
断言。

同族的 `recon_contract_market_corporate_catalog.cpp` 已经采用编译期派发表模式，本轮把
集成层对齐到同一形态。

## 拆分结果

| 实现单元 | 行数 | 职责 |
| --- | --- | --- |
| `..._integration_catalog.cpp` | 57 | 11 项契约的编译期派发表 + 个股韧性族兜底派发 |
| `..._integration_rankings.cpp` | 92 | `benchmark-analysis-live`、`consensus-stage-rankings-live` |
| `..._integration_calendar.cpp` | 154 | `calendar-expanded-live`（十族总量、SH688783 投影、条件资源清单） |
| `..._integration_unlocks.cpp` | 127 | `recent-large-unlocks-live`、`unlock-monthly-pressure-live` |
| `..._integration_securities.cpp` | 156 | 11 项个股韧性契约的类型表 + `stock-roadshows-live` |
| `..._integration_platform.cpp` | 161 | `jsn-discovery-live`、`jsn-candidates-live`、`cloud-variants-fixed`、`report-cache-default`、`report-cache-explicit` |

文件名前缀均为 `recon_contract_market_corporate_integration_`。原文件重命名为
`.removed`，`native/CMakeLists.txt` 的单条目替换为上述六条。

## 派发表替代判断链

原先 13 个 `else if` 分支改为一张 `constexpr std::array`，与同族 catalog 单元同构，
包含编译期唯一性断言：

```cpp
constexpr std::array<MarketCorporateIntegrationContract, 11>
    integration_contracts{{
        {"benchmark-analysis-live", validate_benchmark_analysis_contract},
        ...
    }};
static_assert(unique_contract_ids());
```

11 项个股契约原先在一个 `if` 条件里用 11 个 `||` 列出 id，然后在函数体内构造两张运行期
`std::map<std::string, std::string>`（schemas 11 项、modes 1 项覆盖）并用 `.at()` 查找。
本轮改为一张 `constexpr` 表，把 id、schema、mode 绑定在同一行：

```cpp
constexpr std::array<StockResilienceContract, 11> resilience_contracts{{
    {"stock-research-live", "tdx-market-research-native-v1", "security"},
    ...
    {"stock-ratings-live", "tdx-market-ratings-native-v1", "selection"},
    ...
}};
```

两张 `std::map` 因此消失，`modes` 的"缺失即 security"隐式默认变成表内显式值。

派发顺序：catalog 先查 11 项专有契约，未命中再交给 `validate_stock_resilience_contract`，
后者不认识该 id 时返回 `false`。这与原函数"落到最后 `else` 返回 `false`"等价，父级
`validate_market_corporate_contract` 的六个 validator 轮询语义不变。

## 保持不变的语义

- 73 条断言的名称、顺序、`expected` / `actual` 取值逐条保留（已做全量比对）
- `validate_market_corporate_integration_contract` 的签名与返回约定不变，父级
  `recon_contract_market_corporate.cpp` 的派发数组无需改动
- 只有 `report-cache-explicit` 读取 `context`；新签名统一为
  `(const Json& document, const Json& context, Json& result)`，其余单元把 context 参数留空名
- `calendar-expanded-live` 的条件资源清单（company_events 追加 8 项、futures-calendar
  等 11 个可选单项）追加顺序不变，`exact_sources` 仍按 `required_sources.size()` 比较
- 浮点容差不变：`benchmark` 与 `recent-large-unlocks` 用 `< 0.000001`，
  `recent-ipo` 用 `< 0.01`，`unlock-monthly-pressure` 用
  `max(0.01, |expected| * 1e-12)`

## 与测试表的交叉验证

`native/tests/recon_contract_calendar_resilience_tests.cpp:262` 独立维护着同一张
11 项个股契约的 `{id, schema, mode}` 期望表。把新增的 `constexpr` 表与该测试表逐项排序
比对，结果完全一致——这为运行期 `std::map` 到编译期表的转写提供了独立确认。

## 规模对比

原 610 行 → 六个单元共 747 行（+22.5%），增量来自各单元的 include 头、命名空间壳和派发表
本身。最大单元 161 行，相对原文件下降 74%。原先 592 行的单函数现在最大为 154 行
（`calendar-expanded-live`）。

## 已完成的静态校验

- 22 项契约 id 在新单元中零丢失（11 项在 catalog 表、11 项在个股韧性表）
- 73 条 `add_assertion` 名称与原文件全量比对，无增无减
- 13 个 validator 函数各只有一处定义
- 新增 `constexpr` 个股韧性表与测试期望表逐项一致
- 命名空间开闭平衡；两个带匿名命名空间的单元各自成对
- 12 个新 validator 名在拆分前全仓库无占用，无重名风险
- `array_empty` / `source_exists` / `numeric_value` 等辅助函数均为
  `recon_contract_internal.hpp` 中的 `inline` 定义，各单元可直接取用
- `CMakeLists.txt` 中不再残留原文件条目

## 未执行的验证

当前环境没有可用的 C++ 编译器（无 MSVC / MinGW，`cmake --build` 无法配置），以下项目
仍需在具备工具链的环境中补做：

- `tdx-recon-contract-calendar-resilience-tests`（覆盖 benchmark、calendar-expanded
  正反例、11 项个股韧性、缺失源健康元数据的拒绝、consensus 正反例、roadshows）
- `tdx-recon-contract-market-core-tests`（覆盖 `report-cache-explicit`）
- `tdx-tool` 链接
- `static_assert(unique_contract_ids())` 需编译期确认（两处）
