# 原生 C++ 云端公式计算应用层模块化

日期：2026-08-11

## 背景

`cloud_calc.cpp` 原有约 1,214 行。底层已经存在 config、builtins、interpreter 和 host
四个模块，但外围的配置审计、模板生成、单行计算、批量计算和 CLI 仍放在同一翻译单元，
导致解释器改动和命令行改动拥有相同的重编译边界。

## 新边界

| 文件 | 职责 | 行数 |
|---|---|---:|
| `cloud_calc_service_internal.hpp` | 应用层内部端口和 4 个 schema 常量 | 43 |
| `cloud_calc_service_support.cpp` | 路径、输入文档、请求校验和批量辅助 | 327 |
| `cloud_calc_audit.cpp` | CFG、内建函数、依赖和主机字段覆盖审计 | 215 |
| `cloud_calc_evaluation.cpp` | 单 CFG 求值和确定性 host 入口 | 80 |
| `cloud_calc_template.cpp` | 最小可编辑输入模板生成 | 229 |
| `cloud_calc_request.cpp` | 单行 API 编排和公开 L1/财务补全 | 120 |
| `cloud_calc_batch.cpp` | 共享抓取计划的有界批量求值 | 189 |
| `cloud_calc_command.cpp` | CLI 参数、离线文件和输出编排 | 261 |

原 `cloud_calc.cpp` 已移除。此次没有改动表达式解释器、内建函数计算、host 字段规则、
公开函数签名或 JSON schema。四个 schema 名称集中为只读常量，避免应用层各自写字面量。

## 验证

- 增量构建 `tdx-cloud-calc-tests` 和 `tdx-tool` 通过；
- `tdx-cloud-calc-tests` 专项直测通过；
- 代表性离线模板 `func_kzz_kzzsy101.cfg` 返回 19 个输入字段、3 个 host 字段、15 个
  计算字段，schema 为 `tdx-tbigdata-cloud-calc-template-v1`；
- 按精简测试规则未运行完整 CTest。

样例位于 `output/cloud-calc-refactor-template-sample.json`，全局尺寸数据位于
`output/native-cpp-maintainability-audit.json`。
