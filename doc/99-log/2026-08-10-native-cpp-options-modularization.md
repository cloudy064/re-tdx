# 原生 C++ 期权目录与分析模块化

日期：2026-08-10

## 结果

生产文件 `options.cpp` 从 1,590 行降至 336 行，并按目录、规则解释和分析工作流拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `options_internal.hpp` | 49 | 二叉树节点常量和跨模块最小内部端口 |
| `options.cpp` | 336 | 期权代码解析、7727 目录归一化、筛选与目录缓存 |
| `options_expiry.cpp` | 364 | `FutureRule.ini`、节假日和合约到期日规则解释器 |
| `options_pricing.cpp` | 286 | 欧式/美式、现货/期货期权定价、希腊值与隐含波动率 |
| `options_volatility.cpp` | 158 | 单合约历史/隐含波动率工作流 |
| `options_chain.cpp` | 356 | 行情批量关联、波动率曲面、PCR、最大痛点与链抓取 |
| `options_commands.cpp` | 224 | options/expiry/volatility/chain 四个 CLI 编排 |

到期日仍由通达信本地 `FutureRule.ini` 元数据和节假日配置驱动，没有退化为产品硬编码。
二叉树节点数集中在内部头；市场别名和分析输入通过显式内部端口共享。公开
`tdx/options.hpp`、目录缓存、模型公式、CLI 参数和 JSON schema 均未改变。

这一分层对应 Catalog Repository、Expiry Rule Interpreter、Pricing Strategy、Volatility
Workflow、Chain Analyzer 和 Command Adapter；纯数学函数仍保持无状态。

## 等价性与验证

拆分期间保留工作区内临时源快照，验证完成后已删除。目录辅助、行情辅助、代码解析、
目录、到期日、定价、波动率、期权链和命令等 11 个关键区段逐字符一致；二叉树节点数
仍为 20，`json_number_or` 的默认参数只保留在内部声明。

- `tdx-options-tests` 与 `tdx-tool` 增量编译、链接通过；
- `tdx-options-tests` 通过，覆盖合约解析、到期日规则、定价和期权链分析；
- 代表性真实执行 `market options --market cffex --limit 1` 成功：活跃目录匹配
  730 个 CFFEX 期权，返回 `7:HO8W03UX`，目录报告总量 143,502；证据保存在
  `output/options-modularization.json`；
- 当前目录不是 Git 工作树，因此未执行 `git diff --check`。

未运行完整 CTest 或实时波动率/期权链抓取；本轮没有修改共享 transport、模型公式或
公开 schema，专项测试、机械等价检查和一个真实目录样例已覆盖此次结构调整。
