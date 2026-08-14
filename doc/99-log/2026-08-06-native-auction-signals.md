# 七类竞价策略纯 C++ 固定化

## 目标

把 `sc_jjcl.xml` 中七个无需 L2 的竞价策略从通用 PBRPC 调试模板迁入已有
`market technical-signals`，使其可直接查询、按单票反查并保存集合快照。

## 请求与当前样本

2026-08-06 真实请求全部为 `ErrorCode=0`：

| ReqId | 固定视图 | 客户端标题 | 上游行 | 客户端过滤后 |
| --- | --- | --- | ---: | ---: |
| `200400` | `weak-limit-reversal` | 烂板转强 | 15 | 15 |
| `200401` | `failed-limit-reversal` | 炸板转强 | 18 | 18 |
| `200402` | `auction-bottom-reversal` | 竞价止跌 | 0 | 0 |
| `200403` | `upper-shadow-engulf` | 预吞上影 | 10 | 10 |
| `200404` | `auction-volume-spike` | 竞价爆量 | 14 | 14 |
| `200405` | `limit-up-gap` | 涨停高开 | 11 | 8 |
| `200406` | `five-minute-volume-surge` | 5分钟陡增 | 79 | 79 |

零行的竞价止跌是成功空集合，不是请求错误。原始响应位于
`output/probes/pbrpc-200400-current.json` 至 `pbrpc-200406-current.json`，
规范化响应位于 `output/probes/technical-signals-<view>.json`。

## XML 语义复刻

- `200400..200404` 的竞价占昨日成交额由
  `aggregateAuctionAmount*100/yesterdayAmount` 计算；`200406` 使用
  `bidAmount*100/yesterdayAmount`。服务端已给出 `200405` 的
  `bidYesAmountRatio` 时直接保留。
- `200404` 返回的 `yesterdayGrowth` 是放大 100 倍的原值。客户端实际显示列
  `yesterdayGrowth1` 明确使用 `yesterdayGrowth/100`；规范化层复现该公式。
- `200405` 枚举将代码 0/1 映射为“跌停冲高/涨停高开”，数据源明确筛选
  “涨停高开”，因此默认只保留代码 1。`--raw` 仍可审计全部 11 行。
- 其他选择算法没有在 XML 披露，响应明确返回
  `client_rule_disclosed=false`，不依据标题猜测隐藏阈值。

## 标准化字段

七类结果统一为证券身份、昨日涨幅/成交额、竞价涨幅/成交额、竞价额占比和
信号阶段；再按策略保留开板次数/题材原因、炸板幅度、上影长度、竞价异动
代码或 9:20 匹配额。金额保持元，涨幅和比率保持百分点。

单票反查现由十七扩至二十四个视图。`SZ000001` 最新探针约 13.990 秒完成
24/24 实时请求、失败 0、旧缓存 0，仍命中一条上涨九转；证据保存在
`output/probes/technical-signals-security-SZ000001-24views.json`。

## 验证

- 单元测试新增 `/100` 公式、竞价额占比、`strongStyle=1` 筛选和 9:20 金额；
- 全量 CTest 保持 `29/29` 通过；
- 上游瞬时 `RpcID=-1` 现与 HTTP 429/502/503/504、业务错误 4 一样进入有限
  重试，并在已有同参数缓存时允许显式 `stale-cache` 降级。

部署后服务 PID 为 `22316`，可执行文件 SHA-256 为
`C7E8E17794C2A6D0AA64E3F4A9948D32ECCEBDECECAAF751E17FACE5B2BF7FD0`，主页
HTTP 200、功能目录 78。涨停高开冷请求约 606.9 ms，默认 8 行且明确过滤
3 行，`raw=1` 返回完整 11 行；竞价爆量首行昨日涨幅正确为 1.18%。平安银行
24 视图常驻 API 冷聚合约 13.735 秒、24/24 成功，相同参数缓存约 28.3 ms。
