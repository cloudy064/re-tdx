# 债券分类主表、沪深投影与尺度边界

## 结果

前一轮确认可下载但未类型化的其余 19 个债券资源，已全部通过纯 C++ 7709
下载器完成 MD5 校验和原子落盘。加上“全部债券”主表，本地债券新增资源共
20 个；完整 JSN 镜像现有 600 个文件、492,548 行、225,019,299 字节。

`market bond-reference` 新增 `--include-projections`，对应 API 参数
`include_projections=1`。该开关显式加载客户端跨市场主表、沪深分市场投影，按
`市场 + 证券代码` 做集合对账；默认关闭，避免普通网页查询无条件解析数十 MB
投影。Svelte 债券资料库新增“核对市场投影”按钮。

## 七类真实对账

| 类别 | 选择主表行 | 唯一证券 | 投影并集 | 结果 |
| --- | ---: | ---: | ---: | --- |
| 全部债券 | 42,479 | 41,135 | 41,125 | 主表多 10 只政策性金融债 |
| 国债 | 450 | 450 | 450 | 精确一致 |
| 地方政府债 | 30,010 | 28,440 | 28,440 | 精确一致；主表保留重复内部债券 ID |
| 公司债 | 7,289 | 7,289 | 7,289 | 沪 5,755 + 深 1,534，精确一致 |
| 企业债 | 1,765 | 1,765 | 1,765 | 沪 1,645 + 深 120，精确一致 |
| 私募债 | 11,268 | 11,263 | 11,216 | 完整表独有 77，投影独有 30 |
| 资产支持证券 | 6,738 | 6,728 | 1,588 | 当前只发布深市投影，完整表多 5,140 |

私募债客户端所谓“合并表”只有 1,583 只，集合上精确等于深市投影；资产支持
证券合并表同样只有 1,588 只，精确等于其深市投影。因此两类继续使用更完整的
既有主表，客户端合并表作为 `client_master_comparison` 单独返回，不允许覆盖。

国债、地方政府债、公司债、企业债切换到客户端真正的跨市场主表。主表的实际
行情证券位于 `$SC1/$ZQDM1`，`$ZQDM` 是客户端内部债券 ID；两者已分别输出，
不再把行情证券误判为可转债正股。

## `GM` 尺度修正

分市场 CFG 明确把 `GM` 声明为“债券余额（亿元）”，因此投影行只在该语义下
输出 `outstanding_balance_source_100m/outstanding_balance_yuan`。跨市场合并
CFG 只写隐藏“规模”，真实值同时出现 `6.3` 与 `56000000000` 等不同尺度，
不能统一乘一亿；这些行现在仅输出 `source_scale_raw` 和
`client-master-hidden-unit`，`issue_size_yuan` 保持空值。已有明确来源单位的
发行规模仍按原规则换算。

## 覆盖与验证

- 类型化 JSN：568/622 提升到 587/622；
- 本地文件：581 提升到 600；600 个已下载文件的通用缺口仍为 0；
- 剩余 35 个通用模板全部是公开上游不存在的资源；
- C++ CTest：101/101；
- Svelte 检查：0 错误、0 警告；生产构建成功；
- 临时 8766 新增/相关债券契约：5/5；
- 临时 8766 全量 API 契约：179/179。
- 正式 8765 新增/相关债券契约：5/5；
- 正式 8765 全量 API 契约：179/179。

新增固定契约 `bond-reference-corporate-projections-live` 固定公司债精确集合，
`bond-reference-private-boundary-live` 固定私募债 `11,263 vs 11,216`、差集
`77/30` 及客户端合并表等同 1,583 只深市投影的事实。

正式服务在巡检后重启以释放测试缓存，当前 PID 为 `26596`，仅监听
`127.0.0.1:8765`，8766 已关闭。发布 EXE SHA-256 为
`4CCED577968D18ADCCF11FE3918CB12323BFF61E9273991E1C6866828ACD6875`。

证据文件：

- `output/probes/jsn-bond-projections-download-20260809.json`
- `output/probes/jsn-catalog-bond-projections-20260809.json`
- `output/probes/bond-projection-all-20260809.json`
- `output/probes/bond-projection-government-20260809.json`
- `output/probes/bond-projection-local_government-20260809.json`
- `output/probes/bond-projection-corporate-20260809.json`
- `output/probes/bond-projection-enterprise-20260809.json`
- `output/probes/bond-projection-private-20260809.json`
- `output/probes/bond-projection-asset_backed-20260809.json`
- `output/probes/jsn-variant-gaps-after-bond-projections-20260809.json`
- `output/probes/api-contract-bond-projections-temp-20260809.json`
- `output/probes/api-contract-full-temp-20260809-bond-projections.json`
- `output/probes/api-contract-bond-projections-formal-20260809.json`
- `output/probes/api-contract-full-formal-20260809-bond-projections.json`
