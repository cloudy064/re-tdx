# 期权目录、标的映射与 TQQCalc 波动率闭环

## 结论

通达信扩展目录同时包含可交易商品期权与股指期权。期权线上代码不是显示名，
而是最长 9 字节、可能含内部 ASCII 空格的 wire code，例如大商所
`5:A 8X06SH` 对应显示名 `A2609-C-4400`。纯 C++ 统一工具现已支持这种代码，
并新增：

- `market options`：扫描 7727 `0x23F0/0x23F5` 目录，规范化期权与标的关系；
- `market option-expiry`：读取客户端当前产品规则与交易所节假日，复现精确
  到期日计算；
- `market option-chain`：复用单条 7727 会话批量拉取整条期权链，计算波动率
  曲面、Greeks、Put/Call 比率和持仓量最大痛点；
- `market option-volatility`：通过期权与标的两条 `0x23FF` 日线，复现 TDX
  历史波动率、隐含波动率、模型价格和 Greeks；
- 通用公式解释器已经支持 `IVOLAT(N,0/1)`，会自动获取对应标的日线；
- `/api/v1/market/options`、`/api/v1/market/option-expiry` 与
  `/api/v1/market/option-chain`、`/api/v1/market/option-volatility`：等价只读 API；
- 进程内 300 秒目录缓存，避免网页重复请求时每次重扫约 2.5 万条当前目录。

运行时不加载 DLL、不注入客户端，也不依赖 Python。

## TQQCalc.dll 算法证据

真实 DLL 的四个导出为：

| 导出 | 地址 | 结论 |
| --- | --- | --- |
| `TQQCalc_Lsbdl` | `0x100018E0` | 收盘对数收益率总体方差，乘 `sqrt(250)`，最终四位小数 |
| `TQQCalc_Lsbdl_Model` | `0x10001A00` | 六类历史波动率模型与可选年化 |
| `TQQCalc_Yhbdl` | `0x10001ED0` | 用 `sigma+0.0001` 数值斜率反求隐含波动率 |
| `TQQCalc_Index` | `0x100021E0` | 价格、隐波和 Greeks 的组合入口 |

`TQQCalc_Yhbdl` 的模型分支为：

- `model=0`：欧式闭式模型；现货采用 Black-Scholes，期货采用贴现后的
  Black-76；
- `model=1`：美式二叉树；全局节点数为 20。原 DLL 使用 20 个终端节点和
  19 次回溯，而不是常见的 21 节点实现，C++ 保留该行为；
- 欧式最多迭代 5000 次，美式最多 10 次；下限为 `0.0001`，结果按 DLL 的
  `+0.50300002098` 行为保留四位；
- 正态分布函数使用 DLL 自带的五项多项式近似，而不是平台 `erf`。

`TQQCalc_Index` 中的 Greeks 路径也已落入 C++：欧式 BSM/Black-76 使用闭式
解，美式保持 DLL 的 20 节点树并以邻近价格/波动率/期限/利率重新定价。结果
包含 `model_price`、`delta`、`gamma`、`theta`、`vega` 和 `rho`，并保留原 DLL
的四位舍入行为。

IDA 日志为 `output/ida-tqqcalc.log`，离线脚本为
`output/ida_probe_tqqcalc.py`。

## TdxW 宿主 selector 35

TdxW 中 `sub_609C40` 是 `IVOLAT` 使用的历史波动率实现：对最多 N 个标的
日收盘价计算相邻对数收益，价格不大于 `1e-4` 时该项记 0，使用总体方差，
最后乘 `sqrt(250)`。与独立导出 `TQQCalc_Lsbdl` 不同，宿主路径不做最终
四位小数舍入。

期权元数据由宿主报价记录提供：

| 期权市场 | 标的市场 | 模型 |
| --- | --- | --- |
| 4 郑商所 | 28 | 期货 Black-76，当前合约美式 |
| 5 大商所 | 29 | 期货 Black-76，当前合约美式 |
| 6 上期所 | 30 | 期货 Black-76，当前合约美式 |
| 67 广期所 | 66 | 期货 Black-76，当前合约美式 |
| 7 中金所 | 47 | 现货 BSM，欧式 |

中金所根映射按宿主原值为 `IO→IF300`、`HO→IH50`、`CO→IC500`、
`MO→IM1000`、`ZO→IZ100`。商品期权使用显示名行权月份作为标的，例如
`A2609-C-4400→29:A2609`。宿主默认 `[RATE] NORISK_RATE=0.0187`；当前本机
`CalcUseJyDayNum=0`，到期年限为 `(自然日差+1)/365`。

宿主反编译日志为 `output/ida-tdxw-volatility-helpers.log`，脚本为
`output/ida_probe_tdxw_volatility_helpers.py`。

## 精确到期日资源与规则

到期日并不依赖此前推测的 `hkqzinfo.dat`。TdxW 的真实数据链为：

