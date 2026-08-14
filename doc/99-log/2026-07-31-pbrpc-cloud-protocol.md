# PBRPC 云功能协议与真实请求

> 日期：2026-07-31  
> 范围：`TdxTopicView100.dll`、`TdxZdView100.dll`、`TPData100.dll`、
> `T0002/cloud_cfg` 与 `static.tdx.com.cn:7615/TQLEX`

## 目标

解释 `reqformat=22` 的真实发送格式，并在不启动、不注入 TdxW 的条件下
复现竞价策略、资金模型、选股模型、九转、压力支撑和龙虎榜。

## 静态证据

客户端调用边界为：

```text
TdxW.exe
  └─ TdxTopicView100.dll          云页面工厂、加载 cloud_cfg XML
       ├─ TdxZdView100.dll        datasource 描述符与 protobuf 编解码
       └─ TPData100.dll           FetchDataHandle、会话与收发
```

`TdxZdView100.dll` 的关键函数：

- `sub_100025B0`：解析 `pb_rpc_req:` 描述符，处理 `neststruct`、
  `PbOperType` 和 `$REQID$`，构造并序列化动态 protobuf；
- `sub_102A3770`：识别 `reqformat=22`，创建类型 `1144` 的取数句柄；
- `sub_102ABDE0`：调用序列化函数并通过 TPData 发送；
- `sub_10376A50`：初始化 protobuf 和 `proto\` 路径；
- `sub_1000C430`：使用口令 `tdxokokok` 解开 `proto\zdproto.dat`。

归档中恢复出 12 个原始 schema：

```text
ClientTaskCmd.proto
protocol_mp.proto
PyTaskArgs.proto
QuantDB.proto
QuantDBTable.proto
QuantDBTableCCCZ.proto
QuantDBTableCL.proto
QuantDBTableHC.proto
QuantOutPut.proto
TciTaskArgs.proto
TdxTaskArgs.proto
Utility.proto
```

`protocol_mp.proto` 给出的外层消息为：

```text
pb_rpc_req:
  1 Head, 2 RpcID, 3 StartPos, 4 Moduledll, 5 ReqByte

pb_rpc_ans:
  1 Head, 2 RpcID, 3 StartPos, 4 TotalLen, 5 RetByteNum, 6 RetByte
