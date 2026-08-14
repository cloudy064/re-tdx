# 原生 C++ 公司与市场契约目录模块化

日期：2026-08-10

## 目标

拆分 `recon_contract_market_corporate.cpp` 中 59 个契约 ID 组成的 3,179 行巨型
`if/else if`，让新增契约落到明确领域，而不是继续扩大单函数；不改变断言、上下文、
错误行为和契约结果 schema。

## 结构

入口文件现在只有 29 行，通过固定的
`std::array<MarketCorporateContractValidator, 6>` 依次调用六个契约族：

| 模块 | 行数 | 领域 |
|---|---:|---|
| `recon_contract_market_corporate_offerings.cpp` | 440 | 定增、配股、优先股、员工持股、港股事件 |
| `recon_contract_market_corporate_catalog.cpp` | 810 | 特殊事项、公司转型、场内基金、精选与基金统计 |
| `recon_contract_market_corporate_company.cpp` | 476 | 专项指标、公司变更、筛选、表现、订单与事件影响 |
| `recon_contract_market_corporate_ipo.cpp` | 400 | IPO 辅导、审核、申购、北证明细与美股 IPO |
| `recon_contract_market_corporate_insights.cpp` | 568 | 期货日历、机构、专利、股东、财务洞察与近期关注 |
| `recon_contract_market_corporate_integration.cpp` | 610 | 基准、扩展日历、解禁、一致预期、单票、JSN 与缓存契约 |

公共函数指针类型和六个 Validator 声明位于
`recon_contract_market_corporate_internal.hpp`，CMake 将每个契约族作为独立翻译单元。
Validator 返回 `false` 表示“不拥有该 ID”，返回 `true` 表示已经追加该契约断言；
入口 Registry 保持原来的首次命中语义。

这里使用类型化 Registry/Chain of Responsibility，不引入无状态继承层次。契约校验器
没有对象生命周期或可变状态，函数策略表比虚类更直接，也便于编译器检查签名。

## 完整性校验

机械迁移时曾检测到大型工具输出被截断。损坏的新文件没有进入构建；随后从本次
`apply_patch` 删除事件保存的权威源快照恢复原文件，再完全在本地按边界重新生成。
恢复快照的证据为：

- 原文件 3,179 行；
- 原契约匹配出现 59 次，唯一 ID 59 个；
- 六个模块合计仍为 59 次、59 个唯一 ID；
- 将六个契约链按原顺序重新拼接后，与原契约主体逐字符相同；
- 两侧字符数均为 209,405，SHA-256 均为
  `3EDE1420C3C4EDA9DE9023191ED4D334ACBA57F475C82273943D505CFE71BE80`。

## 验证

- `tdx-recon-contract-tests` 增量编译、链接通过；
- 编译过程中发现并清理五个未使用 `context` 参数名，最终无该类警告；
- `tdx-recon-contract-tests` 运行通过；
- 未运行完整 CTest 或实时 API 巡检：本轮只调整契约校验的物理边界，没有修改 API、
  传输、缓存或生产服务实现，逐字符等价检查比抽样实时调用更直接。

## 后续

当前最大的契约单文件变为 `recon_contract_market_data.cpp`（约 2,204 行）和
`recon_contract_market_research.cpp`（约 2,005 行）；生产侧热点仍包括
`disclosures.cpp`、`jsn_variants.cpp` 与 `tpool.cpp`，可继续使用同样的“内部接口 +
类型化目录 + 领域翻译单元”方式拆分。
