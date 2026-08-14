# 原生 C++ 公式上下文的财务与板块领域拆分

日期：2026-08-10

## 拆分结果

`formula_context.cpp` 从 4,543 行降到 3,964 行，并继续作为公式上下文的组合与
调用顺序入口。新增两个领域模块：

- `formula_context_finance.cpp`（551 行）：历史流通股本、送转事件、
  `SPLIT/SPLITBARS`、`DIVFACTOR`、历史披露可用性与专业财务时点序列；
- `formula_context_blocks.cpp`（190 行）：概念/风格/指数板块成员关系、
  `INBLOCK` 私有文本、类型 167 板块代码序列和代码名称覆盖；
- 两个对应的 `*_internal.hpp` 仅暴露上下文绑定接口与 `SplitBinding` 值对象，
  没有扩展公共头文件或 API schema；
- `TDX_FORMULA_SOURCES` 显式登记新翻译单元。

主文件不再解析股本/送转记录，也不再实现板块成员代码排序；它只决定何时下载
资本变动数据、何时调用领域绑定器，以及如何继续组装其他公式上下文。

## 行为保持

- 历史 `CAPITAL` 仍按生效日期选择流通股本，并保持“股数除以 100”的手数语义；
- `SPLIT/SPLITBARS` 的类别、倒序 occurrence 和日期边界保持不变；
- `DIVFACTOR` 继续使用 bonus-only、float32 前/后复权因子；
- 历史专业财务仍采用“实际披露日之后下一根 K 线可见”，不回退到当前报告；
- 板块代码仍按 DWORD 数值升序排列，概念优先、风格随后，总数限制为 60；
- `FGBLOCK/ZSBLOCK/GNBLOCKNUM/INBLOCK/GETNAMEOFCODE` 的上下文字段和来源说明未变。

## 增量验证

- `tdx-formula-engine-tests` 通过；
- `tdx-disclosures-tests` 通过，覆盖公开的历史财务时点序列构建器；
- `tdx-tool` 增量链接通过；
- 公开 `sz:000001` 的 20 根日线自定义公式执行成功，解释器为
  `tdx-source-interpreter-v1`，输出 1 组、20 个点；
- 证据：`output/native-formula-context-modularization-v25.json`；
- 未运行完整 CTest 或 API 合约巡检，因为本次没有改动共享传输、缓存或 schema。

## 后续边界

下一轮优先处理 `formula_functions.cpp`：把函数实现按时间序列、统计、字符串、
交易/财务函数族拆分，并把固定函数注册信息集中到类型化注册表。之后再拆
`formula_context.cpp` 中横向统计和行业指数序列；这两组存在共享行情加载与板块
解析依赖，应在接口收敛后迁移，避免只按行号切割。
