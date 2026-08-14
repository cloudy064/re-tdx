# 原生 C++ 公式语言前端、共享支持与 CLI 拆分

日期：2026-08-10

## 拆分结果

`formula_engine.cpp` 从 4,917 行降到 3,886 行。解释器现在按以下职责组织：

- `formula_language.cpp`：Token、词法分析、AST、递归下降 Parser 和带参数调用的
  上下文依赖收集；
- `formula_engine_support.cpp`：大小写归一、JSON 查询、路径转换、外部证券依赖
  判断、上下文合并与可执行性前置检查；
- `formula_commands.cpp`：`analyze/context-template/audit/evaluate` 的参数解析、文件
  读取、行情获取和输出编排；
- `formula_engine.cpp`：静态语义分析、数值/字符串求值、绘图 IR 和公共文档 API；
- `formula_functions.cpp`：内置函数实现与函数注册能力。

AST 只通过 `formula_language_internal.hpp` 在解释器内部共享；公开
`tdx/formula_engine.hpp` 没有暴露 Parser 或 Token。CMake 使用
`TDX_FORMULA_SOURCES` 集中管理全部公式翻译单元。

## 行为保持

- 源码解析统一通过 `parse_formula_program()`；
- 带编号的 `FINANCE/DYNAINFO/HORCALC/INSORT/INSUM/CALCSTOCKINDEX` 等上下文
  绑定仍由同一棵 AST 收集；
- CLI 与 API 继续复用同一分析、执行和上下文校验实现；
- 没有新增函数替代物，也没有修改解释器 schema、未来函数限制或 L2 边界。

## 增量验证

- `tdx-formula-engine-tests` 构建并通过；
- `tdx-tool` 链接通过；
- 用公开 `sz000001` 的 20 根日线执行自定义
  `RESULT:CLOSE>MA(CLOSE,N)`，`N=5`；
- 返回 `tdx-source-interpreter-v1`、`native-cpp`、20 根、语法与执行均为 true；
- 证据：`output/native-formula-cli-modularization-v24.json`；
- 未运行完整 CTest 或完整 API 巡检。

下一轮优先拆分 `formula_context.cpp` 的证券元数据、板块、财务时点、横向统计与
市场汇总绑定；随后处理 `formula_functions.cpp` 的函数族注册和实现分组。

