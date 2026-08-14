# 板块历史回测纯 C++ 固定化

## 客户端证据

`T0002/cloud_cfg/BK_BKLSHC.xml` 的页面标题和帮助链接表明该功能是“板块历史
回测”。页面只有三个业务参数：起始日期、截止日期、复权类型；复权值 `0/1/2`
分别是不复权、前复权、后复权。五个主视图使用同一 ReqId `200199`：

| category | CodeList | 客户端页签 |
|---|---|---|
| `all` | `12:0` | 全部 |
| `industry` | `12:1` | 行业板块 |
| `concept` | `12:2` | 概念板块 |
| `style` | `12:3` | 风格板块 |
| `region` | `12:4` | 地域板块 |

主表选择板块后，slave datasource 仍调用 `200199`，但 `CodeList` 变成
`2:<板块代码>|1`，返回该板块的证券行。请求固定使用 `mod_gpsj.dll`、
`HQServ.PBRPC_GPSJ` 和 `BK_BKLSHC.xml`，产品接口不允许任意上游 URL 或请求体。

## 业务接口

```powershell
tdx-tool market block-backtest --root C:\new_tdx `
  --category industry --begin 2026-07-01 --end 2026-08-05

tdx-tool market block-backtest --root C:\new_tdx `
  --block-code 880471 --begin 2026-07-01 --end 2026-08-05
```

HTTP 路径为 `/api/v1/market/block-backtest`，参数使用 `category`、`block_code`、
`begin`、`end`、`adjustment`、`sort` 和 `order`。日期接受 `YYYYMMDD` 或
`YYYY-MM-DD` 并执行真实日历校验。默认前复权、按区间收益降序；也可按净流入、
主力净流入、最大回撤、换手或代码排序。

原始列映射为：前收盘、最高、最低、收盘、区间收益、单日最大涨幅、振幅、
最大回撤、成交量、成交额、换手率、净流入和主力净流入。金额与百分号字段均
输出 JSON 数值；板块尽可能映射本地 `block_id/family`，证券统一为
`SZ/SH/BJ+六位代码`。

## 真实验证

`2026-07-01—2026-08-05` 前复权样本：

- 行业主表 56 条，全部映射本地行业节点；39 涨、17 跌，平均收益
  `0.5535%`、中位数 `3.4934%`；石油 `+17.1071%` 居首，半导体
  `-29.3977%` 居末；
- 银行 `880471` 返回 42 只证券；苏州银行 `+18.5345%` 居首，平安银行
  `+11.9403%`、排名第 10；
- 两次请求均为两轮 RpcID 传输，原始探针和规范化探针分别保存在
  `output/probes/pbrpc-200199-*.json` 与 `output/probes/block-backtest-*.json`。

部署后的固定 API 首次行业请求约 720.6 ms，5 分钟参数缓存内相同请求约
35.6 ms，并明确返回 `cache.hit=true`；非法日期 `2026-02-31` 返回 HTTP 400，
不会发送到上游。服务自检通过，功能注册数增至 77，首页继续返回 HTTP 200。

必须注意：详情成员是本次上游服务为板块返回的集合。当前没有成员加入/退出
日期证据，因此输出固定声明 `membership_basis=upstream-server-selection`、
`historical_membership_reconstructed=false`，避免幸存者偏差被误解成严格历史
成分回测。
