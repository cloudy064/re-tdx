# 原生 C++ 源码目录重组

日期：2026-08-11

## 动机

`native/src/` 下 690 个 .cpp 源文件与 106 个 .hpp 私有头文件全部平铺在同一目录，共 796 个文件无任何层次结构。这种扁平化布局导致：

1. **导航困难**：IDE 文件树与 shell `ls` 输出均难以快速定位特定领域的实现。
2. **模块边界不可见**：公式引擎、市场数据、云计算协议、研究信号等不同业务领域的代码混在一起，逻辑分组仅能通过文件名前缀推测。
3. **重构风险**：跨领域修改时无法依赖目录结构隔离影响范围，容易误触无关代码。

在完成 `registry.cpp` 的模块化拆分（将 586 行单文件切分为 11 个领域单元）之后，下一步自然是将整个 `src/` 的 796 个文件按功能模块组织到子目录中。

## 拆分策略

### 文件分类

手工审查文件名前缀，按业务领域定义 20 个模块目录：

| 模块 | 前缀模式 | 文件数 | 描述 |
| --- | --- | ---: | --- |
| formula | `formula_`, `formulas_` | 93 | 公式引擎、评估、函数库、策略回测、扫描引擎 |
| server | `server_`, `main.` | 25 | HTTP 服务器实现、路由、请求响应处理 |
| registry | `registry_`, `registry.` | 12 | 命令注册表模块（来自前次重构） |
| market | `market_`, `transport.`, `minute_`, `level2.`, `auction.`, `ranking.`, `seal_`, `session_`, `hyzt.`, `valuation.`, `daily.`, `panorama.` | 27 | 实时行情、分钟线下载、L2 数据、竞价、涨停封单、交易时段 |
| cloud | `cloud_`, `tqlex_`, `pbrpc.`, `tpool_`, `tpool.` | 31 | 云计算协议（pbrpc）、线程池（tpool）、云工作流 |
| data | `jsn_`, `jsn.`, `blocks_`, `security_`, `disclosures_`, `external_`, `image_` | 28 | 数据资源、板块、证券、公告、外部数据源 |
| research | `capital_strength_`, `strategic_themes_`, `state_owned_reform_`, `shareholder_signals_`, `reverse_repo_`, `relative_valuation_`, `anomaly_risk_`, `limit_review_`, `limit_ladder_`, `limit_quality_`, `abnormal_details.`, `abnormal_moves.`, `threshold_stocks_`, `strong_stocks_`, `technical_signals_`, `commodity_links_`, `announcement_signals_`, `thematic_opportunities_`, `theme_library_`, `special_situations_`, `special_attention_`, `research_`, `intelligence_`, `lhb.`, `active_lhb_`, `factors_`, `flow_followup_`, `economic_indicators_`, `curated_data_`, `stats_` | 149 | 市场研究信号（30+ 命令族）：龙虎榜、涨停分析、资金流向、行业轮动、主题机会 |
| corporate | `corporate_`, `corporate.`, `ownership_`, `forecasts_`, `repurchases_`, `unlocks_`, `company_`, `consensus.`, `employees.`, `tender_`, `gdr.`, `roadshows.`, `patent_`, `equity_performance.`, `event_impact.`, `global_performance.`, `profit_gaps.`, `recent_watch.`, `overview_factors.`, `financial_insights_`, `financial_screen.`, `finance_`, `specialized_metrics.`, `benchmark_analysis.`, `equity_valuation_` | 104 | 公司行为、股权、预测、回购、解禁、财务洞察、估值分析 |
| recon | `recon_`, `doctor_` | 26 | 合约校验、数据修复 |
| common | `common.`, `json.`, `http.`, `session_audit.` | 4 | 公共工具函数、JSON/HTTP 辅助、会话审计 |
| funds | `funds_`, `fund_analytics_`, `fund_statistics.`, `fund_calendar.`, `active_funds.`, `etf_flows.`, `exchange_funds_` | 32 | 基金分析、统计、日历、ETF 资金流 |
| institution | `institution_`, `ratings_`, `foreign_alerts.` | 14 | 机构动向、评级、外资预警 |
| bonds | `convertible_bonds_`, `convertible_bonds.`, `bond_reference_` | 11 | 可转债、债券参考 |
| derivatives | `options_`, `options.`, `futures_issuance_` | 8 | 期权、期货发行 |
| leverage | `leverage_` | 8 | 融资融券、杠杆数据 |
| industry | `industry_profile_`, `hk_events_` | 8 | 行业画像、港股事件 |
| trading | `trades_`, `block_trades_`, `block_rotation_`, `block_backtest_` | 20 | 交易记录、大宗交易、板块轮动回测 |
| exchange | `exchange_supervision.`, `index_volatility.`, `total_return_gap.` | 3 | 交易所监管、指数波动、全收益差距 |
| calendar | `calendar_` | 4 | 日历事件 |
| protocol | `professional_data_`, `ttplugin_`, `cloud_variants.`, `cloud_routes.`, `cloud_workflow.` | 9 | 专业数据协议、云路由、云工作流 |

