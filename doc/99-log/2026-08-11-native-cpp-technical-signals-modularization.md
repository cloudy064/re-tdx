# 原生 C++ 技术信号链路模块化

日期：2026-08-11

## 结果

`technical_signals.cpp` 从 1,297 行缩减为 243 行的列表视图上游工作流与缓存实现。其余
职责拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `technical_signals_internal.hpp` | 143 | 类型化策略目录、统一视图目录与内部端口 |
| `technical_signals_support.cpp` | 376 | 字段转换、客户端过滤、参数和目录查询 |
| `technical_signals_normalize.cpp` | 208 | 上游行归一化与单票命中判断 |
| `technical_signals_snapshot.cpp` | 193 | 行情补全、快照校验、差异与持久化 |
| `technical_signals_security.cpp` | 111 | 单票跨视图反查 |
| `technical_signals_commands.cpp` | 115 | `market technical-signals` CLI 适配 |
| `technical_signals.cpp` | 243 | 上游请求、重试、缓存和响应组合 |

32 个技术信号视图此前分别出现在合法性校验和单票反查中，现已合并为唯一的编译期目录。
模型、竞价、因子和机会信号四组共 22 条策略定义也从控制流迁入类型化常量目录；单票反查
直接遍历统一目录，新增视图时不再需要同步两份名称列表。

## 等价性与验证

- 8/8 个未配置化的原实现区段逐字核对一致；
- 32 个视图全部唯一，22 个策略视图均被统一目录覆盖；
- `tdx-technical-signals-tests` 专项单测通过；
- `tdx-tool` 编译链接通过；
- 真实目录请求 `nine-turn` 成功，返回 `tdx-technical-signals-native-v1`、
  `availability=live` 和 1 条记录；
- 结构证据保存在 `output/technical-signals-modularization.json`，样例保存在
  `output/technical-signals-modularization-sample.json`。

本轮没有修改共享解析、传输或响应 schema，依照增量验证规则没有运行完整 CTest。当前
目录不是 Git 工作树，未执行 `git diff --check`。
