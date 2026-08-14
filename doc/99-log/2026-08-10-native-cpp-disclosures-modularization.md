# 原生 C++ 信息披露链路模块化

日期：2026-08-10

## 结果

生产文件 `disclosures.cpp` 从 2,347 行降至 765 行，并按数据生命周期拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `disclosures_support.cpp` | 355 | 字段、证券、日期、TQLEX 行和定期报告类型共享解析 |
| `disclosures_normalize.cpp` | 365 | 五类 JSN、公告响应、关注列表与上市日期 DBF 归一化 |
| `disclosures_backfill.cpp` | 514 | 公告批量回填、断点状态与覆盖率审计 |
| `disclosures_archive.cpp` | 223 | 可用性历史合并、修订和归档摘要 |
| `disclosures_service.cpp` | 175 | 主表缓存、公告缓存、过滤与 API 文档组合 |
| `disclosures.cpp` | 765 | CLI 输入解析、维护/审计/回填流程编排 |

新增 `disclosures_internal.hpp` 作为内部边界：五张公开资源、公告入口常量和资源列表
集中定义，`PeriodicReportType` 以及共享解析函数使用显式声明。模块通过
`tdx::disclosure_detail` 共享实现，不再依赖匿名命名空间碰巧位于同一翻译单元。
公开 `tdx/disclosures.hpp` 没有变化。

这种分层把 Parser、Normalizer、Backfill Workflow、Archive Merger、Service 和 Command
分开；Service 继续拥有缓存状态，纯解析函数不被包装成无意义的有状态子类。

## 等价性与验证

拆分前保留了工作区内临时源快照；验证完成后已清理。逐段比较结果：

- normalize、backfill、archive、service、command 五段函数体逐字符一致；
- support 段仅将资源常量和 `PeriodicReportType` 移到内部头，去除该定义后的函数体在
  规范化空行后逐字符一致；
- `tdx-disclosures-tests` 和 `tdx-tool` 增量编译、链接通过；
- `tdx-disclosures-tests` 通过；
- 一次代表性真实数据执行成功：`market disclosures --view schedule --limit 1` 返回
  `tdx-market-disclosures-native-v1`、1 条记录和 5 个来源；证据为
  `output/disclosures-modularization-v44.json`。

未运行完整 CTest 或全量 API 契约；本轮没有修改公开 schema、传输或缓存语义，专项
测试、机械等价检查和一个真实数据样本已覆盖本次风险。
