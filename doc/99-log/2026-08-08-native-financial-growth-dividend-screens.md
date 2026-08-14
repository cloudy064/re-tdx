# 稳健成长、质量增长、利润突破与当前分红方案闭环

> 日期：2026-08-08。范围：7709 JSN 元数据探测、客户端 GBK CFG、纯 C++
> CLI/API、Svelte 数据中心和个股工作台；不依赖 Python、账号态或 Level2。

## 候选重排

旧的缺口榜把“尚未下载”与“远端非空”混在一起，导致已确认不存在的资源持续
占据最高分。使用纯 C++ `jsn download --probe --skip-missing` 对当前 108 个静态
`generic-only` 模板只查询长度和 MD5，不下载正文：73 个非空，35 个零长度。
这与此前文档记录的空表集合一致，也说明后续排序必须先看远端存在性，再看体量、
字段闭合度和已有能力重叠。

本轮没有先搬运 15–40 MB 的债券全量镜像。四张小表拥有明确 CFG、直接证券键、
基础单位和可验证公式，且能补强现有财务洞察，因此收益更高。

## 下载证据

四个资源通过 7709 `709→1721` 原生下载器下载，按远端 MD5 验证并由 JSN 解析器
确认单组、零错误：

| 资源 | 字节 | 行数 | MD5 |
| --- | ---: | ---: | --- |
| `list/func_wjcg101_1.jsn` | 27,209 | 363 | `9ca47af904312dadb2f7a84a4b50d1bd` |
| `list/func_lxsnzz101_1.jsn` | 17,736 | 108 | `9754f5d142b1080d4bc35352a5550cba` |
| `list/func_tqwclr101.jsn` | 10,245 | 104 | `f19b7768514bf4182fe145332e1a3114` |
| `list/func_qxfa101_1.jsn` | 55,329 | 441 | `a830bea67ed10ea3e6c0caa9fa0b4f9d` |

新增 1,016 条后，`market financial-insights` 从十一类 1,464 条扩展为十五类
2,480 条，来源严格为 15 张表。

## 字段和公式边界

- `steady-growth`（稳健成长）保留三年前参考收盘价、上市以来涨幅、累计分红、
  分红次数、累计净利润、Beta 和动态 PE。`ljfh` 是亿元，另给乘 `1e8` 的元值；
  分红率按 `ljfh/(zjlr/1e8)*100` 复算。当前价和近三年涨幅是宿主列，JSN 未下发，
  因而不伪造。
- `quality-growth`（连续质量增长）保留三年营收、销售费用率、毛利率和研发费用，
  并逐项复现“营收递增、研发费用递增、毛利率递增、销售费用率递减”四个条件。
  两段营收/研发增长率只按相邻原值计算。
- `profit-breakout`（利润突破）保留最新或预告利润、去年同期扣非利润和比较报告期
  扣非利润，分别输出同比和相对比较期的变动额/增长率，公式与 CFG 相同，分母
  不改成绝对值。
- `dividend-plan`（当前分红方案）保留公告日、分红年度、登记/除息日、方案进度、
  行业、每十股送转/现金分红和一周/一月/三月表现。每十股值另给除以 10 的每股
  伴生字段；股息率依赖当前价且未写入 JSN，不做静态推断。

Svelte“特色财务线索”页增加四个切换和对应核心指标；个股工作台复用同一 API
自动显示单票命中结果。CLI 仍通过 `market financial-insights --view ...` 调用，
没有 Python 转发。

## 覆盖、验证与部署

- JSN 审计：618 个模板中 514 个类型化，533 个下载文件全部匹配；解析错误 0，
  已下载 `generic-only` 仍为 0。四张新表均映射到固定命令和 API。
- 新增归一化单元样本覆盖亿元转元、分红率、四项质量条件、两类利润增长率和
  每十股转每股；CTest 全量 96/96。
- Svelte `check` 为 0 错误、0 警告，生产构建通过。
- 预部署新增契约 1/1、预部署 full 145/145；正式新增财务和公式复跑 2/2，
  正式 full 145/145。第一次正式 full 的公式组合扫描曾因一次约 10 秒行情链路
  不完整得到 144/145，单项立即复跑通过，第二次 full 通过，未掩盖该瞬时记录。
- 正式服务为 `127.0.0.1:8765`，PID `8644`；健康信息为 533 个 JSN、
  `native_cpp=true`、`python_runtime=false`。
- 安装版 SHA-256：
  `AED62B27E2439C8038553247EFA47A71CEA3CB5CDABEDCA1F2923733A18EA81A`。

主要报告：

- `output/tdx-jsn-variants.json`；
- `output/api-contracts-financial-insights-next.json`；
- `output/api-contracts-financial-insights-formal-predeploy-full.json`；
- `output/api-contracts-financial-insights-formal-selected.json`；
- `output/api-contracts-financial-insights-formal-full.json`。

## 后续

剩余 104 个未类型化静态模板中，当前远端非空候选减为 69 个。下一轮应优先把
73/35 探测结果固化进审计排序，防止零长度资源反复进入高价值榜；业务上可从
债券分类目录、涨停主题快照或其他字段闭合的小型筛选表中继续选择，先排除已有
转债、战略主题和分红能力的重复项。
