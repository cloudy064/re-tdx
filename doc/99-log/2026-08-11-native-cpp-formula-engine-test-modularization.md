# 原生 C++ 公式解释器测试模块化

日期：2026-08-11

## 问题

`formula_engine_tests.cpp` 原有 4,393 行。359 个断言调用全部位于同一个 `main()`，任何失败
只能从断言文本反查位置，且修改渲染测试也会重新编译上下文构建和全库审计 fixture。

## 新结构

| 单元 | 测试域 | 行数 |
|---|---|---:|
| `formula_engine_tests.cpp` | 五域类型化调度和错误域标注 | 37 |
| `formula_engine_test_support.cpp/.hpp` | 公共 K 线、筹码、期货和结果 fixture | 118/45 |
| `formula_engine_context_builder_tests.cpp` | 本地板块、外部序列、行业估值上下文 | 537 |
| `formula_engine_language_tests.cpp` | 语言、序列、窗口、日历、筹码语义 | 1,147 |
| `formula_engine_workflow_tests.cpp` | 扫描、观察池、回测和时点财务 | 130 |
| `formula_engine_render_tests.cpp` | 展示分析与 render IR | 869 |
| `formula_engine_context_library_tests.cpp` | 自动/显式上下文、原生指标和全库审计 | 1,417 |

入口使用 `constexpr std::array<TestDomain, 5>`，仍生成一个测试可执行文件，因此没有把
专项验证膨胀成五次进程启动。域内异常会附加 `context-builder`、`language`、`workflow`、
`render-ir` 或 `context-and-library` 前缀。

## 完整性与验证

- 原文件历史内容从本地会话补丁恢复后按连续行域迁移，没有用截断或删除断言换取编译；
- 五个域连续覆盖原 `main()` 的全部测试主体；
- 当前源中仍有 359 个 `require()` 断言调用；
- `tdx-formula-engine-tests` 增量构建并完整执行通过；
- 未修改解释器生产语义，因此未运行完整 CTest。

维护规则已写入 `AGENTS.md`：测试 `.cpp` 达到 2,500 行时优先按语义域拆分，同时保留
共享 fixture 下的单一专项测试入口。
