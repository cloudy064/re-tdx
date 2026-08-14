# 原生 C++ 公式运行时契约验证器模块化

日期：2026-08-10

## 结果

`recon_contract_formula_runtime.cpp` 从 1,976 行缩减为 28 行策略调度器。25 个解释器
运行时契约按语义拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `recon_contract_formula_runtime_internal.hpp` | 38 | 六类运行时验证器的类型化端口 |
| `recon_contract_formula_runtime.cpp` | 28 | 六策略职责链调度 |
| `recon_contract_formula_runtime_math.cpp` | 363 | 自定义核心、序列统计、滚动方差和基准累计 |
| `recon_contract_formula_runtime_future.cpp` | 346 | 日历过滤、方向柱、ZIGA 和确定性随机数 |
| `recon_contract_formula_runtime_security.cpp` | 424 | 复权标记、证券字符串和板块元数据 |
| `recon_contract_formula_runtime_data.cpp` | 418 | 单点数据、类型 167、市场宽度和证券关系因子 |
| `recon_contract_formula_runtime_host.cpp` | 268 | Host 汇总、证券状态和本地交易日上下文 |
| `recon_contract_formula_runtime_context.cpp` | 268 | 字符串构造、内联公式和显式上下文模板 |

主调度器采用 `std::array<FormulaRuntimeContractValidator, 6>`。公式解释器、公共 API、
断言规则和 JSON schema 均未修改，仅重组验收代码的领域边界。

## 等价性与验证

- 六个语义实现区段逐字符核对通过；
- 拆分前后的 25 个唯一契约 ID 集合完全一致；
- 六个验证器全部进入类型化策略注册表；
- `tdx-recon-contract-tests`、`tdx-tool` 编译链接通过，专项测试通过；
- 正式服务选择 `formula-sequence-statistics-inline-post`、
  `formula-ziga-inline-post`、`formula-context-template-kline-live`，结果 3/3 通过；
  报告保存在 `output/recon-formula-runtime-modularization-contracts.json`，结构证据
  保存在 `output/recon-formula-runtime-modularization.json`；
- 当前目录不是 Git 工作树，因此未执行 `git diff --check`。

依据增量验证规则未运行完整公式契约套件或 CTest。
