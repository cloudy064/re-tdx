# 原生 C++ 商品与涨价题材关联链路模块化

日期：2026-08-11

## 目标

拆解 780 行的 `commodity_links.cpp`，合并重复维护的视图、帮助目录和排序规则，并分离商品
行情、题材事件、证券关联、缓存服务、查询编排和 CLI。

## 落地结果

原根文件已移除，形成八个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `commodity_links_catalog.cpp` | 82 | 7 项视图目录、两项主资源和排序能力 |
| `commodity_links_support.cpp` | 254 | 字段、证券集合、搜索、排序和参数支持 |
| `commodity_links_normalize_quotes.cpp` | 52 | 商品报价及区间涨跌幅 |
| `commodity_links_normalize_themes.cpp` | 99 | 涨价题材、驱动事件和长期关联股 |
| `commodity_links_normalize_securities.cpp` | 50 | 商品关联个股、行业与 ETF |
| `commodity_links_service_fetch.cpp` | 25 | JSN 抓取和缓存 |
| `commodity_links_service_query.cpp` | 232 | 视图编排、反查、过滤和响应 |
| `commodity_links_command.cpp` | 65 | CLI 参数与输出 |

七项视图现在由 `ViewDefinition` 统一描述名称、资源、选择参数、默认排序和允许排序，编译期
拒绝重复视图。catalog 继续只展示六个用户功能，不把 catalog 自己递归列入目录；详情视图
继续兼容原先忽略无关排序参数的行为。

## 增量验证

- `tdx-commodity-links-tests` 与 `tdx-tool` 增量构建通过；
- 专项测试覆盖商品报价、题材、驱动、商品关联股与关联证券并通过；
- 实时代表样本继续返回 221 个报价行、219 个唯一商品 ID、一个来源和六个目录项；
- `research-signals` 定向契约通过，覆盖商品列表、商品详情和题材详情；
- schema 保持 `tdx-market-commodity-links-native-v1`，未运行完整 CTest。

## 后续候选

当前最大生产实现为 778 行的 `native/src/formula_scan.cpp`。
