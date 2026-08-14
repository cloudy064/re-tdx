# 沪深港通十四模板语义补齐

## 缺口与客户端语义

JSN 覆盖审计中的 `hsgt` 族原有 14 个模板没有绑定类型化业务命令。其中 7 个
当前可下载，共 1,405 行；另外 7 个配置资源由服务器明确报告为零字节。结合
客户端 GBK CFG/SP 恢复出以下业务分支：

- `func_hsgt107/110/111/112`：北向行业、概念、风格及全部分类；
- `func_hsgt203`：陆股通当前持仓；
- `func_hsgt205—211`：日增/日减、连续增/减、五日增/减和二十日频繁增仓；
- `func_hsgt301`：南向港股行业资金；
- `hsgt/<YYYYMMDD>2.jsn` 与 `hsgt/<YYYYMMDD>4.jsn`：指定日期沪股通、
  深股通活跃股票。

当前 `107/110/111/112/203/209/210` 是配置存在但零字节的资源。它们表示当前
关系为空，接口必须返回 `availability=empty`、空记录和 `missing=true` 来源，
不能把正常空表转成 HTTP 失败，也不能虚构记录。

## 纯 C++ 实现

扩展已有 `market stock-connect` 和 `/api/v1/market/stock-connect`：

- `view=activity` 提供七类增减仓榜，统一持股数、变化、比例、连续天数和净买额；
- `view=industry` 提供北向分类和南向港股行业资金，派生五日/一月涨跌；
- `view=active-stocks&date=YYYYMMDD&channel=sh-northbound|sz-northbound`
  读取指定交易日活跃股，并计算净买和成交占比；
- 每行保留 `raw`，所有来源输出尝试次数、陈旧状态、缓存年龄和上游错误；
- `func_hsgt205—208/211` 当前数据日期是 `20240816`，因此明确输出
  `snapshot_freshness=historical-snapshot`，不冒充今日榜单；
- 南向行业样本当前日期为 `20260318`；活跃股使用显式请求日期，不默认伪造
  “最新交易日”。

## 真实证据

- `output/probes/stock-connect-daily-increase.json`：日增仓榜，日期
  `20240816`，明确为历史快照；
- `output/probes/stock-connect-southbound-industry.json`：南向行业资金，日期
  `20260318`；
- `output/probes/stock-connect-northbound-industry-empty.json`：零字节北向行业表
  被规范化为正常空关系；
- `output/probes/stock-connect-active-sh-20260731.json`：沪股通指定日 10 只
  活跃股，证券名称可解析；
- `output/probes/tdx-jsn-variants-current.json`：14 个 `hsgt` 模板全部退出缺口；
- `output/probes/api-contracts-official-current.json`：正式服务 31/31 契约通过。

覆盖审计由 194/616 提升为 208/616，通用独占从 422 降为 408，已下载但
通用独占从 122 降为 115。当前最高收益缺口变为 `zcjc`：6/6 个模板已下载，
共 600 行。

## 验证与部署

- 新增增减仓、行业派生值、指定日活跃股的确定性单元测试；
- 新增“历史增仓快照”和“配置零字节正常空关系”两项正式 API 契约；
- 全量原生 CTest 48/48，临时及正式完整 API 契约 31/31；
- 正式服务 PID `27904`，命令行
  `dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`；
- 部署 EXE SHA-256：
  `52FB90434E5D97FFDAE48CF6456381EEBEB491CC7A4BA06E70A23A748415D871`；
- 功能目录仍为 94 项，`native_cpp=true`、`python_runtime=false`；本轮未修改
  Svelte UI。

## 下一批高收益方向

~~优先审计 `zcjc` 六张增减持表，并与现有 `market ownership` 对账，避免为
同一业务语义重复建命令。~~ 已完成，详见
[股东增减持六类聚合榜](2026-08-06-native-ownership-rankings.md)。下一组为
`gdrs`，其后依次为 `zdtfx`、`qszj`。
