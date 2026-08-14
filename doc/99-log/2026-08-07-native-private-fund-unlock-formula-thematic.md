# 私募、解禁、公式显式上下文与主题机会

> 日期：2026-08-07  
> 范围：公开 JSN 数据、TCalc 公式解释器、纯 C++ CLI/API 与 Svelte 页面；不包含 L2。

## 本轮结论

本轮把四组已有非空样本且字段语义明确的功能迁入统一纯 C++ 工具：

- `func_jgcg108_1.jsn` → `market institution-analysis --view notable-private-funds`；
- `func_jqgz103_1.jsn` → `market unlocks --view recent-large`；
- `SIGNALS_QS(id,mode)` → `formulas evaluate/audit --context-file` 显式序列绑定；
- `func_ydyl101/102 + ydyl1`、`func_rdhs101/102` →
  `market thematic-opportunities`。

服务已部署到 `http://127.0.0.1:8765`，功能目录为 110 项。完整部署态契约
`79/79` 通过；C++ 测试 `65/65` 通过，Svelte 检查为 0 错误/0 警告。

## 知名私募

`notable-private-funds` 当前返回 70 行、16 家私募管理人、68 只证券和 37 个行业，
披露持仓市值合计 23,605,585,400 元。原字段单位是万元，因此同时输出
`holding_market_value_10k` 与乘 10,000 后的 `holding_market_value`；契约包含故意
混淆两种单位的反例，防止后续回归。

这张表是披露期持仓关系，不是实时仓位。同一管理人—证券的多条披露会保留，
不会为追求唯一性而丢行。

## 近期大比例解禁

`recent-large` 当前有 48 个事件，日期范围 `20260706—20260812`。按服务运行日
2026-08-07 计算，41 个为“已解禁”、7 个为“待解禁”。状态由事件日期推导，
不再沿用下载时的陈旧文案。

客户端字段 `jjgzb` 是 0—1 比例；标准字段
`unlock_to_total_pct = unlock_to_total_ratio × 100`。该窗口与 `DBLJJ` 日历相互
独立，而且没有股东详情资源，因此不会虚构或错误拼接具体解禁股东。

## `SIGNALS_QS` 显式上下文

公式分析 schema 升级为 v5。解释器会收集常量调用所需的
`SIGNALS_QS#<id>#<mode>` 键；当前 6 条系统公式需要这类券商宿主序列。调用方可用
`tdx-explicit-formula-context-v1` JSON 按 `日期|时间` 注入真实值，`evaluate` 和
`audit` 都可执行。样本“主力密码”已验证最新 `ENTER=-1`，单公式审计 1/1 通过。

未传入完整序列时仍返回 `explicit_context_unavailable`，不会以 0 代替。该能力
只解决“已有合法序列如何进入解释器”，不负责获取券商私有数据，也没有扩大 L2
边界。

## 行业区域机会与爆炒复盘

`thematic-opportunities` 提供六个视图：`catalog`、`groups`、`group`、`security`、
`hype-completed`、`hype-active`。当前 YDYL 数据为 6 个细分行业组、8 个核心区域组、
729 条成员关系和 557 只证券；`IGCC技术` 样本可展开 10 条逐股投资逻辑。

RDHS 当前返回 1 条近期已爆炒板块龙头记录和 6 条正在爆炒股票。已爆炒记录保留
板块、龙头、起止日期、涨停结构、龙头区间收益和分析；正爆炒记录同时输出个股、
上证指数及两者差值。`JSYSP/NZJSP` 只被解释为近三月/年初至今复权收盘参考价，
不伪装成收益率。

Svelte 数据中心新增“主题机会”页面，支持行业/区域组、逐股逻辑、已爆炒和
正爆炒切换，并可从股票跳转个股工作台。

## 覆盖与证据

- JSN 类型化覆盖：`316/616`；
- 已下载文件：340，全部匹配模板，解析错误 0；
- 已下载但仅通用入口：16；
- 部署二进制 SHA-256：
  `D6089C20CA2DC19AFCC26F6CE02CD4C8771941016BF212B816221844345F730C`；
- 部署态契约报告：
  `output/probes/api-contracts-2026-08-07-thematic-formula.json`；
- JSN 覆盖报告：`output/probes/jsn-variants-after-thematic-current.json`；
- 公式样本：`output/probes/signals-qs-evaluate-current.json` 与
  `output/probes/signals-qs-audit-current.json`。

下一批优先审计剩余非空通用入口中的 `ztxx` 专题时间线、`xwlb` 新闻列表、
`dpyd` 大盘异动和 `rzrq3—8` 两融动态关系。先做与现有功能的字段级去重，再决定
是否新建页面或并入已有命令。
