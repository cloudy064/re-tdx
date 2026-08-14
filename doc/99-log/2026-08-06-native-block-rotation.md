# 行业、概念、地区、风格板块轮动纯 C++ 固定化

## 页面与同名功能边界

`T0002/cloud_dax/BKLD.sp` 的页面名为“板块轮动”，四个并列主单元明确对应：

| 单元 | 客户端分类 | CFG | JSN |
| --- | --- | --- | --- |
| 1 | 行业板块 | `func_bkld101.cfg` | `list/func_bkld101_1.jsn` |
| 2 | 概念板块 | `func_bkld102.cfg` | `list/func_bkld102_1.jsn` |
| 3 | 地区板块 | `func_bkld103.cfg` | `list/func_bkld103_1.jsn` |
| 4 | 风格板块 | `func_bkld104.cfg` | `list/func_bkld104_1.jsn` |

`BKLD2.sp` 实际是每日热点预测、网页和关联个股组合，不使用这四张表；
`BK_BKJJ_LSBKLD.sp` 则通过 `reqformat=1/cfg_bk_bkld_yjhy` 展示历史板块强弱。
两者没有被按相似文件名拼入本接口。

## 字段语义

四份 CFG 字段完全一致：`date` 为上次异动日，`ts` 为上次至今天数，`pl` 为
平均异动周期；`yd/zyd/dyd` 分别是总异动、上涨异动、下跌异动，覆盖近一周、
一月、三月和一年；`zf` 是对应区间涨幅。客户端展示列 `zf=pl-ts` 不在 JSN，
纯 C++ 以 `cycle_gap_days` 精确复现。当前涨幅等 `$ZAF` 宿主行情列未被伪造。

每条结果同时保留四个周期和 `raw`，查询时可指定一个周期用于排序、方向过滤
和汇总。`up-dominant/down-dominant/balanced/inactive` 只由所选周期的原始上涨、
下跌异动数确定，不对异动原因作额外猜测。

## 板块名称和成员边界

行业 56、概念 269、风格 158 个代码都能与本地板块目录一一关联，因而返回
`block_id`、本地族、成员数和 `/api/v1/blocks?q=<代码>`。地区 32 个代码不在
本地 `tdxzs3.cfg/infoharbor_block.dat` 成员目录，但能从 `shm.tnf` 指数证券目录
解析名称；它们明确返回 `members_available=false`，没有伪造地区成分。

风格表 `880836` 的当前原始行缺少 `$SC`，本地板块身份仍可解析，但
`upstream_market/security_id` 保持空值，未按其他行擅自填为沪市。

## 命令与 API

```powershell
tdx-tool market block-rotation --category all --period 1w --sort anomalies
tdx-tool market block-rotation --category industry --period 1m --sort return
tdx-tool market block-rotation --category industry --code 880305
```

固定接口为 `/api/v1/market/block-rotation`，支持：

- `category=all|industry|concept|region|style`；
- `period=1w|1m|3m|1y`；
- 最近异动日、距今天数、平均周期、周期差、总/涨/跌异动、方向差和涨幅九种排序；
- 五类方向过滤、代码/文本、分页、刷新、缓存和超时；
- 四资源来源尝试次数、陈旧状态、聚合健康以及按分类汇总。

## 真实结果

2026-08-06 在线四表为 56/269/32/158 行，共 515 行，板块名称解析 515/515，
最新上次异动日为 2026-08-05。近一周四分类各记录 50 次异动，合计 200 次，
其中上涨/下跌各 100 次。单板块 `880305` 正确解析为电力、85 个本地成员。

真实探针：

- `output/probes/block-rotation-all-current.json`；
- `output/probes/block-rotation-industry.json`；
- `output/probes/block-rotation-concept.json`；
- `output/probes/block-rotation-region.json`；
- `output/probes/block-rotation-style.json`；
- `output/probes/block-rotation-880305.json`；
- `output/probes/block-rotation-contracts-temp.json`。

## 覆盖、测试与部署

- 四张 `bkld` 模板全部绑定 `market block-rotation`，类型化覆盖由 233/616
  提升到 237/616，通用独占由 383 降到 379，已下载通用独占由 94 降到 90；
- `tdx-block-rotation-tests` 覆盖四分支路由、本地板块和 TNF 名称解析、成员边界、
  缺失市场、客户端派生公式、周期空值和排序；全量 CTest 52/52；
- 正式契约增加到 42 项并 42/42 通过；
- 正式服务 PID `37756`，端口 `8765`，功能目录 97 项，EXE SHA-256 为
  `ACE37A3452CD1A2FA35B5D5E556D8E32584811832227FD04BDEEA8CF8CA15280`；
- 运行时保持 `native_cpp=true`、`python_runtime=false`，临时 `8879` 已关闭。

`lbtt101` 已在后续阶段固定为独立连板天梯接口，见
[研究行业/概念连板天梯](2026-08-06-native-limit-ladder.md)。下一批非 L2 高收益
缺口依次是 `bygtj102`、`ldph101`、`tbgz108/tzcg104` 和 `ygzl101`；五张
`qszj` 是 DDX 大单资金衍生榜，按当前约束暂缓。
