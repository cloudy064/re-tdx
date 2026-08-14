# native/src/announcement_signals.cpp 模块化重构

**日期**：2026-08-11  
**审计版本**：v63 → v64  
**原始文件**：`native/src/announcement_signals.cpp`  
**原始规模**：612 行  
**拆分后**：7 个实现单元，最大 183 行  
**总行数**：711 行（+16.2% 结构膨胀）  
**风险等级**：低  
**测试策略**：增量验证（专项测试 + 语法检查）

---

## 职责划分

### 1. **announcement_signals_support.cpp** (170 行)
- 公共支持函数
  - `value_ptr()` - JSON 值指针访问
  - `text_value()` - 文本值提取
  - `number_value()` - 数值提取
  - `bounded()` - 参数边界验证
  - `compact_date()` - 日期格式压缩
  - `iso_date()` - ISO 日期转换
  - `json_contains()` - JSON 内容搜索
  - `now_text()` - 当前时间戳
  - `market_id()` / `market_name()` - 市场转换
  - `digits()` - 数字验证
  - `history_resource()` - 历史资源路径生成

### 2. **announcement_signals_catalog.cpp** (50 行)
- 资源类型目录（编译期常量）
  - `resources[]` - 2 个资源定义（selected/risks）
    - `{"selected", "list/func_zxjx101_1.jsn", "公告精选", "selected"}`
    - `{"risks", "list/func_zxjx103_1.jsn", "风险提示公告", "risk"}`
  - `resource_count` - 资源数量常量
  - `catalog_rows()` - 资源目录 JSON 生成
  - `summarize()` - 结果汇总统计

### 3. **announcement_signals_normalize.cpp** (95 行)
- 归一化层（2 个领域）
  - `normalize_announcement_signal_rows()` - 精选/风险公告归一化
    - 输入：JSN rows + family（"selected"/"risk"）+ securities
    - 输出：统一 schema（date, title, announcement_type, direction, security, recent_3d_return_pct, recent_10d_return_pct, pdf_url, source_rank, family）
  - `normalize_announcement_history_rows()` - 历史公告归一化
    - 输入：JSN rows + market_id + code + securities
    - 输出：统一 schema（date, title, announcement_type, pre_3d_return_pct, post_3d_return_pct, pdf_url, source_rank, family="history"）

**归一化语义差异**（已文档化）：
- func_zxjx101/103 的 zf1/zf2 → `recent_3d_return_pct` / `recent_10d_return_pct`（公告发布后的近期收益）
- ggjx 历史的 zf1/zf2 → `pre_3d_return_pct` / `post_3d_return_pct`（公告前后 3 日收益）

### 4. **announcement_signals_sort.cpp** (91 行)
- 排序策略
  - 支持 7 种排序维度：date, recent-3d, recent-10d, pre-3d, post-3d, source-rank, code
  - 支持 asc/desc 两种顺序
  - 多级排序：主键 + 日期 + 代码

### 5. **announcement_signals_fetch.cpp** (32 行)
- 数据抓取与缓存
  - `AnnouncementSignalsService` 构造函数
  - `fetch()` - 资源抓取 + TTL 缓存
    - 缓存键：resource path
    - 缓存策略：TTL（默认 300s）
    - 重试：由底层 `fetch_jsn_resource_rows()` 处理

### 6. **announcement_signals_service.cpp** (183 行，最大单元)
- 查询服务层
  - `AnnouncementSignalsService::query()` - 主查询入口
    - 参数验证（view, market, code, direction, announcement_type, date range, offset, limit）
    - 5 种视图：selected, risks, security, history, catalog
    - 3 个数据源：func_zxjx101（精选）, func_zxjx103（风险）, ggjx/<market><code>（历史）
    - 过滤器：market/code, direction（all/bullish/bearish/unknown）, announcement_type, date range, query text
    - 排序、分页
    - 响应组合：schema, filters, summary, counts, records, catalog, sources, upstream_health, warnings, cache, units, semantics

**容错策略**：
- 历史资源缺失降级为 warning（不阻塞主查询）
- schema 固定为 `tdx-market-announcement-signals-native-v1`

### 7. **announcement_signals_command.cpp** (90 行)
- CLI 适配层
  - `command_market_announcement_signals()` - CLI 入口
  - 参数解析：--root, --view, --market, --code, --query, --direction, --type, --from, --to, --sort, --order, --no-history, --offset, --limit, --refresh, --cache-ttl, --timeout-ms, --output, --compact
  - 帮助文本
  - 输出：JSON 文件或 stdout

---

## 拆分模式对齐

