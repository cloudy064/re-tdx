# 异常证券刷新摘要与上榜原因纯 C++ 固定化

## 主从关系

`hq_lhb_dryc.xml` 的当前异常证券页面由三条请求组成：PBRPC `500107` 返回
十九类证券列表，普通 JSON `500109` 返回顶部刷新摘要，选择证券后普通 JSON
`500108` 返回上榜原因。原工具已把 `500107` 固定为 `market abnormal-moves`，
但后两条仍只存在于通用 `cloud workflow lhb-details`。

新增纯 C++ `market abnormal-details` 和固定只读
`/api/v1/market/abnormal-details`：

- `summary`：按全市场、沪深主板、创业板、科创板或北交所，返回刷新时间、
  服务端报告条数和可空预测准确率；
- `explanation`：按市场/六位代码返回原因、累计偏离、累计成交股数、累计成交
  金额和异常期间；支持十九类异常码和指定数据日。

它补充的是当前云端异常列表。`market lhb` 仍负责本地 JSN 历史事件和营业部
买卖席位，`market anomaly-risk` 负责 `2044/200720` 相对指数与停牌风险，三者
没有合并为伪造的统一口径。

## 字段与空值

当前 `500109` 文本形如“刷新时间 14:23 预测准确率 -- 共81条”。接口将时间
规范为 `14:23:00`，条数转为数值，`--` 准确率转为 null；没有把空准确率改成
0。`500108` 的中钨高新样本结构化得到约 20.7% 累计偏离、3.18 亿股累计成交、
约 182.6 亿元累计成交和 2026-08-04—2026-08-06 异常期间。上游万元字段同时
保留 `_10k_cny` 并提供乘 10,000 后的 `_yuan`。

两条上游结果都声明 `ColNum=2`，但只有一个列描述和一个内容单元；来源元数据
保留 `consistent=false` 和 `row_count_trusted=decoded-content`。未上榜证券不
一定返回零行，而可能返回一行空 `Detail`；接口同样规范为
`availability=empty`、`data=null`，避免把正常空关系报成 HTTP 400。

## 验收

- 专项测试覆盖空准确率、数值准确率、原因、偏离、成交量、万元/元单位、日期
  区间、零行及一行空占位；
- 全量 CTest 39/39 通过；
- 真实证据为 `output/probes/tqlex-500109-current.json`、
  `tqlex-500108-sz000657-current.json`、`abnormal-details-summary-current.json`
  和 `abnormal-details-sz000657-current.json`。

## 正式部署

正式 `8765` 服务已切换到本版本，PID 为 `27416`，EXE SHA-256 为
`E88B107B66AB1A157C98FD52E55D4FD4B6E8A91789F894EDEBBABD59AD3AAEFE`。
健康检查为 `ok=true`、`native_cpp=true`、`python_runtime=false`，功能目录为
88 项并包含 `market abnormal-details`。

正式 API 复验时，`summary` 返回刷新时间 `14:35:00`、已报告 88 条、准确率
为 null；中钨高新返回 21.75% 累计偏离、322,605,840 股、
18,466,500,900 元和 2026-08-04—2026-08-06 区间。平安银行按正常空关系返回
`availability=empty`、`data=null`。旧的停牌风险、资金流后续模型接口均通过
在线回归，主页 HTTP 200；Svelte HTML/JS/CSS 哈希仍为 `0F1FBB...5777`、
`B03C28...4011`、`733085...30A`，没有覆盖用户 UI。

该部署随后已被同日的基金报告期持仓版本替代；当前正式进程与哈希见
`2026-08-06-native-fund-reported-holdings.md`，本节只保留当时验收记录。
