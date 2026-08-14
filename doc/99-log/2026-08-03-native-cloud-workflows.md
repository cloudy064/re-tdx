# TQLEX/PBRPC 主从工作流纯 C++ 迁移

> 日期：2026-08-03

## 目标

在两类单请求协议已经原生化后，把客户端 `masterid` 表达的主表—明细关系
迁入统一工具。调用者只选择业务工作流和主键，不再手工复制基金代码、指数
代码或龙虎榜的 `Code/SetCode`。

## 已实现工作流

| 工作流 | 主表 | 明细 | 协议组合 |
| --- | --- | --- | --- |
| `fund-holdings` | `500050` | `500051/500052` | JSON→JSON |
| `fund-risk` | `500030` | `500031` | JSON→JSON |
| `fund-volatility` | `500032` | `500033` | JSON→JSON |
| `fund-interval-holdings` | `500055` | `500056/500057` | JSON→JSON |
| `index-valuation` | `200000` | `200001` | JSON→PBRPC |
| `lhb-details` | `500107` | `500108` | PBRPC→JSON |

实现统一解析 `ResultSets/ColDes/Content`，兼容数组行、对象行和 PBRPC 的
空格分隔字符串行；字段查找大小写不敏感。指定 `--select` 后自动完整分页
主表，确认目标键确实存在，再按声明映射子请求占位符。默认只展开一行，
防止一次生成大量网络请求。

龙虎榜链显式固定 `MarketType=0` 的主/从模板，并将工作流参数 `result`
映射为明细模板的 `#2111.result`，避免同 ReqId 六份模板被错误混用。

## 真实验证

2026-08-03 六条链全部成功：

| 工作流 | 主表 | 首个/指定明细 |
| --- | ---: | --- |
| 基金持仓 | 20（指定时完整 1,187） | `000326 + 20250630`：11 行业、106 股票 |
| 基金收益风险 | 20 | 48 个交易日 |
| 基金月度波动 | 20 | 36 个月 |
| 基金区间持仓 | 20 | 6 个行业、9 个报告期 |
| 指数估值 | 180 | 482 日估值比 |
| 龙虎榜原因 | 55 | 1 条原因说明 |

基金 `20251231` 主表当前能返回，但多数基金的 `holdPosNum/holdIndustryNum`
为零，明细也为空；这属于服务器当前数据可用性。网页保留动态披露日期默认值，
同时提供已验证的 `000326 + 20250630` 快捷示例，不把空明细视为协议错误。

## 产品入口

```powershell
tdx-tool cloud workflow --root C:\new_tdx --list
tdx-tool cloud workflow --root C:\new_tdx --workflow index-valuation
tdx-tool cloud workflow --root C:\new_tdx --workflow fund-holdings `
  --select 000326 --page-size 100 --set report_date=20250630
```

固定 API 为 `/api/v1/cloud/workflows` 与 `/api/v1/cloud/workflow`；Svelte
新增“关联工作流”页面，展示协议链、默认参数、主表选中行和所有明细结果集。
