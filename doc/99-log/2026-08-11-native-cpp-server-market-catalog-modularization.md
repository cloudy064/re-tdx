# 原生 C++ 市场查询适配器模块化

日期：2026-08-11

原 814 行 `server_market_catalog.cpp` 名为 catalog，实际包含 33 个互相独立的 HTTP 查询
适配器。它不是单一固定注册表，因此本轮按服务领域整体迁移函数并移除旧根文件：

| 文件 | 领域 | 适配器数 | 行数 |
|---|---|---:|---:|
| `server_market_catalog_funds.cpp` | 主动基金、ETF、转债、债券参考、交易所基金和基金日历/统计 | 7 | 187 |
| `server_market_catalog_discovery.cpp` | 证券目录、经济指标、战略主题、主题库、机会、日历和员工 | 7 | 202 |
| `server_market_catalog_corporate.cpp` | 港股事件、特殊情形、精选数据、公司变化、GDR、订单、事件和股东研究 | 12 | 277 |
| `server_market_catalog_analytics.cpp` | 专项指标、财务筛选/洞察、权益/全球表现、概览和基准分析 | 7 | 168 |

`server_market_catalog_internal.hpp` 的 33 项声明保持不变，路由表和公开 URL 无需调整。拆分后
静态核对声明与定义集合严格相等且无重复，`tdx-tool` 增量链接通过。

验证使用不监听端口的 `tdx-tool serve --self-test`：返回 `ok=true`，149 项功能完成初始化，
JSN 可用，公式匹配为 5，银行板块 1 个/42 条成员关系，证券样本返回 34 个板块。正式 8765
服务未启动、停止或替换；按精简验证规则未运行完整 API 或 CTest。
