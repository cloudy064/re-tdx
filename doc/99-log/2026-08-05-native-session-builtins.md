# TOTALFZNUM、HOUR/MINUTE 与 REFDATE

> 日期：2026-08-05。范围：非 L2 的 TCalc 公式解释器；运行时为纯 C++。

## 结论

四个此前缺失的会话/时间内建量已按 TCalc 求值器实现：

| 内建量 | TCalc 求值器 | 精确语义 | 解锁公式 |
| --- | --- | --- | --- |
| `TOTALFZNUM` | `sub_100174A0` | 最多四段交易时段的总分钟数 | `VOL-TDX`、`AMO-TDX` |
| `HOUR` | `sub_10006440` | 每根 K 线记录的小时字节 | `JSJG` |
| `MINUTE` | `sub_10006480` | 每根 K 线记录的分钟字节 | `JSJG` |
| `REFDATE(X,D)` | `sub_100074F0` | 取不晚于目标日期的最后一根 X，并广播到全段 | `BSQJ` |

## 证据与边界

`TOTALFZNUM` 请求宿主 type 105。TdxW case 105 把 set-code 0/1/2 的四段
时段写入返回偏移 `15..29`；当前静态默认值均为 `570–690`、`780–900`、
`900–900`、`900–900`，所以沪、深、北三个已支持市场都是 240 分钟。其他
市场会从证券交易模板读取自定义时段；本工具目前的 K 线入口只接受
`sz/sh/bj`，没有把期货夜盘错误归为 240 分钟。

`REFDATE` 的日期编码与 `DATE` 一致，为 `(year-1900)*10000+month*100+day`。
第二参数只取末值；找不到不晚于目标的柱时保留空值。这一点与逐柱动态
回溯不同，单元测试覆盖了指定历史日和 `DATE` 最新日两种情况。

离线证据：

- `output/ida-probe-tcalc-next-builtins.log`
- `output/ida-probe-tdxw-market-sessions.log`

## 验证

- `VOL-TDX`：平安银行 800 根日线，末值含 `VOLUME/MAVOL1/MAVOL2`；
- `AMO-TDX`：平安银行 800 根日线，末值含 `AMOW/AMO1/AMO2`；
- `JSJG`：平安银行 800 根 1 分钟线，末值含逼近结算价和参考结算价；
- `BSQJ`：平安银行 800 根日线，末值正常给出持仓状态；
- MinGW CTest `24/24` 通过。

整库覆盖由 310 增至 314，直接执行由 214 增至 216，技术指标含上下文覆盖
由 164 增至 168。314 条日线审计全部无运行错误；313 条有数值末值，唯一
日线全空的 `JSJG` 按源码本就要求 `PERIOD=0`，其 1 分钟验证已有有效结果。

对应输出：

- `output/tcalc-formula-coverage-session-symbols.json`
- `output/tcalc-formula-coverage-refdate.json`
- `output/tcalc-formula-context-runtime-audit-refdate.json`
- `output/formula-vol-tdx-totalfznum-000001.json`
- `output/formula-amo-tdx-totalfznum-000001.json`
- `output/formula-jsjg-hour-minute-000001.json`
- `output/formula-bsqj-refdate-000001.json`
