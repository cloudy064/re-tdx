# 原生 C++ 因子链路模块化

日期：2026-08-11

## 结果

`factors.cpp` 从 1,458 行缩减为 595 行的服务工作流与缓存编排。原有实现按职责拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `factors_internal.hpp` | 67 | 类型化视图目录与内部端口 |
| `factors_support.cpp` | 297 | 视图元数据、字段转换、检索和行情补全 |
| `factors_normalize.cpp` | 125 | 因子归一化与个股反查 |
| `factors_breadth.cpp` | 251 | 市场广度聚合与成员对账 |
| `factors_snapshot.cpp` | 157 | 快照校验、差异计算与持久化 |
| `factors_commands.cpp` | 73 | `market factors` CLI 适配 |
| `factors.cpp` | 595 | 上游请求、视图组合与服务缓存 |

原先位于控制流中的 10 个视图定义已统一为只读类型化目录，集中保存视图名、请求号、
分页大小、标题和数据范围。服务仍使用原有 `FactorService` 公共接口，各种视图的请求号、
分页参数、响应 schema 和缓存键语义均未改变。

## 等价性与验证

- 拆分前后 7/7 个原实现区段逐字核对一致；
- 10/10 个因子视图及请求号核对一致；
- `tdx-factors-tests` 专项单测通过；
- `tdx-tool` 编译链接通过；
- 使用真实通达信目录请求 `catalog` 视图的一条记录成功，结果为
  `tdx-factors-native-v1`、`availability=live`、`returned=1`；
- 结构证据保存在 `output/factors-modularization.json`，样例数据保存在
  `output/factors-modularization-catalog.json`。

本轮只调整实现边界，没有修改共享解析、传输或响应 schema，依照增量验证规则没有运行
完整 CTest。当前目录不是 Git 工作树，未执行 `git diff --check`。拆分后最大的生产实现文件
是 1,419 行的 `formula_render.cpp`，可作为下一轮结构治理目标。
