# 原生 C++ 云计算内建函数模块化

日期：2026-08-11

## 目标

拆解 692 行的 `cloud_calc_builtins.cpp`。原文件同时维护 TBigData 36 项恢复目录、日期与计息
算法、债券现金流/PV/YTM 算法、参数投影以及逐 ID 执行分派。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `cloud_calc_builtins_catalog.cpp` | 91 | 36 项名称/ID/参数/地址目录、查询和审计投影 |
| `cloud_calc_builtins_algorithms.cpp` | 406 | 日期、计息时间、现金流、PV 和 YTM 算法 |
| `cloud_calc_builtins_execute.cpp` | 216 | 参数语义和 36 路恢复 ID 执行策略 |

现有 `Builtin` 类型目录增加编译期 ID 与名称唯一性检查。日期/现金流算法通过模块内部端口
供执行器组合；逐 ID switch 暂时保留，以维持从 TBigData handler 恢复出的精确调用语义。
解析辅助函数恢复为翻译单元私有，避免与配置解析器同名符号冲突。

## 增量验证

- `tdx-cloud-calc-tests` 与 `tdx-tool` 增量构建通过；
- 专项测试验证 36/36 已实现目录，并实际执行 31 个非当前日期内建，错误数为零；
- `func_kzz_kzzsy101.cfg` 真实模板继续返回 19 个输入、3 个宿主字段和 15 个计算字段；
- 未改变 CFG 语法、公开 schema 或宿主数据解析，未运行完整 CTest。

## 后续候选

排除 L2 和共享 `jsn.cpp` 后，下一低风险候选为 687 行的 `native/src/recon.cpp`。
