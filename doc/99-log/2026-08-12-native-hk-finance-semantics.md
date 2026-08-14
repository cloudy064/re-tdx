# 本地港股财务字段语义与估值投影闭环

日期：2026-08-12

## 结果

`hkcwdata.dat` 的 16 个非代码源列均已获得可复核的字段角色。接口继续保留原来的
`raw_fields`、`native_values` 和 `tdx-market-hk-finance-native-v1` schema，同时新增
`finance` 类型化投影和每个原生字段的 `semantic_key/label_zh/unit` 元数据。

类型化对象覆盖：

- 报告日、上市日、分类码；
- 总股本、H 股本；
- 总资产、净资产、少数股权；
- 营业收入、净利润；
- 每股股息、每股收益、每股净资产；
- 市盈率（TTM）和市盈率（静）。
- 币种折算原生码及是否启用折算。

单位使用“报告币种”而不是臆定港币。金额内存值是万报告币种，股本是万股，每股值
是报告币种/股。币种折算码的角色有直接消费者证据，但数值到币种名称的枚举没有在
当前调用链中出现，因此只公开 `native_code` 和 `conversion_required`。

## 直接证据

`TdxW!sub_940640` 的分支条件明确包含市场 `71/31/48/49`，且同一分支还渲染港股
`CAS/VCM` 字段。其标签单元和数值单元成对：

- `security+132 / 10000`：总股本（亿）；
- `security+160 / 10000`：总资产（亿）；
- `security+192 / 10000`：净资产（亿）；
- `security+196 / 10000`：营业收入（亿）；
- `security+236 / 10000`：净利润（亿）；
- `security+156`：每股收益；
- `security+244`：每股净资产；
- `security+240`：每股股息；下一对单元以
  `100 * security+240 / 当前价` 计算股息率；
- 紧凑财务快照 `+56/+52`：市盈率（TTM）/市盈率（静），与 hkcw companion
  的同偏移写入以及 `selector 49 -> companion+52` 静态市盈率消费者一致。

这些内存偏移再与 `sub_51F310` 的 17 列加载顺序连接，消除了仅凭数值猜字段的风险。
通用财务描述表与消费者另外确认 `security+152` 为 H 股、`security+184` 为少数股权。

源列 15 写到 `security+352` 的单字节。`sub_950CB0/sub_9513A0/sub_951FA0` 在该字节
非零时，用两套行情数组的比率折算数值，直接证明其“币种折算码/标记”角色；消费者
只判断零/非零，没有暴露 13、24、55 等原生值的名称，因此枚举名仍保留边界。

主要机器证据：

- `output/ida-hkcwdata-companion-accessor-xrefs-20260812.json`；
- `output/ida-hkcwdata-finance-label-consumers-20260812.json`；
- `output/ida-hkcwdata-finance-selector-host-xrefs-20260812.json`；
- `output/ida-hkcwdata-special-renderers-20260812.json`；
- `output/ida-hkcwdata-currency-consumers-20260812.json`；
- `output/ida-hkcwdata-offset-labels-20260812.json`。

## 公式边界

后续已完成独立的 TCalc 调度闭环，而不是仅凭本表字段名套 selector：

- `TCalc!sub_10026B20` 的 `1/7` 分支分别读取宿主 type-103 的两个 float；
- TdxW 对扩展市场跳过 A 股股本历史，类别 2 的两个槽均回退到 `security+152`；
- 其余已闭合财务字段来自宿主 type `105`；
- `TdxW!sub_60E580 case 0x69` 对市场 `71/31/48/74` 明确生成同一 201 字节结构；
- TCalc 对 `31/53`、`38` 等 selector 另有 `71/31/48/74` 判断。

因此纯 C++ 解释器现从 `hkcwdata.dat` 自动绑定港股
`FINANCE(1/2/3/6/7/9/10/16/19/20/30/31/32/33/34/37/38/42/53)`。该映射严格按宿主槽
恢复，不能把同一 selector 的 A 股中文名机械套到港股；例如港股 `16` 的槽来自
已证明的少数股权，而港股类别 2 的 `1/7` 都是当前 H 股本，并不是伪造的历史序列。
19 个 selector 仅对当前目录已证明全为类别 2 的市场 `31/48` 开放；兼容市场 `71`
无当前类别样本，继续保留 17 个 type-105 selector。其他未闭合 selector 仍稳定拒绝。完整实现与验证见
[港股 FINANCE 解释器记录](2026-08-12-native-formula-hk-finance.md)。

## 验证

- 增量构建 `tdx-hk-finance-tests` 与 `tdx-tool` 通过；
- 专项 CTest 通过；
- 真实 `31:00700` 返回总股本 `912288.3125` 万股、营收 `55739500` 万报告币种、
  每股股息 `4.5`、TTM/静态市盈率 `21.7985/20.9248`，币种折算原生码为 `13`；
- 临时 18769 的 health、features、单票投影、非法代码拒绝 4 项契约分别返回
  `200/200/200/400`，临时服务自动退出，正式 8765 未操作；
- 完整 CTest 首轮 112/113 通过，未改动的 `tdx-formula-engine-tests` 在并行运行中出现
  一次组合板块临时目录断言，随后单独复跑通过；港股财务专项在完整回归中通过。

真实投影保存在 `output/hk-finance-00700-typed-20260812.json`。
