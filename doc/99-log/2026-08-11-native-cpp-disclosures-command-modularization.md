# 原生 C++ 披露命令与资源目录模块化

日期：2026-08-11

## 目标

拆解 765 行的 `disclosures.cpp`。披露归一化、归档、回填和服务此前已经独立，该文件剩余的
问题是 JSON/CSV/文本/TDX 板块输入适配与四种命令模式混在同一个 CLI 单体中。

## 落地结果

原根文件已移除，替换为三个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `disclosures_catalog.cpp` | 65 | 5 项类型化资源与归一化函数策略 |
| `disclosures_command_input.cpp` | 250 | JSON、CSV、文本、自选股和板块 universe 输入 |
| `disclosures_command.cpp` | 531 | 查询、覆盖审计、维护和批量回填命令编排 |

五项公开披露资源不再以平行字符串常量由服务手工调用；`DisclosureResourceDefinition` 同时
描述资源、事件族、市场和归一化函数，编译期检查重复路径。服务按目录顺序抓取和归一化，
保持原始来源顺序。

## 增量验证

- `tdx-disclosures-tests` 与 `tdx-tool` 增量构建通过；
- 专项测试覆盖披露日程、快报、公告报告识别、DBF 上市日期、批量续跑、覆盖审计和归档合并；
- 实时 schedule 样本汇总 6,865 行、5 个来源，并返回 3 行；
- schema 保持 `tdx-market-disclosures-native-v1`；
- 仓库没有独立披露 API 契约，本轮未运行无关契约域或完整 CTest。

## 后续候选

当前最大生产实现为 757 行的 `native/src/tqlex.cpp`。
