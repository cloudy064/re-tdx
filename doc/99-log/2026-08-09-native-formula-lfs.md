# TCalc LFS 历史流通股本换手闭环

## 目标与结论

本轮继续处理 TCalc 静态注册表中不需要登录或 Level2、但需要宿主历史序列的
高收益入口，完成 `LFS` 的注册、处理函数、TdxW type 103 分派、32 位原 DLL
固定向量和纯 C++ 自动上下文闭环。

`LFS` 不是普通 EMA，也不是按名称猜测的“流通市值”。它用每根 K 线的原始
成交量除以当日流通股本得到换手率，再维护两个带 `(1-换手率)` 衰减的快慢状态，
输出二者的相对差。运行时不加载 `TCalc.dll/TdxW.exe`，不启动 Python；Python
只用于离线驱动 IDA 生成反编译证据。

## TCalc 注册、宿主请求与算法

当前 DLL 指纹下的静态记录为：

| 名称 | opcode | 处理函数 | 宿主类型 |
| --- | ---: | --- | ---: |
| `LFS` | 1197 | `sub_1002CC30` / `0x1002CC30` | 103 |

权威注册行在 `output/ida-tcalc-function-registry-jsonl-v4.log`。处理函数及两个辅助
函数的反编译保存在 `output/ida-tcalc-lfs-helpers.log`，离线探针为
`output/ida_probe_tcalc_lfs_helpers.py`。

`sub_100108D0` 把全部 35 字节 K 线日期组装成 `YYYYMMDD` 数组，请求宿主 type
103，并取得每柱两个 float；`LFS` 使用第二个 float，即流通股本。K 线偏移
`+27` 是原始成交量；既有 `VOL` 处理函数也独立确认了该偏移。

股本序列先按原处理函数规范化：

- 判定无效的精确条件为
  `capital - abs(capital)*1.000000011686097e-7 - 9.999999747e-6 < 1`；
- 前导无效股本保持缺失，并把首个有效点向后移动；
- 序列内部无效股本继承上一有效股本；
- 全序列无有效股本时全部缺失。

从首个有效股本开始，令 `t=raw_volume/capital`：

```text
首点：fast=t, slow=t
后续：fast=t+(1-t)*(4/5)*previous_fast
      slow=t+(1-t)*(12/13)*previous_slow
输出：(1-fast/slow)*100
```

每个原生存储点都按 float 舍入。首个有效柱成交量为零时 `0/0` 输出缺失，但
下一根正成交量会从两个零状态恢复。恒定换手率序列的输出持续上升，因此不能
用普通 EMA 或现有指标近似替代。

`sub_10097A60` 还保留指数类证券门控：深市排除 `39*`；沪市排除 `8*`、数值
代码 `<=999` 或 `>=990000`；北市排除 `899000..899999`。其他市场号不由这个
帮助函数排除。

## TdxW type 103 与公共历史股本

TdxW type-103 分派和历史选择链保存在：

- `output/ida-tdxw-option-resources-v3.log`；
- `output/ida_probe_tdxw_type103_capital.py`；
- `output/ida-tdxw-type103-capital-v2.log`。

`sub_526A10` 转入 `sub_519DB0`，从 29 字节股本变更记录中按市场、代码、日期和
类别 2/3/5/7/8/9/10 选择有效记录，并向后填充请求日期；缺少本地历史时回退
当前证券记录中的流通股本。项目现有 `fetch_capital_changes_document`、
`circulating_capital_events` 和 `historical_capital_points` 已经恢复同一 0x000f
数据链，因此无需新增网络协议或 L2 权限。

公式公开的 `CAPITAL` 单位是手，而 type 103 与原始成交量单位均是股。纯 C++
执行前把逐日 `CAPITAL` 乘 100 还原为股，不仅保持换手率一致，也保留原 DLL
“小于 1 股无效”的阈值。

## 32 位原 DLL 固定向量

新增 `output/native_probe_tcalc_lfs.cpp` 和 32 位
`output/native_probe_tcalc_lfs.exe`。探针把伪宿主回调写入 TCalc 模块 RVA
`0x310BF0`，为 type 103 注入可控股本，并直接调用 RVA `0x2CC30`。结果保存在
`output/native-tcalc-lfs-probe.json`：

```text
standard:
[0,3.59550452,13.6147108,7.20301151,16.1161156,14.9956331,5.48789358,17.6482849]

constant_turnover:
[0,6.05042219,11.2079668,15.5987787,19.3320427,22.5021114,25.190485,27.4674435]

leading_capital_gap:
[null,null,0,0.863314271,10.9888277,11.316988,4.4044776,16.7228985]

zero_volume:
[null,0,13.3333368,6.32656908,15.5171022,26.7814884,25.0766449,14.2726002]
```

此外还锁定内部股本缺口与标准序列完全一致、全零股本全部缺失。纯 C++ 单元测试
逐点核对全部六组向量，并额外核对深证指数门控。

## 纯 C++ 接入与 API

解释器新增 `LFS()`，并发布：

```json
{
  "custom_formula_capital_turnover_function_count": 1,
  "custom_formula_capital_turnover_functions": ["LFS"]
}
```

支持函数由 207 增至 208，自动符号保持 54，静态注册证据保持 390。分析器将
`LFS` 标为自动宿主依赖，同时列出 `market_dependencies=["CAPITAL","VOL"]`。
自动上下文仅在实际依赖 `LFS` 时取得当前流通股本和 0x000f 变更历史，生成
逐 K 线 `CAPITAL` 序列，并复用既有 `capital_series_*` 元数据。

现有 `formula-sequence-statistics-inline-post` 契约追加 `LF:LFS();`，核对末值有限
且上下文模式为 `tdx-0x000f-historical`，没有增加 API 契约总数。

平安银行正式 API 固定证据保存在 `output/native-lfs-real-api.json`：120 根日线
最新日期 `2026-08-07`，`LFS=59.171077728271484`；共使用 46 个流通股本事件，
最早 `1991-05-02`、最新 `2025-06-30`。

## 验证与发布

- `tdx-formula-engine-tests`：原 DLL 六组向量、市场门控和依赖分析通过；
- `tdx-recon-contract-tests`：能力清单和 LFS API 断言通过；
- CTest：101/101；
- 临时服务专项：3/3；
- 临时服务 full API：211/211；
- 正式服务专项：3/3。

专项和全量报告分别为 `output/native-lfs-targeted-api.json`、
`output/native-lfs-full-api.json`、`output/native-lfs-formal-api.json`。正式 EXE
SHA-256 为
`6866057CF63343DD0338834DC3169EFE65DFD3EFCBB4F666A9F44B0BF505244A`；服务 PID
41884，仅监听 `127.0.0.1:8765`，健康检查为 `native_cpp=true`、
`python_runtime=false`。网页资源没有变化。

## 边界

`LFS` 依赖流通股本变更历史的完整性；当前值和已知事件之间没有被上游提供的
缺口不会被推测补齐。扩展市场仍沿用现有自动 `CAPITAL` 上下文边界，不因为
算法本身可计算就伪造合约股本。该入口使用公共历史股本和 L1 成交量，不包含、
推导或绕过任何 Level2 数据。