```

`ReqByte` 是 XML 中的 JSON 字符串。`QuantDB.proto` 还包含
`ReqSelect/AckSelect`、插入、更新、删除和任务进度消息；它们属于同一
动态 protobuf 注册系统，但不是本批云页面请求的业务返回格式。

## 网络流程

请求地址：

```text
POST http://static.tdx.com.cn:7615/TQLEX?Entry=<datasource name>
Content-Type: application/octet-stream
```

1. 首包发送 `RpcID=0, StartPos=0`；
2. 首次响应分配正数 `RpcID`；
3. 后续包重复原请求，并带回 `RpcID` 和累计 `StartPos`；
4. 拼接 `RetByte`，直至达到 `TotalLen`；
5. 部分模块在 JSON 后附一个 NUL，需要在 JSON 解码前去掉。

以 `ReqId=200404` 为例，第一次请求正文的已知测试向量以
`0a030a0131220b6d6f645f7065672e646c6c...` 开始；真实服务通常在第二轮
返回完整业务 JSON。这也解释了直接 POST `pb_rpc_req:...` 描述符为何
返回 HTTP 503。

## 真实结果

以下是同一交易日的一次快照，行数不是固定接口契约：

| ReqId | 功能 | 行数 | 主要字段 |
| --- | --- | ---: | --- |
| `200400` | 烂板转强 | 4 | 昨日开板次数、涨停原因、几天几板 |
| `200401` | 炸板转强 | 7 | 炸板幅度、曾涨停原因 |
| `200402` | 竞价止跌 | 592 | 昨日涨幅、竞价涨幅/成交额 |
| `200403` | 预吞上影 | 77 | 上影长度、竞价涨幅/成交额 |
| `200404` | 竞价爆量 | 10 | 竞价涨幅/成交额、昨日成交额 |
| `200405` | 涨停高开 | 33 | 竞价涨幅、竞价/昨日量比、强势类型 |
| `200406` | 5 分钟陡增 | 208 | 9:20 匹配额、竞价成交额 |
| `200225` | 事件驱动 | 322 | 涨幅、开盘涨幅、事件换手、入选时间 |
| `200250` | 创新高 | 2 | 涨幅、换手、成交额、入选时间 |
| `200316` | 强势启动 | 14 | 入选日、入选后涨幅、最大回撤、安全分 |
| `200320` | 主板上涨通道 | 90 | 压力位、支撑位、趋势起点、安全分 |
| `200325` | 主板下跌通道 | 1,446 | 压力位、支撑位、趋势起点、安全分 |
| `200340` | 市场分时段主力资金 | 39 | 各时段净额、成交额、主力占比 |
| `200341` | 行业成分股分时段资金 | 32 | 各时段净额、成交额、主力占比 |
| `200451` | 上涨/下跌九转 | 143 | 九转类型、日期和价格选择字段 |
| `500107` | 全市场龙虎榜 | 63 | 异常类型、涨跌/振幅/换手、主力净额 |
| `200001` | 指数相对基准的估值比走势 | 484 | 日期、估值比 |
| `200011` | 融资融券历史模型 | 2,113 | 两融余额/比率及未来指数表现 |
| `200302` | 行业 PE(TTM) | 30 | 行业代码、名称和 PE |
| `200300` | 个股 PE 历史 | 728 | 股价、PE、25/50/75 分位及极值 |
| `200301` | 行业内 PE 估值 | 32 | 预测收益/股价/PE 与估值判断 |
| `200305` | 行业 PB-ROE | 30 | 行业 PB、ROE 和 ROE 中位数 |
| `200303` | 行业内 PB-ROE | 32 | 个股 PB/ROE、预测 PB 和估值判断 |
| `200452` | 个股 RPS | 62 | 三周期 RPS 及区间涨幅 |
| `200453` | 板块 RPS | 10 | 三周期 RPS 及区间涨幅 |
| `200199` | 板块成分区间回测 | 32 | 涨跌、回撤、换手、成交和资金流 |
| `200327` | 创新高 / 创新低 | 37 / 131 | 突破日、周期、回撤和安全分 |
| `200329` | 横盘突破 | 48 | 突破日、涨幅、成交额、行业和安全分 |
| `500503` | 北向资金历史统计 | 2,227 | 净流入/净买入和沪深 300 后续涨幅 |

`200341` 对宽基代码 `880008/999999` 返回 protobuf
`int32 RpcID=-1`，但对行业代码 `881001` 成功返回煤炭行业 32 只成分
股。这证明它是行业从属查询；工具对负数 int32 的解析正确，失败原因是
业务参数范围，不是协议编码。

估值和 RPS 参数不是试错猜测，而是从 XML 子控件的 `defsel`、
`datevalue` 和 `defval` 恢复：

- PEG：PE(TTM)、3% 要求年回报率、近 36 个月；
- 个股 RPS：10 日/90、20 日/90、60 日/90；
- 板块 RPS：10 日/85、20 日/85、60 日/85；
- 板块回测：前复权，客户端默认近 5 年。

`200199` 的全类别模板 `CodeList=12:0` 返回 `RpcID=-1`，但从属模板
`CodeList=2:881001|1` 成功返回煤炭板块 32 只成分股。这说明同一 ReqId
下必须保留模板选择语义，不能总取占位符最少的第一项。

另行确认 `jj_ccfx_dqcc.xml` 中旧 PBRPC `500007` 已被注释；当前基金
集中度页面使用普通 JSON `500050—500052`。基金 `000326` 在
`20250630` 返回 11 个行业持仓和 106 个股票持仓；季度末空结果与报告期
数据可用性有关，不是 PBRPC 问题。

排除 XML 注释后共有 29 个唯一 PBRPC ReqId。随着 `200001`、
`200011`、`200327`、`200329` 和行业参数下的 `200341` 成功返回，
当前启用 ReqId 的真实请求覆盖率达到 29/29。

## 实现

新增 `doc/90-scripts/tdx_pbrpc.py`：

- 只使用 Python 标准库；
- 从客户端 XML 自动发现 Entry、模块、请求体和动态占位符；
- 可用 `--body-contains` 在重复 ReqId 中选择特定请求模板；
- 手工编码/解析所需 protobuf wire types，不依赖生成代码；
- 自动完成 RpcID 握手、分页、长度校验和 JSON 解码；
- 支持列出配置、替换模板变量、覆盖请求参数和保存结果。

相应单元测试覆盖已知请求字节、RpcID 响应、负数 int32、两阶段拼包、
XML 描述符和尾随 NUL JSON。

## 后续

PBRPC 编码层已不是阻塞点。收益较高的后续工作是：

1. `reqformat=2` 的配置发现、分页和导出已由 `tdx_tqlex.py` 完成；
2. 下一步在需要动态从属参数的功能中自动执行主表请求，再执行明细请求；
3. 为 34 个启用 JSON ReqId 建立真实成功覆盖矩阵。
