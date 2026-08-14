# 原生 C++ 可转债链路模块化

日期：2026-08-10

## 结果

生产文件 `convertible_bonds.cpp` 从 1,854 行降至 464 行，并按资源、生命周期和服务层
拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `convertible_bonds_catalog.cpp` | 38 | 八组固定 JSN 主表、投影和定价资源目录 |
| `convertible_bonds.cpp` | 464 | 字段、证券、日期、现金流、YTM 和明细共享工具 |
| `convertible_bonds_listed.cpp` | 171 | 已上市转债六主表与交换债投影归一化 |
| `convertible_bonds_issuance.cpp` | 336 | 待发行、申购、新债投影、对账和排序 |
| `convertible_bonds_pricing.cpp` | 232 | 实时价格关联、纯债价值、溢价率、YTM 与双低排序 |
| `convertible_bonds_service.cpp` | 698 | 分视图缓存、远端抓取、过滤、详情与报告组合 |
| `convertible_bonds_commands.cpp` | 70 | CLI 参数和文件输出编排 |
| `convertible_bonds_internal.hpp` | 85 | 资源常量及跨模块最小内部接口 |

固定下载地址不再埋在控制流中，统一由 catalog 模块定义。发行和定价排序继续使用已有
`map` 字段注册表；各视图归一化函数保持无状态，不增加无意义的继承层级。公开
`tdx/convertible_bonds.hpp`、四种 view、CLI 参数、缓存语义和 JSON schema 均未改变。

## 等价性与验证

拆分期间保留工作区内临时源快照，验证完成后已删除。资源目录、共享工具、上市归一化、
发行生命周期、定价、服务和命令 7 个区段逐字符一致；`CashFlow` 仅从原实现迁移到内部
头，以便估值模块共享类型。

- `tdx-convertible-bonds-tests` 与 `tdx-tool` 增量编译、链接通过；
- `tdx-convertible-bonds-tests` 通过；
- 代表性真实执行 `market convertible-bonds --view pending --limit 1` 成功，返回
  `tdx-market-convertible-bonds-native-v1`、`availability=live`、1 条记录和 3 个来源；
  当前上游汇总包含 156 个待发行项目，证据保存在
  `output/convertible-bonds-modularization.json`；
- 当前目录不是 Git 工作树，因此未执行 `git diff --check`。

未运行完整 CTest、全量 API 契约或其他三个网络 view；本轮没有修改共享传输、缓存结构
或公开 schema，专项测试、机械等价检查和一个真实上游样例已覆盖此次结构调整。
