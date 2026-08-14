# 原生 C++ 对账研究目录层模块化

日期：2026-08-11

## 背景

`native/src/recon_contract_market_research_catalog.cpp` 累积到 598 行，其中
`validate_market_research_catalog_contract` 单个函数占 582 行，由 5 个顶层
`contract_id` 分支覆盖 12 项契约。三个分支内部还嵌了 `detail_mode` 二级分支，题材机会分支
更嵌了五路 `detail/legacy/completed/active` 判断，最深处有三层嵌套。

本轮沿用同日完成的对账契约集成层做法：编译期派发表 + 按域分单元，公开断言输出不变。

## 拆分结果

| 实现单元 | 行数 | 职责 |
| --- | --- | --- |
| `..._catalog_dispatch.cpp` | 61 | 12 项契约的编译期派发表与入口 |
| `..._catalog_forecasts.cpp` | 63 | `forecast-latest-live` |
| `..._catalog_indicators.cpp` | 138 | 经济指标目录/明细两项契约 |
| `..._catalog_themes.cpp` | 103 | 题材库目录/明细两项契约 |
| `..._catalog_strategic.cpp` | 180 | 战略主题目录/明细两项契约 |
| `..._catalog_thematic.cpp` | 153 | 题材机会目录/明细/旧客户端题材三项，并定义共享前置断言 |
| `..._catalog_thematic_hype.cpp` | 57 | 炒作已完成/进行中两项契约 |

文件名前缀均为 `recon_contract_market_research_catalog_`。原文件重命名为 `.removed`，
`native/CMakeLists.txt` 的单条目替换为上述七条。

## 派发表替代嵌套判断

12 个 id 改为一张 `constexpr std::array`，含编译期唯一性断言，与同族
`recon_contract_market_corporate_catalog.cpp` 同构：

```cpp
constexpr std::array<MarketResearchCatalogContract, 12> catalog_contracts{{
    {"forecast-latest-live", validate_forecast_latest_contract},
    ...
}};
static_assert(unique_contract_ids());
```

原先的 `detail_mode` 布尔开关消失：目录视图与明细视图各成一个独立函数，`view` 期望值不再
由三元表达式推导，而是各自写死。题材机会分支原先用
`(detail_mode || legacy_mode) ? "group" : completed_mode ? "hype-completed" : ...`
推导 `expected_view`，现在五个函数分别传入 `"catalog"` / `"group"` / `"group"` /
`"hype-completed"` / `"hype-active"`。

## 四组共享前置断言

同域的两个视图共享一段前置断言，提为 helper 而非在两个函数里复制：

| helper | 覆盖断言 | 归属 |
| --- | --- | --- |
| `assert_indicator_common` | schema、view、availability、indicator_count | 匿名命名空间，返回 `sources` 指针 |
| `assert_theme_library_common` | schema、view、availability、catalog_totals、master_consistency | 匿名命名空间 |
| `assert_strategic_common` | schema、view、availability、catalog_totals、master_consistency、category_hierarchy | 匿名命名空间，返回 `StrategicSourceFlags` |
| `assert_thematic_common` | schema、view、availability、opportunity_population、master_sources | 内部头文件，跨两个单元复用 |

`assert_strategic_common` 原先在分支内散落五个 `bool`（`has_5g` / `has_defense` /
`has_defense_groups` / `has_internet` / `has_detail`）供两个子分支各取所需，现在收进
`StrategicSourceFlags` 一起返回，作用域边界因此明确。`assert_thematic_common` 因为被
`..._thematic.cpp` 与 `..._thematic_hype.cpp` 两个翻译单元引用，声明进
`recon_contract_market_research_internal.hpp`，其余三个保持单元私有。

## 保持不变的语义

- 56 条断言的名称、每项契约内的先后顺序、`expected` / `actual` 取值逐条保留
- `validate_market_research_catalog_contract` 的签名与返回约定不变，父级
  `recon_contract_market_research.cpp` 的 validator 轮询数组无需改动
- 未知 `contract_id` 仍返回 `false`（原为末尾 `else`，现为派发表遍历落空）
- 全部 12 项契约都不读 `context`，per-contract validator 签名收窄为
  `(const Json& document, Json& result)`，`context` 的 `(void)` 抑制留在入口
- 浮点与阈值不变：预测缺口用 `< 0.000001`，炒作相对收益公式同样用 `< 0.000001`，
  指标 72 项、题材库 1102/927/850、战略 26/598/655/9、题材机会 15/6/8/1/1 全部原值

## 规模对比

原 598 行 → 七个单元共 755 行（+26.3%），增量来自 include 头、命名空间壳、派发表与四个
helper 的签名。最大单元 180 行，相对原文件下降 70%；原先 582 行的单函数现在最大为 180 行
（战略主题域，含其 helper）。

## 已完成的静态校验

用一支临时校验脚本对三项对账拆分做了统一比对（脚本用后即删，未入库）：

- 12 项契约 id 与原文件完全一致（另比对了集成层的 22 项）
- 56 条 `add_assertion` 名称按多重集比对，无增无减
- 每个 `add_assertion` 的完整实参文本（空白归一化后）都能在原文件中找到，仅
  `std::string(expected_view)` 与 `flags.*` 五项为本轮有意替换，已列入白名单
- 每项契约内部的断言顺序与原分支一致
- 4 个 helper 各只有一处定义，11 处调用点的 view 实参与原 `expected_view` 推导一致
- 15 个新增符号名在拆分前全仓库无占用
- `CMakeLists.txt` 中不再残留原文件条目

校验脚本做过反向验证：向副本注入三处缺陷（改断言名、删派发表一行、改阈值 598→599）后，
脚本分别以断言名缺失、id 缺失、实参漂移三种方式全部报出，未被漏过。

## 未执行的验证

当前环境没有可用的 C++ 编译器（无 MSVC / MinGW，`cmake --build` 无法配置），以下仍需在
具备工具链的环境中补做：

- 覆盖这 12 项契约的对账测试与 `tdx-tool` 链接
- `static_assert(unique_contract_ids())` 的编译期确认
- 类型层面的检查：`StrategicSourceFlags` 的返回、`std::string_view` 到 `Json` 的显式
  转换、各单元 include 是否完备，都只做了静态阅读，未经编译器确认