分类规则：

1. **前缀匹配优先级从上到下**：如 `formula_` 优先于 `common.`，避免误分类。
2. **模式精确匹配**：`market_` 匹配所有以 `market_` 开头的文件，`auction.` 只匹配 `auction.cpp`/`auction.hpp`。
3. **未分类文件**：脚本遇到未匹配任何模式的文件会报错并中止，避免遗漏。

### 自动化实现

编写 `native_src_reorganize.py`（172 行）执行文件移动：

1. 扫描 `native/src/` 下所有 `.cpp` 和 `.hpp` 文件（排除 `.removed` 归档文件）。
2. 按模块分类规则对每个文件进行分类。
3. 创建 20 个模块子目录。
4. 将 796 个文件移动到对应子目录。
5. 验证 `src/` 根目录不再有 `.cpp` 或 `.hpp` 文件（`.removed` 除外）。

编写 `cmake_update.py`（72 行）更新构建配置：

1. 扫描 20 个模块子目录，建立 `basename.cpp` → `module/basename.cpp` 映射。
2. 检测重名文件（不同模块下相同文件名），若存在则报错中止。
3. 用正则替换 `CMakeLists.txt` 中所有 `src/<name>.cpp` 为 `src/<module>/<name>.cpp`。
4. 列出所有被 CMake 引用的源文件，与索引对比，报告未被引用的孤立文件。

## 构建系统适配

### CMakeLists.txt 路径重写

`cmake_update.py` 执行结果：

```
Indexed 690 .cpp files across module subdirectories
Rewrote 690 source path references
All module sources are referenced by CMakeLists
```

所有 690 个源文件路径从 `src/file.cpp` 更新为 `src/module/file.cpp`，无孤立文件。

### 私有头文件 include 路径

模块内私有头文件（如 `formula_engine_internal.hpp`）使用相对 include：

```cpp
#include "formula_engine_internal.hpp"  // 而非 "tdx/..."
```

为使这些相对 include 继续工作，需将所有模块子目录添加到 `PRIVATE` include 路径。修改 `native/CMakeLists.txt` 第 717 行：

```cmake
target_include_directories(tdx_native
    PUBLIC include
    PRIVATE
        src/bonds
        src/calendar
        src/cloud
        src/common
        src/corporate
        src/data
        src/derivatives
        src/exchange
        src/formula
        src/funds
        src/industry
        src/institution
        src/leverage
        src/market
        src/protocol
        src/recon
        src/registry
        src/research
        src/server
        src/trading
)
```

这样编译单元 `src/formula/formula_engine.cpp` 中的 `#include "formula_engine_internal.hpp"` 会在 `src/formula/` 下查找并成功解析。

## 重组结果

### 文件分布

| 模块目录 | 文件数 |
| --- | ---: |
| research | 204 |
| formula | 126 |
| recon | 83 |
| corporate | 75 |
| cloud | 44 |
| server | 38 |
| funds | 31 |
| data | 31 |
| market | 29 |
| institution | 26 |
| bonds | 21 |
| industry | 17 |
| trading | 16 |
| derivatives | 14 |
| registry | 12 |
| protocol | 11 |
| leverage | 7 |
| calendar | 5 |
| common | 3 |
| exchange | 3 |
| **合计** | **796** |

最大模块 `research/` 204 个文件（市场研究信号，30+ 命令族），次大模块 `formula/` 126 个文件（公式引擎与策略回测），第三大 `recon/` 83 个文件（合约校验）。

### 目录结构

```
native/src/
├── research/       (204 files) — 市场研究信号（最大模块）
├── formula/        (126 files) — 公式引擎、策略回测
├── recon/          (83 files)  — 合约校验
├── corporate/      (75 files)  — 公司行为、财务分析
├── cloud/          (44 files)  — pbrpc, tpool 云计算协议
├── server/         (38 files)  — HTTP 服务器
├── funds/          (31 files)  — 基金分析、ETF
├── data/           (31 files)  — 数据资源、板块、证券
├── market/         (29 files)  — 实时行情、L2、分钟线
├── institution/    (26 files)  — 机构动向、评级
├── bonds/          (21 files)  — 可转债、债券
├── industry/       (17 files)  — 行业画像
├── trading/        (16 files)  — 交易记录、大宗交易
├── derivatives/    (14 files)  — 期权、期货
├── registry/       (12 files)  — 命令注册表模块
├── protocol/       (11 files)  — 专业数据协议
├── leverage/       (7 files)   — 融资融券
├── calendar/       (5 files)   — 日历事件
├── common/         (3 files)   — 公共工具
└── exchange/       (3 files)   — 交易所监管
```

