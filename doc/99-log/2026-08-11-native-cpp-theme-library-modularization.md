# 原生 C++ 题材库链路模块化

日期：2026-08-11

## 动机

`native/src/theme_library.cpp` 是审计标记的下一个低风险候选，594 行单文件承担了五张
ZTTZ 母表的抓取、三种 view 的过滤分页、两个缓存域的生命周期以及 CLI 适配。其中
`ThemeLibraryService::query` 独占 158 行（27%），是文件内唯一的集中点：它把参数校验、
题材过滤、成员/明细/走势的按需抓取、失败记录和 17 个字段的响应组装全部串在一条函数里。

同时文件顶部 177 行是 17 个匿名 `static` helper，从字段读取、市场代码换算一直到全文
检索和摘要裁剪，全部对同一个翻译单元可见。任何一次改动都要在这 594 行里定位上下文。

## 拆分结果

内部头 `native/include/tdx/theme_library_internal.hpp`（44 行）把 17 个 helper 提升为
`namespace tdx::theme_library_detail` 的具名声明，按字段读取、证券身份、成员解析、
全文检索、摘要裁剪、排序、杂项七组加注释分区。

| 单元 | 行数 | 职责 |
| --- | --- | --- |
| `theme_library_support.cpp` | 195 | 17 个 helper 实现：`value_ptr`/`text_value`/`number_value`/`unsigned_value`、`digits`/`market_id`/`market_name`/`market_prefix`、`security_document`/`parse_members`/`document_for_resource`、`json_contains`/`theme_summary`/`sort_themes`、`now_text`/`bounded`/`native_path` |
| `theme_library_catalog.cpp` | 19 | `theme_library_sources()`：五张 ZTTZ 母表（地域、国资、公司、通用、持股）到 `list/func_zttz102_1.jsn`…`106` 的映射 |
| `theme_library_normalize.cpp` | 110 | `normalize_theme_library_rows` / `_details` / `_chart` 三个公开归一化函数 |
| `theme_library_fetch.cpp` | 98 | 构造函数、`fetch_master`（五母表折叠 + 跨源摘要，整体缓存为一份文档）、`fetch_dynamic`（`zttz/ID` 与 `zttz1/ID` 分资源缓存） |
| `theme_library_service.cpp` | 192 | `query` 主流程，再分 `validate_query` / `filter_and_select` / `page_array` / `record_failure` 四个助手 |
| `theme_library_command.cpp` | 64 | `command_market_theme_library` CLI 适配与帮助文本 |

原 594 行 / 1 文件变为 678 行 / 6 文件（另加 44 行内部头），最大单元 195 行。行数净增
84 行，来自单元头部的 include 与命名空间样板，以及四个助手的签名。

## `query` 的分解方式

四个助手都放在 `theme_library_service.cpp` 的匿名命名空间里，不进内部头，因为它们只服务
这一个函数：

```cpp
void validate_query(ThemeLibraryQuery& options);        // trim/lower 归一 + view/source/分页校验
void filter_and_select(const Json& themes, const ThemeLibraryQuery& options,
                       Json& filtered, Json& selected);
Json page_array(const Json& source, int offset, int limit);
void record_failure(Json& errors, const Json& resource, const char* message);
```

`page_array` 替掉了三份逐字重复的 offset/limit 循环（themes、members、details 各一份）。
两处 `fetch_dynamic` 的 try/catch 仍留在 `query` 内联，因为它们需要 `this`，提成自由函数
反而要把 service 引用穿进去。

## 保持不变的语义

- JSON 字段插入顺序即输出顺序，响应组装严格保持原 17 键次序：`schema`、`generated_at`、
  `view`、`availability`、`source_options`、`themes`、`selected_theme`、`members`、
  `details`、`chart`、`summary`、`counts`、`errors`、`sources`、`upstream_health`、
  `cache`、`semantics`。
- 公开 schema `tdx-market-theme-library-native-v1` 未变；`--help` 文本逐字节一致。
- 五张 ZTTZ 母表是互相独立的来源快照而非层级关系，因此 `record_id` 仍为 `SOURCE:ID`
  复合键；`theme_library_catalog.cpp` 里补了注释说明这一点，避免后来者误以为可以合并。
- 三个 normalize 函数的 `securities = {}` 默认参数留在头文件声明上（定义移出后默认值
  必须在声明侧）。

## 验证

拷贝 `native/` 到 `build/equiv-src`，把归档的 `theme_library.cpp.removed` 还原回
`theme_library.cpp`，将 CMakeLists 里六条拆分条目折回单条，另建 `build/equiv-baseline`
得到拆分前的 `tdx-tool.exe`，与拆分后的 `build/verify-mingw/tdx-tool.exe` 跑同样样本。

归一化字段：`generated_at`、`sources[].age_seconds`、`sources[].attempts`、
`sources[].endpoint`、`elapsed_ms`。`cache.*` 对全新进程是确定的，无需归一。

| 样本 | 命令要点 | 归一后字节 | SHA256（两侧一致） |
| --- | --- | --- | --- |
| 1 | `--view catalog --source all --sort members --order desc --limit 8` | 6,726 | `37D9CF82858C2613…6D7C` |
| 2 | `--view catalog --source region --query 经济 --sort name --order asc --limit 5` | 5,789 | `7ED1BBB708825032…A806` |
| 3 | `--view security --market sz --code 000001 --limit 6` | 4,168 | `13ECB96C757953A8…FB98` |

三个样本覆盖 catalog 与 security 两种 view、`all` 与单源两种取源、members/name 两种排序
方向以及全文检索路径，均返回 `availability=live`（样本 1 matched=1102，样本 3 matched=3）。

另外执行：`tdx-theme-library-tests` 通过，`tdx-recon-contract-tests` 通过，
`tdx_native` 与 `tdx-tool` 均正常构建。

## 构建期修正

拆分后首次编译暴露一处遗漏：`fetch` 与 `service` 两个单元用到
`fetch_jsn_resources_rows`、`jsn_source_metadata`、`jsn_sources_health`，但原文件同时
include 了 `tdx/jsn.hpp` 与 `tdx/jsn_data.hpp`，新单元只补了后者。补上 `tdx/jsn.hpp`
与 `<string>` 后，连带报出的 `std::set<std::pair<int,std::string>>::insert` 花括号初始化
错误一并消失——那是缺声明的次生错误。

## 归档

原文件保留为 `native/src/theme_library.cpp.removed`（594 行），与
`announcement_signals.cpp.removed`、`curated_data.cpp.removed`、`trades.cpp.removed`
等同族归档一致，随时可用于重建等价基线。
