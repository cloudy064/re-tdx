# 原生 C++ TPool 规则引擎模块化

日期：2026-08-10

## 结果

生产文件 `tpool.cpp` 从 2,087 行降至 637 行，按职责拆为四个实现模块：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `tpool.cpp` | 637 | XML/属性解析、公式兼容性检查、流程图静态分析与池文件清单 |
| `tpool_flow.cpp` | 692 | 调度时钟、流程投影、外部状态推进与告警差异 |
| `tpool_evaluate.cpp` | 617 | 证券状态、过滤规则、公式比较、横向排名与只读求值 |
| `tpool_commands.cpp` | 261 | inspect/evaluate/watch 参数、输出与监控循环编排 |
| `tpool_internal.hpp` | 40 | 四层之间的最小内部函数端口 |

领域私有结构没有集中塞入共享头：XML 的 `Attributes/CellBlock`、流程的调度状态、求值的
证券与排名结果仍由各自模块拥有。只有跨层使用的 JSON 取值、公式库查询、操作符、时钟
和路径适配函数进入 `tdx::tpool_detail`。公开 `tdx/tpool.hpp`、CLI、JSON schema、规则
算法和只读安全边界均未改变。

这次采用模块/策略边界而非有状态继承层级：XML 检查器、流程状态机和规则求值器可以
独立演进，CLI 只负责组合它们。

## 等价性与验证

拆分期间保留了工作区内临时源快照，验证完成后已删除。11 个解析、流程、求值和命令
关键区段逐字符一致；仅将 `json_bool` 的默认参数移到内部声明，并补齐跨翻译单元的
`float_text` 声明。

- `tdx-tpool-tests` 与 `tdx-tool` 增量编译、链接通过；
- `tdx-tpool-tests` 通过；
- 代表性只读执行 `pool inspect --input output/tpool-native-fixture.xml` 成功，返回
  `tdx-tpool-catalog-v1`、1 个池、1 条函数规则和 2 只证券；证据保存在
  `output/tpool-modularization-inspection.json`；
- 当前目录不是 Git 工作树，因此未执行 `git diff --check`。

未运行完整 CTest、网络公式求值或持续 watch；本轮没有修改行情传输、公式引擎、状态
格式或公开 schema，专项测试、机械等价检查和一个只读池文件样例已覆盖此次结构调整。
