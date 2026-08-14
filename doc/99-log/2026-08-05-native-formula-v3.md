# 原生指标 v3：成交量、能量与均线公式扩展

## 本轮结果

- 从 `TCalc.dll` 的签名恢复结果确认 7 个系统技术指标的参数、范围与输出槽；
- 纯 C++ `formulas calculate` 从 13 类扩展至 20 类，新增 `VOL`、`OBV`、
  `PSY`、`VR`、`BRAR`、`BBI`、`EXPMA`；
- TPool 规则兼容表同步识别这 7 个指标，不会出现计算器可用但池规则被拒绝；
- `/api/v1/formulas/calculate` 与 Svelte 公式验证台同步开放新增指标；
- 引擎标识升级为 `tdx-native-compatible-v3`，仍明确标注 `dll_loaded=false`，
  不宣称支持任意 TCalc 字节码或用户公式。

## 已确认的 TCalc 签名

| 公式 | 默认参数 | 参数范围 | 输出槽 |
| --- | --- | --- | --- |
| VOL | M1=5, M2=10 | 2..500 | VOLUME, MAVOL1, MAVOL2 |
| OBV | M=30 | 2..100 | OBV, MAOBV |
| PSY | N=12, M=6 | 2..100 | PSY, PSYMA |
| VR | N=26, M=6 | 2..100 | VR, MAVR |
| BRAR | N=26 | 2..120 | BR, AR |
| BBI | M1=3, M2=6, M3=12, M4=24 | 2..100 | BBI |
| EXPMA | M1=12, M2=50 | 2..500 | EXP1, EXP2 |

参数和输出槽来自 `output/tcalc-all-formulas-with-signatures.json`。公式数值由
64 位工具按通达信标准函数语义直接计算，不加载 32 位 DLL。

## 成交量单位边界

真实 7709 数据表明，个股 K 线返回的成交量以股计；通达信公式系统中的
`VOL` 以手计。计算器因此保留点上的原始 `volume`，只对非指数的公式输入
执行 `/100`；指数公式输入保持协议原值。输出增加：

```json
{
  "formula_volume_unit": "hand",
  "formula_volume_divisor": 100
}
```

上证指数对应为 `index-native` 和 `1`。这项处理同时作用于 `VOL`、`OBV`、
`VR`，防止个股结果整体放大 100 倍。

## 真实数据验证

平安银行 `SZ000001` 的 90 根日线在同一 7709 主站逐项完成计算，2026-08-05
末值均有效：

```text
VOL    VOLUME=1511509.92, MAVOL1=1719235.488, MAVOL2=1436207.4
OBV    OBV=7759033.88, MAOBV=884462.8507
PSY    PSY=66.6667, PSYMA=79.1667
VR     VR=249.7717, MAVR=264.6277
BRAR   BR=117.8991, AR=181.3135
BBI    BBI=11.2773
EXPMA  EXP1=11.2646, EXP2=10.9497
```

`EXPMA` 另以 2 页、160 根 30 分钟线验证，末值为 `EXP1=11.3045`、
`EXP2=11.3568`。真实输出保存在 `output/formula-v3/`。

## 验证边界

- 单元测试用可手算序列覆盖预热区、参数默认值、输出顺序和 7 个新增公式；
- 个股与指数各验证一次公式成交量/原始成交量比例；
- 当前属于标准公式兼容实现，TCalc 内部公式正文仍未被直接导出，不能把结果
  描述成 DLL 内部执行结果；
- 下一批优先候选为 `DMI/WVAD/EMV/ASI/CHO`，需要先继续核对递推初值和
  通达信特有缩放常数，再决定是否进入稳定接口。
