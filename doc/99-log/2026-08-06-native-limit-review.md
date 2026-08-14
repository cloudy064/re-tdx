# 2026-08-06 原生涨跌停复盘、原因历史与年度行为

## 客户端语义审计

本轮从 `ZDTFX.sp`、对应 GBK CFG 和八类已下载 JSN 模板恢复涨跌停复盘页，
没有按资源名猜测字段：

- `func_zdtfx101`：当日涨停原因、板型、首次/末次封板、炸板次数、连板天数和
  一年“涨停基因”；
- `func_zdtfx102`：当日跌停原因、时点、次数和连板信息；
- `func_zdtfx103`：当日涨幅超过 10% 的触发时点与原因；
- `func_zdtfx106`：年度收盘/盘中涨跌停次数、平均换手和次日高开/高收概率；
- `func_zdtfx107`：市场逐日成交额、涨跌停家数、封板/炸板、封单金额和
  1—12 板分布；
- `zdtfx1/<市场><代码>.jsn`：单票历次涨跌停原因和解释；
- `zdtfx2/3/<YYYYMMDD>.jsn`：指定交易日涨停/跌停成员。

本地审计样本分别为 231、13、494、131、484、20、231 和 13 行。五张静态
主表与三种动态键均有真实下载文件，解析错误为 0。

## 纯 C++ 能力与边界

新增 `market limit-review` 和 `/api/v1/market/limit-review`，提供五个视图：

- `view=current`：`all/limit-up/limit-down/surge` 当日复盘；
- `view=annual`：个股年度涨跌停行为；
- `view=history`：市场涨跌停温度历史；
- `view=daily&date=YYYYMMDD`：指定日涨停/跌停成员；
- `view=security&market=...&code=...`：当前/年度命中与单票原因历史。

这组 JSN 在客户端标题中明确属于逐步更新、非实时复盘数据，因此没有并入实时
`market limit-quality`。后者继续负责 `0x054B`、五档、封单和竞价质量；CFG 中
依赖宿主行情的列也没有被伪造成 JSN 返回字段。标准字段之外始终保留 `raw`，
所有来源报告 endpoint、重试、陈旧缓存和缺失状态。

## 真实数据证据

- `output/probes/limit-review-current.json`：2026-08-06 当前三类合计 177 行，
  可见证券名称、原因和涨停基因；
- `output/probes/limit-review-annual.json`：当前年度表 130 行；
- `output/probes/limit-review-history.json`：市场历史 485 日，包含成交额、封板、
  炸板和连板分布；
- `output/probes/limit-review-daily-20260731.json`：2026-07-31 涨跌停成员合计
  244 行；
- `output/probes/limit-review-security-000009.json`：深市 `000009` 取回 10 条
  历史原因；
- `output/probes/api-contracts-official-current.json`：正式服务 38/38 契约通过。

## 覆盖、验证与部署

- JSN 类型化覆盖由 220/616 提升为 228/616，通用独占由 396 降为 388；
- 已下载但通用独占由 104 降为 96，八个 `zdtfx` 模板全部标为
  `typed-command`；
- 新增 `tdx-limit-review-tests`，并为 current/history/daily/security 四条真实
  路径增加正式契约；完整原生 CTest 49/49，临时及正式契约 38/38；
- 正式服务 PID `6480`，命令行
  `dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`；
- 部署 EXE SHA-256：
  `4E4C334C738E42F84C73E963690C188FBDA8104194BF8F55FF235728A5DA090C`；
- 功能目录 95 项，`native_cpp=true`、`python_runtime=false`；本轮未修改 Svelte UI。

## 下一批高收益方向

覆盖审计当前下载行数最高的缺口为 `func_phcje101`（5,508 行）。下一轮应先从
客户端 CFG/SP 核对其页面、字段单位、刷新属性和主从关系，再决定是否独立建模；
其后按审计收益依次考虑 `dfkzz201`、`bkld102/104` 和 `qszj`，继续遵守“不按
资源名猜语义”的边界。

该方向已完成，见[原生开盘与盘后成交排行](2026-08-06-native-session-turnover.md)。
