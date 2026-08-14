# 财报公告批量回补、限速重试与断点续传

日期：2026-08-06

## 结论

纯 C++ `market disclosures` 已可把单票静态 F10 公告回补扩展为受控批处理，
不依赖 Python，也不默认扫描全市场。任务可从显式证券、自选股、JSON/CSV/文本或
通达信板块生成证券集合，并持续幂等合并同一个披露归档。

常用入口：

```powershell
# 先解析自选股；不联网、不写归档和状态
tdx-tool market disclosures --root C:\new_tdx `
  --backfill-announcements --backfill-watchlist --dry-run

# 回补默认自选股 T0002\blocknew\zxg.blk
tdx-tool market disclosures --root C:\new_tdx `
  --backfill-announcements --backfill-watchlist --archive

# 精确板块 id/code/name；父板块自动包含所有子板块成员
tdx-tool market disclosures --root C:\new_tdx `
  --backfill-announcements --backfill-block research-industry:X50 `
  --archive --max-securities 500

# 多种证券清单可以合并，最终按市场和代码去重
tdx-tool market disclosures --root C:\new_tdx `
  --backfill-announcements --security sz300503 `
  --securities sz000001,sh600000 --backfill-input securities.csv `
  --archive
```

`--backfill-input` 接受 JSON、CSV、普通文本以及通达信 `.blk`。JSON 可使用
`market + code`、`security_id`，并递归读取 `items/rows/members/matches` 等常见
容器；CSV 支持 `market,code,name` 或 `security_id`，也可直接读取本工具导出的
板块成员 CSV。

## 任务语义

默认状态文件是：

```text
C:\new_tdx\T0002\tdx-tool\disclosure-announcement-backfill-state.json
```

状态按证券保存 `pending/running/completed/failed`、尝试次数、错误、报告数和时间。
每次请求前先把证券标成 `running`；成功后先原子替换披露归档，再原子替换任务
状态。若进程恰好在两次替换之间退出，重启会重做该证券，但归档合并仍然幂等，
不会重复制造披露事件。已完成证券默认直接跳过；需要重新取数时显式使用
`--refresh-completed`。

状态文件绑定归档的规范化绝对路径，误把旧状态用于另一个归档时会拒绝执行。
单个证券失败不会丢弃此前成功进度，最终仍写入完整汇总，并以退出码 `3` 表示
存在失败项。

流量控制与安全阀：

- `--delay-ms`：不同证券之间的间隔，默认 250 毫秒；
- `--retries`：每只证券失败后的重试次数，默认 2；
- `--retry-delay-ms`：线性退避基数，默认 1000 毫秒；
- `--max-securities`：解析后的数量上限，默认 500，超限直接拒绝而非截断；
- `--dry-run`：只输出最终证券集合，网络请求和持久写入均为 0。

## 验收

离线故障注入覆盖了“第二只证券第一次失败、重试成功”的路径：2 只证券产生
3 次请求，成功后归档含 2 个条目；原归档和状态再次传入时，2/2 被跳过且抓取器
调用次数为 0。请求前状态检查点、成功归档检查点及重复运行的幂等性均有原生测试。

真实数据验证结果：

- 父级研究行业 `research-industry:X50`（银行）递归解析出 42 只证券；
- 当前 `zxg.blk` 去除无效代码和重复项后解析出 37 个条目；
- 首次自选股批处理成功 37、失败 0、网络请求 37；
- 同命令再次执行跳过 37、网络请求 0；
- 披露归档从 5,563 个“证券×报告期”条目增至 5,697，正式报告可用条目从
  142 增至 276；
- 工具自身生成的预览 JSON 可再次作为输入并还原 37 个条目；
- 111,082 行板块成员 CSV 去重解析为 5,972 只深沪京证券，全程网络请求 0；
- 原生测试 `26/26` 全部通过。

验收产物：

- `output/probes/disclosure-backfill-bank-preview.json`
- `output/probes/disclosure-backfill-watchlist-run.json`
- `output/probes/disclosure-backfill-watchlist-rerun.json`
- `output/probes/disclosure-backfill-json-input-preview.json`
- `output/probes/disclosure-backfill-csv-input-preview.json`

## 边界

批处理改善的是覆盖率、可恢复性和流量控制，不改变上游边界：每只证券仍只查询
静态 F10 最近一年、最多 300 条公告。显式证券清单会保留 ETF、指数等合法代码，
它们可能成功返回零个正式财报；工具不会擅自把用户输入过滤为股票。

后续已增加上市证券类别感知的覆盖率审计、未来预约保护和可直接回灌的可行动
队列，并据此修复沪市“2025年度报告”年份边界及少数 `LSGG` 正文分类；详见
[覆盖率审计记录](2026-08-06-disclosure-coverage-audit.md)。

批处理里程碑版本曾部署到 `dist/tdx-tool/bin/tdx-tool.exe`，SHA-256 为
`19A8980DF1636A1D4049461C4720C76ED68FBC87A7CF2F491356B2B74A87BA11`，服务进程
为 `17168`。`/api/v1/health` 返回 `ok=true`、`native_cpp=true`、
`python_runtime=false`。
