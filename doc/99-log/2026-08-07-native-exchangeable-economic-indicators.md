# 可交换债条款补齐与经济指标关系链

## 目标

按 JSN 覆盖审计的实际信息增益继续收敛缺口，同时复审 TCalc 公开数据
上下文下的全库运行边界。这一阶段不新建 Python 后端，命令、HTTP API、数据规范化和
缓存均由 C++ 完成。

## 可交换债补充表

`list/kjhz_kjhzsy201_1.jsn` 当前两行：`SH132024 26江铜EB`与
`SH132026 G三峡EB2`，分别关联江西铜业和长江电力。它与已有六张可转债表存在
代码交集，但为可交换债提供了原先缺失的核心条款。

实现将其作为 `market convertible-bonds view=listed` 的可选第七源：

- 增加 `instrument_type=exchangeable-bond` 与 `exchangeable_supplemented`；
- 补齐面值、发行价、发行/上市/到期日、换股价/期、剩余年限、主体/债项
  评级和三类收益字段；
- CFG 字段语义证明 `FXTS` 是风险提示，不再错标为发行人；
- 补充表上游不可用时返回 `partial/master_errors`，不阻断原有六表。

`SH132024` 实测返回面值 100、上市日 2026-04-20、换股价 52.4、到期日
2031-04-09、发行人评级 AAA；整个已上市目录中可交换债 2 只、两只均完成补充。

## 经济指标—历史—股票

`C:\new_tdx\T0002\cloud_pad\JJZB.sp` 页面标题明确为“经济指标”。三个单元的关系为：

| 单元 | 资源 | 语义 |
| --- | --- | --- |
| 21301 | `list/func_jjzb101_1.jsn` | 指标目录及当前值 |
| 21302 | `jjzb1/<indicator_id>.jsn` | 日期—数值历史 |
| 21303 | `jjzb2/<indicator_id>.jsn` | 关联证券及细分行业 |

新鲜主表为 72 项，其中价格指标 71、景气指数 1；日/周/季频率分别为
59/12/1，最新日 2026-08-06。对 72 个指标的 144 个动态键全量探测：
`jjzb1` 72/72 非空，`jjzb2` 72/72 非空，缺失为 0。

新增纯 C++ `market economic-indicators`，支持 `catalog|indicator` 视图、文本检索、分页、
排序、历史/关联股/行情独立开关、缓存和来源健康。关联股的名称、现价、涨跌、
成交额不在 `jjzb2` 原字段中，只在公开 `0x054C` 请求成功时合并。

阴极铜样本 `M2800000005` 实测为当前值 107170 元/吨、100 个历史点、
139 只关联股票。Svelte 数据中心新增“经济指标”页，使用 Lightweight Charts
展示可滚轮缩放/拖拽的历史曲线，关联股可跳转个股工作台。截图回归发现并修复了
图表面板未声明 `fill` 导致画布被压成细带的布局问题。

## 公式解释器复审

以平安银行、800 根日线、完整公开市场上下文和显式只读未来函数开关重跑
`formulas audit`：

- 报告 379/379，未漏报 0；
- 通过 354，解释器错误 0，产生数值 354；
- 依赖不可用 20：14 条为 `L2_AMO`/大单与逐笔派生字段，6 条为券商私有
  `SIGNALS_QS`；
- 市场不适用 4，周期不适用 1；
- 20 条只读未来函数全部进入可执行路径；
- 352 条最新点有数值，两条云端涨跌停广度因上游日期对齐在最新点为空，
  但历史序列有数值，不是解释器错误。

因此当前没有普通 A 股公开非 L2 语法缺口；剩余 20 条不应用零值或推测数据
伪装闭合。

## 验收与证据

- CTest：63/63；
- Svelte：0 错误、0 警告，生产构建成功；
- 新增契约：3/3；正式服务 full：71/71；
- JSN 覆盖：284/616 类型化、332 个通用独占、340 个下载文件全部匹配、
  296 个已下载模板、解析错误 0；
- 正式服务 PID `5348`，功能目录 108 项；
- 部署 EXE SHA-256：
  `888F7107C85E8A4FB1B239D5EBC252C779EF87E8274DCDD207D83793D829CB58`；
- 可交换债证据：`output/probes/exchangeable-bonds-current/`；
- 经济指标证据与页面截图：`output/probes/economic-indicators-current/`；
- 公式审计：`output/probes/formula-runtime-audit-current.json`；
- 全量契约：`output/probes/api-contracts-official-current.json`；
- 覆盖审计：`output/probes/tdx-jsn-variants-current.json`。

## 下一步

> 后续更新：下述 24 张战略主题主表已经由纯 C++ `market strategic-themes`
> 固定化，不再是待办；详见[战略主题迁移记录](2026-08-07-native-strategic-themes.md)。

下载缺口榜的下一类高分资源不是单一页面，而是
`func_5G101/bdt/dcl/djr/dxf/dzq/fglc/fjcjj/gcr/gfjg/gy/...` 多张同构专题分组。
它们都使用 `$S_ZQDM/$ZQDM/S_NUM/gname` 成员串模式，适合先审计与已有
概念/战略主题的独立信息增益，再用一个通用引擎产品化。
