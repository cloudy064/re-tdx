# TCalc 未来函数只读执行模式

## 结论

`TCalc.dll` 中 18 条含未来函数的公式已全部进入纯 C++ 解释器，但执行边界被拆成
两条互不混用的通道：

- 普通执行仍为 217 条纯行情公式、319 条带只读市场上下文公式；可用于计算、
  条件扫描和专家回测；
- 未来函数公式只有在 CLI 传入 `--allow-future` 或 HTTP 传入
  `allow_future=1` 时才执行，响应固定标注
  `future_execution_mode=explicit-read-only-lookahead`、
  `scan_allowed=false`、`backtest_allowed=false`。

因此，解释器现在可运行体检的公式总数为 **337**，但扫描/回测的安全兼容数仍是
**319**，没有把重绘信号伪装成可交易历史信号。

## 原生证据

`TCalc` 内置帮助和静态描述符确认：

| 函数 | 原生证据 | 已实现语义 |
| --- | --- | --- |
| `BACKSET(X,N)` | 帮助示例说明 `N=2` 标记当前及前一周期 | 条件成立时回填当前与前 `N-1` 根 |
| `REFX/REFXV` | 帮助分别标为未来平滑/未来不平滑引用 | 越过右边界时仅 `REFX` 复用末值 |
| `DRAWLINE/PLOYLINE` | 帮助给出两锚点、延长和折线连接定义 | 线性插值；`DRAWLINE(...,1)` 按斜率右延 |
| `BARSNEXT` | 帮助定义为下一次条件成立距当前周期数 | 反向扫描下一次非零位置 |
| `INCLUDED/INCLUDEDV` | 帮助定义向前/向后 K 线包含和实体/高低模式 | OHLC/实体区间包含，`N=0` 搜索全部方向 |
| `ZIG` | 静态描述符将名称绑定到 `sub_100227A0` | 按百分比确认转向点，逐段线性连接并重绘当前段 |
| `PEAK/TROUGH` | 分别绑定 `sub_10038F40/sub_100393C0`，内部调用 `sub_100227A0` | 从 ZIG 转向序列维护第 `M` 个波峰/波谷值 |

同时补齐 `BARSSINCE`、`DATETODAY`、`DRAWNUMBER_DIF` 和 `PARTLINE` 的公式
依赖。`PARTLINE` 只改变绘图属性，数值结果保持首个价格参数，修复了此前
`WAVEKX` “波段”输出被错误压成全零的问题。

离线 IDA 探针为 `output/ida_probe_tcalc_future_functions.py`，日志为
`output/ida-tcalc-future-functions.log`。

## 覆盖结果

未来函数公式 **18/18** 可显式只读执行：

- 7 条技术指标：`XT`、`SQJZ`、`ICHIMOKU`、`CYX`、`WAVE`、`NXTS`、
  `WAVEKX`；
- 11 条五彩 K 线：`WYGD`、`SGCJ`、`SZTAI`、`PINGDING`、`PINGDI`、
  `HYFG`、`TKQK`、`SFWY`、`SSSBQ`、`XDSBQ`、`FENLI`。

以平安银行 800 根日线和完整市场上下文运行全库审计：

- eligible 337；passed 337；errors 0；
- 18 条未来函数公式全部通过，全部有数值输出；
- 全库 336 条有数值末值，唯一例外仍是只适用于盘中周期的参考结算价。

审计结果保存在
`output/tcalc-formula-context-runtime-audit-future-readonly.json`，覆盖分析保存在
`output/formula-coverage-future-readonly.json`。

## 使用

```powershell
tdx-tool formulas evaluate --root C:\new_tdx --formula XT `
  --market sz --code 000001 --pages 2 --page-size 400 --allow-future

tdx-tool formulas audit --root C:\new_tdx --market sz --code 000001 `
  --with-context --allow-future
```

HTTP 等价调用：

```text
/api/v1/formulas/evaluate?market=sz&code=000001&formula=XT&allow_future=1
```

不带显式开关时，未来公式会返回清晰的拒绝原因。`formulas scan` 和
`formulas backtest` 没有该开关，也不会接受未来函数公式。
