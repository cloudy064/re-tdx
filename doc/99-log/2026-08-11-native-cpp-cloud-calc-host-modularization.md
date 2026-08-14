# 原生 C++ 云计算解释器宿主策略模块化

日期：2026-08-11

原 873 行 `cloud_calc_host.cpp` 把 L1 行情宿主列、封单推导、板块成员聚合、财务/转债字段
和最终绑定编排放在一起。现拆分如下：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `cloud_calc_host_internal.hpp` | 宿主值、证券索引、板块聚合对象和内部端口 | 98 |
| `cloud_calc_host_quote.cpp` | L1 快照索引和行情宿主列 | 201 |
| `cloud_calc_host_seal.cpp` | 涨跌停规则与封单金额/比例 | 128 |
| `cloud_calc_host_group.cpp` | 板块成员解析、六项聚合与 44 项已知宿主列目录 | 139 |
| `cloud_calc_host_finance.cpp` | 股本、市值、换手、行业和转债应计利息 | 145 |
| `cloud_calc_host_resolve.cpp` | CFG 引用解析、绑定/缺失审计和请求清单 | 263 |

旧根文件已移除。内部通用 JSON 查找端口显式命名为 `host_optional`，避免与云服务层同名端口
产生隐式命名空间歧义；公开 API、`tdx-tbigdata-host-fields-v1` 和来源语义未改变。

增量验证：`tdx-cloud-calc-tests` 与 `tdx-tool` 通过。测试覆盖 29 项宿主绑定、ETF IOPV、
封单、板块聚合、财务市值和转债 Actual/365。可转债模板样本仍识别 19 个输入、3 个宿主
字段、15 个计算字段，位于 `output/cloud-calc-host-refactor-template.json`。未运行完整 CTest。