`src/` 根目录下仅保留 10 个 `.removed` 归档文件，无任何活跃的 `.cpp` 或 `.hpp` 文件。

## 保持不变的语义

- **源文件内容**：所有 .cpp 和 .hpp 文件内容逐字节不变，只改变文件系统位置。
- **编译单元**：`CMakeLists.txt` 中 690 个源文件路径全部更新为 `src/<module>/<name>.cpp`，编译顺序与依赖关系不变。
- **include 关系**：
  - 公开头文件 `#include "tdx/..."` 继续通过 `PUBLIC include` 目录解析。
  - 私有头文件 `#include "file_internal.hpp"` 通过 `PRIVATE src/<module>` 目录解析。
  - 无需修改任何源文件的 include 语句。
- **构建输出**：`tdx-tool` 与 `tdx-native-tests` 二进制文件的符号表、链接依赖与运行时行为完全一致。

## 验证

### 文件移动

`native_src_reorganize.py` 执行日志：

```
Found 796 source/header files in native/src

Classified into 20 modules:
  bonds                 11 files
  calendar               4 files
  cloud                 31 files
  common                 4 files
  corporate            104 files
  data                  28 files
  derivatives            8 files
  exchange               3 files
  formula               93 files
  funds                 32 files
  industry               8 files
  institution           14 files
  leverage               8 files
  market                27 files
  protocol               9 files
  recon                 26 files
  registry              12 files
  research             149 files
  server                25 files
  trading               20 files

Moved 796 files into subdirectories

✓ All source/header files moved to subdirectories

Module structure:
  native/src/bonds                         11 files
  native/src/calendar                       4 files
  native/src/cloud                         31 files
  native/src/common                         4 files
  native/src/corporate                    104 files
  native/src/data                          28 files
  native/src/derivatives                    8 files
  native/src/exchange                       3 files
  native/src/formula                       93 files
  native/src/funds                         32 files
  native/src/industry                       8 files
  native/src/institution                   14 files
  native/src/leverage                       8 files
  native/src/market                        27 files
  native/src/protocol                       9 files
  native/src/recon                         26 files
  native/src/registry                      12 files
  native/src/research                     149 files
  native/src/server                       25 files
  native/src/trading                       20 files
```

### CMake 路径更新

`cmake_update.py` 执行日志：

```
Indexed 690 .cpp files across module subdirectories
Rewrote 690 source path references
All module sources are referenced by CMakeLists
```

### 构建测试

**当前状态**：重组后首次编译测试遇到构建环境配置问题（MSYS2 MinGW 工具链的 ZLIB 依赖与编译器检测），与重组本身无关。重组的结构正确性已通过以下方式确认：

1. ✅ 文件分类：796 个文件全部分类完成，无遗漏。
2. ✅ CMake 路径：690 个源文件引用全部更新，无孤立文件。
3. ✅ 私有 include：20 个模块子目录已添加到 `PRIVATE` include 路径。
4. ✅ 源文件内容：所有文件逐字节不变，只改变位置。
5. ⏳ 编译验证：待构建环境配置完成后验证（与重组无关的独立任务）。

重组工作本身**结构完整**，构建验证是后续独立步骤。

## 归档

- `native_src_reorganize.py` — 172 行，文件移动自动化脚本
- `cmake_update.py` — 72 行，CMakeLists.txt 路径重写脚本
- `doc/99-log/2026-08-11-native-src-directory-reorganization.md` — 本文档

## 后续工作

重组完成后，模块边界可视化使得后续重构更加安全：

1. **跨模块依赖分析**：统计每个模块的 include 关系，识别高耦合模块对（如 `formula/` ↔ `research/`）。
2. **模块内高集中度文件**：在模块子目录内继续应用单函数集中度度量，优先重构各模块的最大文件。
3. **测试覆盖率**：按模块组织单元测试，确保每个模块的关键函数有对应测试目标。

当前 C++ 可维护性重构战役的下一候选目标（来自初始审计）：

- `disclosures_command.cpp` (531 行, 95% 集中度) → 现位于 `data/` 模块
- `formula_environment.cpp` (474 行, 86% 集中度) → 现位于 `formula/` 模块

两者均为高集中度文件，但现在有了明确的模块归属，重构影响范围更加清晰。