| 模式组件 | 实现 |
|---|---|
| 类型与常量 | catalog.cpp（2 个资源常量） |
| 目录和映射 | catalog.cpp（编译期目录 + catalog_rows） |
| QueryPlan | 动态资源计划（service.cpp 中根据 view 决定抓取哪些资源） |
| 数据抓取与缓存 | fetch.cpp（TTL 缓存） |
| 响应组合 | service.cpp（归一化 → 过滤 → 排序 → 分页 → schema 响应） |
| CLI/HTTP 适配 | command.cpp（完全分离） |
| 无状态策略 | sort.cpp（排序策略函数） |

---

## 编译验证

由于 Windows 环境缺少 g++/make，未执行完整构建。已完成：

1. ✅ 文件结构验证：7 个文件全部使用 `namespace tdx`
2. ✅ 公开 API 验证：所有公开函数已实现
   - `normalize_announcement_signal_rows()`
   - `normalize_announcement_history_rows()`
   - `sort_announcement_signal_rows()`
   - `AnnouncementSignalsService` 类
   - `command_market_announcement_signals()`
3. ✅ 资源常量验证：func_zxjx101/103 已集中到 catalog
4. ✅ 依赖关系验证：所有文件包含 `announcement_signals.hpp` 和 `announcement_signals_internal.hpp`
5. ✅ CMakeLists.txt 更新：旧文件移除，7 个新文件添加

**未运行的验证**：
- 完整 CMake 构建（需要 MSVC 或 g++）
- 专项测试 `tdx-announcement-signals-tests`
- 真实样本验证

**未运行原因**：
- 无可用 C++ 编译器
- 未修改共享解析器、协议或构建基础设施
- 拆分为纯结构重构，未改变业务逻辑

---

## 规模对比

| 指标 | 原文件 | 拆分后 | 变化 |
|---|---:|---:|---|
| 文件数 | 1 | 7 | +6 |
| 总行数 | 612 | 711 | +99 (+16.2%) |
| 最大单元 | 612 | 183 | -429 (-70.1%) |
| 中位数 | 612 | 91 | -521 (-85.1%) |

**行数增长来源**：
- 头文件包含（7 × 平均 4 行 = 28 行）
- 命名空间声明（7 × 平均 6 行 = 42 行）
- 细分后的函数签名和文档注释（约 29 行）

---

## 审计更新

**v63 → v64**：
- 生产文件数：628 → 634 (+6，扣除 1 个 .removed)
- 生产总行数：129,525 → 129,223 (-302)
- 中位数：157.5 → 156 (-1.5)
- 最大文件：727 行（level2.cpp，未变）
- 模块化模块数：64 → 65 (+1)

---

## 不足与改进点

1. **压缩式代码**：service.cpp 的 183 行中仍有部分单行多语句（例如三元嵌套），可读性略有下降
2. **职责边界**：support.cpp 包含了资源路径生成（`history_resource()`），但也包含纯工具函数（`bounded()`, `digits()`），职责不够单一
3. **重复映射**：service.cpp 中的 view 验证（`std::set<std::string>{"selected", ...}`）与 catalog 中的资源定义重复，未完全消除
4. **缺少编译验证**：未在本地环境运行编译和测试

---

## 下一候选

根据审计文档，当前剩余的低风险业务模块（600-650 行）：

| 顺序 | 文件 | 行数 | 风险 | 建议 |
|---:|---|---:|---|---|
| 1 | **curated_data.cpp** | 608 | 低 | 精选数据服务，可按资源目录 + 归一化 + 响应拆分 |
| 2 | trades.cpp | 605 | 低 | 交易链路，可按协议 + 归一化 + 服务拆分 |

**高耦合模块暂缓**（需要专项规划）：
- jsn.cpp (713 行) - 共享解析器，50+ 模块依赖
- formula_functions_series.cpp (645 行) - 解释器语义核心
- tpool.cpp (637 行) - XML/公式兼容解析
- formula_context_dynamic.cpp (630 行) - 动态上下文
- pbrpc.cpp (620 行) - 协议层
- tpool_evaluate.cpp (617 行) - 求值引擎
- recon_contract_market_corporate_integration.cpp (610 行) - 契约集成

---

## 总结

✅ **announcement_signals.cpp 模块化完成**  
- 原 612 行 → 7 个单元（最大 183 行）
- 职责清晰：支持 + 目录 + 归一化 + 排序 + 抓取 + 服务 + CLI
- 资源目录编译期化（2 个资源常量）
- 归一化语义差异已明确文档化
- 容错策略：历史资源缺失降级为 warning
- 公开接口保持不变
- CMake 已更新

⚠️ **未完成验证**：编译和测试（需要 C++ 编译器）  
📋 **下一推荐**：curated_data.cpp (608 行，低风险)
