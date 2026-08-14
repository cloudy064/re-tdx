# TCalc 市场基准 BETA 与 SUMBARSX

## 目标

上一批保留了两个明确缺口：`SUMBARSX` 的优化处理函数存在当前柱边界歧义，
`BETA(N)` 缺少宿主证券到市场基准的选择规则。本轮不按函数名称猜测，而是把
静态注册、处理函数、宿主加载器和原 DLL 固定输出向量串成闭环，再落为纯 C++
实现。函数用途与参数形态同时用
[通达信官方函数表](https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html)
交叉核对；计数容差、预热和基准映射以当前 DLL 为准。

## 静态证据

`output/ida_probe_tcalc_sumbarsx_beta.py` 对现有 IDA 数据库做离线反编译，输出：

- `output/ida-tcalc-sumbarsx-beta.log`；
- `output/ida-tcalc-sumbarsx-beta-loader.log`；
- `output/ida-tcalc-sumbarsx-beta-constants.log`；
- `output/ida-tcalc-sumbarsx-beta-final.log`。

注册表与处理函数为：

| 函数 | opcode | 处理函数 |
| --- | ---: | --- |
| `SUMBARS` | 1137 | `sub_1001ABE0` |
| `SUMBARSX` | 1362 | `sub_1001AE30` |
| `BETA` | 1159 | `sub_10038980` |

`BETA` 调用的宿主基准加载器是 `sub_10023F40`。它读取当前证券的市场字段和
代码，不使用固定的单一沪深指数。恢复出的选择规则如下：

| 当前证券 | 基准 |
| --- | --- |
| 深市普通证券 | `sz:399001` |
| `399006` 或代码以 3 开头且第二位不是 9 | `sz:399006` |
| 沪市普通证券 | `sh:999999` |
| `000688`、688/689 开头证券 | `sh:000688` |
| 北交所 | `bj:899050` |
| 市场 27/31/48/49/71 | `27:HSI` |
| 市场 47/28/29/30/66 | 合约代码的非数字前缀加 `L9` |

`0x100E8E14` 附近常量区也直接包含 `HSI`，与加载器分支一致。

## 原 DLL 输出向量

仅靠优化后的伪代码不能可靠确定 `SUMBARSX` 的 -1/0 和历史不足边界，因此新增
32 位纯 C++ 探针 `output/native_probe_tcalc_sumbars.cpp`。探针直接调用原 DLL，
结果保存在 `output/native-tcalc-sumbars-probe.json`。关键向量为：

| 输入 / 目标 | `SUMBARS` | `SUMBARSX` |
| --- | --- | --- |
| `10,1,1,1` / 5 | `0,1,2,3` | `-1,1,2,3` |
| `5,1,4,2` / 5 | `0,1,2,2` | `0,1,1,1` |
| `1,4,1,4` / 5 | `0,1,2,2` | `无效,1,1,1` |
| `1,2,3,4` / 6 | `0,1,2,2` | `无效,无效,2,1` |
| `1,1,1,1` / 9 | `0,1,2,3` | 全部无效 |

由此确认原解释器的 `SUMBARS` 实现通常把达到目标的前序柱多计一柱，而且没有
保留最左边界的封顶规则。本轮同步修正，而不是只增加 `SUMBARSX`。

另一个 32 位探针 `output/native_probe_tcalc_beta.cpp` 在加载后的进程内临时替换
分配器、CLOSE 与基准加载调用，以确定性数组调用 `BETA`，退出前恢复全部修改；
不修改磁盘 DLL，也不访问网络。证据保存在
`output/native-tcalc-beta-probe.json`。个股 CLOSE
`50,60,48,67.2,67.2,80.64`、基准 CLOSE
`100,110,99,118.8,118.8,130.68`、`N=3` 时，原 DLL 输出从第三点起约为
`0.5,2,2,2`。它还证明：首个对齐 CLOSE 原样保留，从第二点起转简单收益率，
最后按个股/基准协方差除以基准方差计算。

## 纯 C++ 实现

`native/src/formula_engine.cpp` 新增 `BETA` 和 `SUMBARSX`，支持函数总数由 172
增至 174，并新增精确能力分组：

- `custom_formula_benchmark_cumulative_function_count=2`；
- `custom_formula_benchmark_cumulative_functions=[BETA,SUMBARSX]`。

`SUMBARS/SUMBARSX` 共用原生 float 舍入、绝对 `1e-5` 与相对 `1e-7` 容差，
但分别保留不同的计数和不足历史语义。`BETA` 从自动上下文读取
`__BETA_BENCHMARK_CLOSE`，按原 DLL 转简单收益率，再复用已经验证的 `BETAEX`。

`native/src/formula_context.cpp` 实现 `sub_10023F40` 映射，按目标证券相同周期、
分页和起点下载基准 K 线，以 `date|time` 对齐 CLOSE。实际选择通过
`context.beta_benchmark.security/market/code/mode` 回传；模式固定为
`TCalc-sub_10023F40-native-security-market-benchmark-selection`，便于调用方审计。
只有公式依赖 `BETA` 时才加载基准，常规公式不会增加第二次行情请求。

Svelte 公式库增加“基准/累加核心”能力标签。运行时仍完全由 C++ 完成；IDA
脚本只产生离线证据，不进入工具、服务或网页运行链路。

## 契约与验证

新增 `formula-benchmark-cumulative-inline-post`，在平安银行 120 根真实日线上同时
断言：

- 分析器把 `BETA` 标为可自动补齐上下文；
- `sz:000001` 精确绑定 `sz:399001`，且上下文序列实际存在；
- `SUMBARS` 为 `0,1,...,5`，`SUMBARSX` 为 `-1,1,...,4`；
- 目标 200 历史不足时，`SUMBARS` 返回 119，`SUMBARSX` 保持无效；
- 最新 20 周期 `BETA` 为有限值。

发布验证：

- CTest：101/101；
- Svelte：0 错误、0 警告，生产构建成功；
- 公式覆盖：174 个支持函数，新增分组精确为 2 项；
- 临时服务专项：4/4；
- 临时服务 full：206/206；
- 正式服务专项：4/4。

正式 EXE SHA-256 为
`FB211A5B6EE6F7853F32986689FC1038E581B3A9E15014EA5EB092C4CCF9193B`；网页资源为
`assets/index-Dsz1Pn3Z.js` 和 `assets/index-BBbdHCcI.css`。服务 PID 29068，
仅监听 `127.0.0.1:8765`，健康信息继续为 `native_cpp=true`、
`python_runtime=false`。