- `T0002/hq_cache/code2name_qq.ini`：每个期权产品的规则代码、参数、切换合约
  和切换合约的精确到期日；
- `T0002/hq_cache/neednote.dat`：`[Data] RecentHSHoliday` 提供沪深/境内交易所
  非交易日，当前安装覆盖 2026 年；
- `sub_522DC0` 解析 17 列产品记录，`sub_528840` 选择切换日或规则计算，
  `sub_650DA0` 执行星期与节假日调整。

当前资源实际使用七类规则：`d/3`、`h/12|5`、`i/5|10|13`、`j/3`、
`r/3`、`}/3`、`~/12`。C++ 按资源值执行这些规则，并保留切换合约的精确
日期；例如 `A2609-C-4400` 自动解析为 `2026-08-18`。证据脚本和日志为：

- `output/ida_probe_tdxw_option_rules_protocol.py`；
- `output/ida_probe_tdxw_option_holidays.py`；
- `output/ida-tdxw-option-rules-protocol-v5.log`；
- `output/ida-tdxw-option-holidays.log`。

## 使用

```powershell
tdx-tool market options --market DCE --contract A2609 --type call

# 单独查看本地规则推导结果与证据来源
tdx-tool market option-expiry `
  --root C:\new_tdx --security '5:A 8X06SH' --name A2609-C-4400

# 显示名可跳过首次全目录查找；到期日默认从本地规则自动解析
tdx-tool market option-volatility `
  --root C:\new_tdx --security '5:A 8X06SH' `
  --name A2609-C-4400 --lookback 60

# 显式日期可用于历史复核，并优先于本地规则
tdx-tool market option-volatility `
  --security '5:A 8X06SH' --name A2609-C-4400 `
  --expiry 20260807 --lookback 60

# 整条 A2609 期权链：实时价、五档顶层、成交/持仓、IV 和 Greeks
tdx-tool market option-chain --root C:\new_tdx `
  --market DCE --contract A2609 --lookback 60

# IVOLAT 的第二参数：0=历史波动率，1=隐含波动率
tdx-tool formulas evaluate --root C:\new_tdx --formula VOLATILITY `
  --market 5 --code 'A 8X06SH' --option-name A2609-C-4400 `
  --period day --pages 1 --page-size 120 --param N=60
```

API：

```text
/api/v1/market/options?market=DCE&contract=A2609&type=call
/api/v1/market/option-expiry?market=5&code=A%208X06SH&name=A2609-C-4400
/api/v1/market/option-chain?market=DCE&contract=A2609
/api/v1/market/option-volatility?market=5&code=A%208X06SH&name=A2609-C-4400
```

## 真实验收

2026-08-06 的 7727 目录实测扫描 25,180 条可读记录；大商所 `A2609` 匹配
62 个期权。`5:A 8X06SH` 的期权日线成功返回 20 条，`29:A2609` 标的日线
成功返回 80 条。

以 2026-08-05 为计算日、60 个标的收盘价计算得到历史波动率
`0.1154709035`（11.5471%）。未传到期日时，本地规则将 `A2609-C-4400`
解析为 2026-08-18，并以 13 个自然日进入隐波、模型价格和 Greeks 分支。该
期权收盘价比内在价值低 0.5，原始求解器因此返回隐波下限 `0.0001`，不是
传输或计算失败。样本位于：

- `output/probes/options-A2609.json`；
- `output/probes/option-A2609-C-4400-expiry.json`；
- `output/probes/option-A2609-C-4400-volatility-auto-expiry.json`；
- `output/probes/formula-volatility-A2609-C-4400-auto-expiry.json`。

整链真实验收使用 `0x23FA` 的持久连接一次取回 `A2609` 的 62 个合约，而不是
为每个合约重复握手：共形成 31 个行权价，62 个合约全部取得行情并计算 IV；
标的现价 `4724`，ATM 与最大痛点均为 `4700`，成交量 Put/Call 比为
`0.6577535`，持仓量 Put/Call 比为 `0.7089784`。冷启动约 28 秒主要用于首次
扫描约 2.5 万条扩展目录；常驻服务后续请求复用 300 秒目录缓存。样本位于
`output/probes/option-chain-A2609-live.json`。

25 项原生测试全部通过，新增测试覆盖 wire code、标的映射、目录过滤、DLL
正态分布近似、历史波动率、欧式/美式价格—隐波互逆、Greeks、规则到期日，
以及 `IVOLAT` 两种模式的通用公式执行。

## 仍保持的边界

到期日计算只执行当前 `code2name_qq.ini` 中实际出现且已逆向确认的七类规则，
不会为未知规则套用经验日期。历史合约若早于资源的切换边界，或者
`neednote.dat` 没覆盖目标年份，会明确返回不可解析；此时仍可计算历史波动率，
也可用显式 `--expiry` 做历史复核。资源由通达信盘后/行情维护链更新，因此
长期运行的工具应同步更新该安装目录，而不是固化一份节假日表。
