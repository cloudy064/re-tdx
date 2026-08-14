# TCalc 港股 FINANCE type-103/type-105 上下文闭环

日期：2026-08-12

## 结论

原先“扩展市场一律不适用 FINANCE”的判断过宽。当前 DLL 的真实路径为：

1. `TCalc!sub_10026B20` 的 selector `1/7` 分支都调用 `sub_100108D0`；selector 1
   读取 type-103 每条记录的第一个 float，selector 7 读取第二个 float；
2. `TdxW` 的宿主回调 `case 103` 对证券类别 2 把 `security+152` 同时作为两个
   回退值；`sub_519DB0` 对所有扩展市场跳过 A 股 29 字节股本历史并广播回退值；
3. 其余闭合 selector 由宿主 type `105` 提供；`TdxW!sub_61B630 case 105` 转入
   `sub_60E580`；
4. `TdxW!sub_60E580 case 0x69` 构造 201 字节财务结构，并对市场
   `71/31/48/74` 设有明确分支；
5. TCalc 对 `31/53`、`38` 等 selector 另有 `71/31/48/74` 判断。

这证明港股并非借用 A 股 HTTP 财务接口，而是由证券主记录和
`hkcwdata.dat` 装配同一宿主结构。

## 已实现 selector

新增独立策略模块 `formula_context_hk_finance.cpp`，当前自动绑定：

| selector | 港股宿主来源/算法 |
| --- | --- |
| 1/7 | type-103 第一/第二槽；类别 2 均回退为 `security+152 * 10000`，即 H 股本 |
| 2/3 | 市场号、TCalc 证券类型 |
| 6 | `security+152 * 10000`，H 股本 |
| 9 | `(总资产-净资产-少数股权)/总资产*100` |
| 10/16/19/20/30 | 总资产、少数股权、净资产、营收、净利润，万单位还原为原生单位 |
| 31/32 | 港股原生分支固定 0 |
| 33/38 | `security+156`，每股收益 |
| 34 | `security+244`，每股净资产 |
| 37 | `净利润/总股本/EPS*4`，保留 TCalc 阈值 |
| 42 | 当前本地日期减 `security+128` 上市日 |
| 53 | `security+240`，每股股息 |

`FINANCE(16)` 在港股槽中来自已验证的少数股权，不能套用 A 股同 selector 的字段名。
`FINANCE(1/7)` 也不能机械套用 A 股“总股本/历史流通股本”：扩展市场原生路径不读
A 股历史文件，类别 2 的两个返回槽都使用当前 H 股本。当前 7727 全目录实测市场 31
的 2582 条记录和市场 48 的 339 条记录均为类别 2；市场 71 仍存在于 DLL 静态分支，
但当前目录已无样本，因此只记录为兼容市场号，不声称有当前目录验证。

同时补齐了 A 股 `FINANCE(2/31/32/37/38/53)`：市场号、未分配利润及每股值、
报告期比例、当前 EPS 和原生零分支，避免扩大全局 selector 注册后制造 A 股退化。

## 市场审计

全库审计现在按 selector 判断港股适用性：

- 全部 selector 位于上述集合时可生成自动上下文；
- 任一 selector 未闭合时，公式状态为 `market_inapplicable`；
- 响应通过 `unsupported_expansion_bindings` 给出精确键，例如 `FINANCE#23`；
- 不会为港股请求 A 股财务文档。

## 验证

- `tdx-hk-finance-tests`：通过 19 个 selector 的固定夹具、缩放、派生值和拒绝边界；
- `tdx-formula-engine-tests --domain context-and-library`：通过新增 selector 注册、解释器
  透传和港股逐 selector 市场审计；
- 真实 `31:00700`、30 根港股 K 线：自动加载
  `C:\new_tdx\T0002\hq_cache\hkcwdata.dat`，14 个请求 binding 全部执行；
  `FINANCE(1/7)` 均为 `9122883125`，H 股本、
  EPS、每股净资产、每股股息分别为
  `9122883125/20.061776/140.26/4.5`；
- 聚焦解释器审计中 `FINANCE(34/1/7)` 均通过，未支持的 `FINANCE(23)` 精确发布为
  `unsupported_expansion_bindings=FINANCE#23`；
- A 股 `sz:000001` 的新增六 selector 实测市场号 `0`、未分配利润
  `283713984000`、每股未分配利润 `14.6199717547514`、当前 EPS
  `0.748379924037351`、`FINANCE(53)=0`。

机器证据：

- `output/ida-tcalc-finance-evaluator-boundary-20260812.log`；
- `output/ida-tdxw-type103-hk-fallback-20260812.log`；
- `output/ida-probe-tdxw-finance-callback.log`；
- `output/ida-tdxw-type105-market-boundary-20260812.log`；
- `output/probes/market-instruments-hk31-20260808.json`；
- `output/probes/market-instruments-hk48-20260812.json`；
- `output/formula-hk-finance-capital-00700-20260812.json`；
- `output/formula-audit-hk-finance-20260812.json`；
- `output/formula-ashare-finance-added-selectors-000001-20260812.json`。
