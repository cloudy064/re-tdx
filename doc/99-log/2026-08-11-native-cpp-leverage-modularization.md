# 原生 C++ 杠杆资金链路模块化

日期：2026-08-11

## 目标

拆分原先将融资融券、沪深港通归一化、资源缓存、查询策略和 CLI 集中在一起的
`native/src/leverage.cpp`，保持公开服务接口与 JSON 契约不变。

## 结构调整

原文件共 1,083 行，现已移除并拆为六个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `leverage_stock_connect_normalize.cpp` | 279 | 资金流、持仓、历史、活跃股与行业归一化 |
| `leverage_support.cpp` | 265 | 字段解析、市场映射、筛选、缓存和资源抓取 |
| `leverage_stock_connect_service.cpp` | 201 | 七种互联互通视图查询策略与结果组装 |
| `leverage_margin_normalize.cpp` | 138 | 市场、转融通、证券、分类和历史归一化 |
| `leverage_margin_service.cpp` | 137 | 六种融资融券视图查询策略与结果组装 |
| `leverage_command.cpp` | 75 | margin 与 stock-connect 子命令入口 |

`leverage_internal.hpp` 集中六组共 41 个资源定义，并通过 `ResourceDefinition` 编译期目录
查找。查询服务不再持有运行时字符串 `map`；缓存仍由 `LeverageService` 实例拥有，避免把
请求状态放入全局对象。

## 兼容性

- `native/include/tdx/leverage.hpp` 未改变；
- margin 和 stock-connect 的视图、分类名称与 CLI 参数保持不变；
- `tdx-market-margin-native-v1` 与 `tdx-market-stock-connect-native-v1` 保持不变；
- 零长度当前空关系、历史快照、hsgtcg1/2 对账和金额单位转换语义保持不变。

## 增量验证

- `tdx-leverage-tests` 与 `tdx-tool` 编译、链接通过；
- `tdx-leverage-tests.exe` 执行通过；
- margin market 样例返回 3 条、availability 为 `live`；
- stock-connect northbound-total 样例返回 3 条、availability 为 `live`；
- 样例分别保存为 `output/leverage-refactor-margin-sample.json` 和
  `output/leverage-refactor-stock-connect-sample.json`；
- 没有执行完整 CTest。

## 后续候选

生产代码当前最大的单文件为 1,056 行的 `native/src/server_formula.cpp`。
