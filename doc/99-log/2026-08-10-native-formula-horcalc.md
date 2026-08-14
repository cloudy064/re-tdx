# TCalc HORCALC 日线横向统计闭环

## 结果与边界

纯 C++ 公式解释器新增
`HORCALC('板块名',数据项,计算方式,权重)`。当前精确闭合的是日线自动上下文：

- 数据项 `100..106`：最高、开盘、最低、收盘、成交量、涨跌幅、成交额；
- 计算方式 `0/1/2`：求和、当前证券排名、平均；
- 权重 `0..4`：总股本、流通股本、等权、流通市值、总市值；
- 板块名复用 `BLOCKSETNUM` 的 `HY./GN./MY.` 强制目录和无前缀查找顺序。

自动上下文目前只接受 `period=day`。分钟、周线和月线明确报错，动态板块名及越界
参数也明确拒绝。它不需要 Level2、登录账号或券商私有状态；本批 IDA Python 仍
仅用于离线反编译证据，正式工具和服务完全是 C++。

[通达信官方 HORCALC 帮助](https://help.tdx.com.cn/gspt/docs/markdown/tdxgs-1d1k7biu16p6s/tdxgs-1d1p8b1jsdnhk.html)
给出的参数表与上述枚举一致。

## 原 DLL 行为

静态注册表将 `HORCALC` 映射到 opcode 1245、
`TCalc.dll!sub_10033710`。32 位原生探针给处理函数注入可控的板块成员、行情、
财务和 K 线回调，确认：

1. handler 通过 command 8、type 7 请求目标板块的跨证券数据；
2. 每只成员按目标证券日期对齐，缺失日期使用上一条有效记录；首条有效记录之前
   保持 0；
3. 求和与平均采用 float 累加；排名沿用 TCalc 容差比较；
4. 涨跌幅的负值是合法数据，不得按缺失值吞掉；
5. 总/流通股本权重读取财务值，市值权重再乘相应证券收盘价。

五根目标柱、两只成员的固定向量中，收盘等权求和为
`[5,25,27,38,38]`，平均为 `[5,12.5,13.5,19,19]`，排名为
`[1,2,2,2,2]`；成交量求和为 `[500,2500,2700,3800,3800]`，涨跌幅求和为
`[0,0,0.4,0.642857134,0.642857134]`。这同时覆盖中间缺日和末尾向前填充。

离线证据及 SHA-256：

- `output/ida-tcalc-horizontal-aggregates-handlers.json`：
  `71D90386347AB26425A65165F7C9717704549B437E32A7BF43F34E4DD5C771C2`；
- `output/ida-tcalc-horizontal-buffer-helpers.json`：
  `5D0FEF010F6218C396DBB54984BBECC0D786F003E2BE7D17367F85F29F331E42`；
- `output/native_probe_tcalc_horcalc.cpp`：
  `B9631A2787898454D5B770A50A5ADB7EEDDC7F8B94FCDEABC104B7FB6DD4D4A0`；
- `output/native_probe_tcalc_horcalc.exe`：
  `D05DBB29BA6B05D24ABF27DA6C54368B323AAD11FBF1A3262ED3920F01E4DA75`；
- `output/native-tcalc-horcalc-probe-v1.json`：
  `2AB4CED11077C7545531C961E048D2D5803FC3C5F5499838B9AD65814709BD8C`；
- `C:\new_tdx\TCalc.dll`：
  `13FACAA52DAC552C5BE1F63331781219DE9BF798C443AF8F5E4AFC193A02E7F5`。

## 纯 C++ 实现

新增 `daily.hpp/daily.cpp`，按通达信 32 字节 `.day` 记录读取日期、OHLC、成交额
和成交量；`blocks.cpp` 新增自定义 `.blk` 成员枚举。公式分析器只为四个静态参数
生成精确的 `HORCALC#<block>#<item>#<calc>#<weight>` 序列绑定，动态调用不会进入
自动上下文。

上下文按目标 K 线日期对齐全部成员日线，计算数据项并执行求和、排名或加权平均。
等权和所有求和/排名不访问财务接口；只有 `calc=2,weight!=2` 才批量读取股本，
只有 `weight=3/4` 才批量读取最新行情。API 的 `context_metadata` 透传解析目录、
板块代码、成员数、日线文件数、有效序列数和每个绑定的结果覆盖，模式固定为
`tcalc-opcode1245-command8-type7-local-day-date-aligned-horizontal-aggregate`。

真实 `HY.银行` 样本解析到研究行业 `881385/银行`，42 个成员全部有本地日线。
2026-08-07 的结果为：

- 等权：总和 `368.3199768066406`、平均 `8.769523620605469`、平安银行排名 `10`；
- 总股本权重平均 `7.843054294586182`；
- 流通股本权重平均 `7.658320903778076`；
- 总市值权重平均 `10.55433464050293`；
- 流通市值权重平均 `11.010936737060547`。

真实样本文件为 `output/native-horcalc-bank-v1.json`（SHA-256
`9C6C878A7AF65EC8AE0352B8DCF0D860F2471872BAAB897DE4D81035DC78E65E`）和
`output/native-horcalc-bank-weighted-v1.json`（SHA-256
`4B55846557BDC9D7CCBC9311505B1AD4FA72A010F769D7167623AC6474985D83`）。

## 覆盖率与后续边界

能力清单由 227 增至 228 个支持函数，自动符号仍为 69；390 条静态注册名称中
识别并集为 280，剩余 110。379 条内置公式保持源码、语法和数值安全
`379/379`，降级数值输出为 0。

`INSORT/INSUM` 已可复用本批板块解析、日线载入与跨证券对齐，但不能直接等价为
收盘价排行/求和：二者要按名称调用另一个活动指标，还需还原公式目录、输出线选择、
嵌套参数与缓存生命周期。L2 专用变体继续排除。该边界已写入
`output/native-formula-registry-next-audit-v8.json`，后续优先处理普通非 L2 路径。

## 验证、产物与部署

验证结果：

- 新增 `.day` 与 HORCALC 离线测试，CTest `102/102`；
- 候选专项健康/覆盖/HORCALC 契约 `3/3`；
- 候选 full API 契约 `219/219`；
- 正式 8765 同三项 `3/3`，健康状态保持 `native_cpp=true`、
  `python_runtime=false`。

本批核心产物及 SHA-256：

- `output/native-formula-coverage-horcalc-v8.json`：
  `19C147836C6B1E4E47F4EA861B1770669044E30C715D4C9619068FAC2A5A6AC5`；
- `output/native-formula-registry-next-audit-v8.json`：
  `034D3FEC1B411261CDC00E8F127F7DEBB0CA36A6268368D284E6124BFD18C1A2`；
- `output/native-horcalc-v1-candidate-api.json`：
  `C44093DA6B7E1A16C1253573F1037753E33A9E93BEEB3E442E3B22CABF413F2F`；
- `output/native-horcalc-v1-candidate-full-api.json`：
  `C1D488D32FB63FC93FDFBDFDE466AA72473052495097CFB6B5750EB7C0088A39`；
- `output/native-horcalc-v1-formal-final-api.json`：
  `1B2F93FAA26ACFE9A6831C588AD4540A439CF5F680798F47B3E7311F8EC915B6`。

正式 build/dist EXE 均为 21,905,954 字节，SHA-256
`A9E3ADC1140178115B4922499EF953AFF87B9BFDC636467D0C19016D058625DA`。
服务 PID 41868，仅监听 `127.0.0.1:8765`。旧正式版保存在
`output/tdx-tool-horcalc-v8-predeploy-rollback-20260810.exe`，大小 43,403,234 字节，
SHA-256 `DCA124143C44231D6AF057BFD2690D87C64E86CA048EBDB3BEE85FCD032E942C`；
临时 8875 已关闭。
