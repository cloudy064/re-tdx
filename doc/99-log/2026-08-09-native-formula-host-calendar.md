# TCalc ISJYDATE / LOCALDAYNUM 宿主日历闭环

## 目标与结论

本轮继续处理 TCalc 静态注册表中需要宿主回调、但不需要登录或 Level2 的候选，
完成 `ISJYDATE` 与 `LOCALDAYNUM` 的处理函数、TdxW type 122/168 分派、本地文件
路径和 32 位原 DLL 固定向量对照，并接入纯 C++ 解释器：

- `ISJYDATE` 比较 TdxW 当前交易日与机器本地日期，相等为 1，否则为 0；
- `LOCALDAYNUM` 返回本地日线文件的 32 字节记录数；沪深北本地最后日期落后
  当前交易日时再加 1；
- 两项都是零参数宿主值并广播到每根 K 线，同时兼容公式中的无括号写法。

运行时不加载 `TCalc.dll/TdxW.exe`，也没有 Python 转发。Python 仅用于离线驱动
IDA 生成反编译证据。

## TCalc 注册与处理函数

当前 DLL 指纹下的静态注册记录为：

| 名称 | opcode | 处理函数 | 宿主类型 |
| --- | ---: | --- | ---: |
| `ISJYDATE` | 1378 | `sub_10035230` / `0x10035230` | 122 |
| `LOCALDAYNUM` | 1367 | `sub_10026AC0` / `0x10026AC0` | 168 |

`output/ida-tcalc-next-unlicensed.log` 显示：`ISJYDATE` 请求 96 字节 type-122
缓冲区，只读取偏移 `+1` 的 DWORD，并与 `GetLocalTime` 得到的 `YYYYMMDD`
比较；`LOCALDAYNUM` 请求 type 168，只读取返回缓冲区首 DWORD。两项随后都把
同一个数值写入整个输出序列。

新增 `output/ida_probe_tcalc_host_callback_global.py`，结果保存为
`output/ida-tcalc-host-callback-global.log`，锁定 TCalc 宿主回调全局地址为
`0x10310BF0`（模块 RVA `0x310BF0`）。

## TdxW type 122/168 与本地文件

TdxW type 122 会清零 96 字节结果，并把全局当前交易日写入偏移 `+1`；
`ISJYDATE` 不读取同一结构中的其他界面状态字段。type 168 调用
`sub_529C70`，将其记录数写入首 DWORD；沪深北证券的本地最后日期小于全局
当前交易日时，将记录数加 1。

新增 `output/ida_probe_tdxw_localdaynum.py`；IDA 输出保存在
`output/ida-tdxw-localdaynum-run.log`。`sub_529C70` 的精确文件规则为：

- 沪深北：`vipdoc/<sz|sh|bj>/lday/<前缀><代码>.day`；
- 扩展市场：`vipdoc/ds/lday/<市场号>#<代码>.day`；
- 返回文件长度右移 5 位，即除以 32；读取最后一条 32 字节记录首 DWORD 作为
  本地最后日期。

当前安装中的 `sz000001.day` 为 268352 字节、8386 条记录，最后日期
`20260610`。真实日线请求的当前交易日为 `20260807`，因此 type-168 结果是
`8386 + 1 = 8387`。

## 32 位原 DLL 伪宿主固定向量

新增 `output/native_probe_tcalc_host_calendar.cpp` 和 32 位探针
`output/native_probe_tcalc_host_calendar.exe`。探针把伪回调写入 TCalc RVA
`0x310BF0`，分别注入 type-122 日期和 type-168 整数，再直接调用原处理函数。
结果保存为 `output/native-tcalc-host-calendar-probe.json`：

```text
机器日期 20260809：
ISJYDATE(type122=20260809) = [1,1,1,1]
ISJYDATE(type122!=20260809) = [0,0,0,0]
LOCALDAYNUM(type168=0)       = [0,0,0,0]
LOCALDAYNUM(type168=4321)    = [4321,4321,4321,4321]
LOCALDAYNUM(type168=8387)    = [8387,8387,8387,8387]
```

该向量排除了两项常见误读：`ISJYDATE` 不是任意日期交易日判断；
`LOCALDAYNUM` 不是从当前请求 K 线数量推导，而是直接广播宿主给出的本地日线计数。

## 纯 C++ 接入与 API

解释器新增
`custom_formula_host_calendar_functions=["ISJYDATE","LOCALDAYNUM"]`，支持函数
由 205 增至 207，自动符号仍为 54，TCalc 静态注册证据仍为 390。两项被归为
自动上下文依赖，同时支持 `ISJYDATE/LOCALDAYNUM` 裸符号和
`ISJYDATE()/LOCALDAYNUM()` 零参数调用。

自动上下文使用请求 K 线的最新日期作为 TdxW 全局当前交易日的显式代理，并返回：

- 当前交易日及其来源、机器日期；
- 本地 `.day` 文件路径、是否存在、原始记录数和最后日期；
- 是否按 TdxW 规则补当前交易日、最终有效计数和两项广播值。

平安银行真实 API 返回 `ISJYDATE=0`、`LOCALDAYNUM=8387`，并报告
`current=20260807`、`machine=20260809`、`records=8386`、`last=20260610`、
`synthetic_current=true`。既有 `formula-security-status-inline-post` 契约已扩展为
六个宿主依赖，按元数据动态核对日期比较与最终记录数，不把会随本地下载更新的
8387 写成永久业务常量。

## 验证与发布

- `tdx-formula-engine-tests`：裸符号、零参数调用、全柱广播及自动依赖通过；
- `tdx-recon-contract-tests`：能力和宿主元数据契约通过；
- CTest：101/101；
- 临时服务定向 API：3/3；
- 临时服务 full API：211/211；
- 正式服务定向 API：3/3。

正式 EXE SHA-256 为
`C1BB6803D85D75BC8193C1CDE1AA584FA66C15A604F0CF480078A179CA15B61C`；服务 PID
36908，仅监听 `127.0.0.1:8765`，健康检查为 `native_cpp=true`、
`python_runtime=false`。网页资源没有变化。

## 边界

`ISJYDATE` 复现的是“当前宿主交易日是否等于机器今天”，不能用于历史节假日表。
`LOCALDAYNUM` 描述本地 `.day` 缓存及当前交易日补位，不等于交易所从上市以来的
权威交易日总数；本地数据缺口不会被推测补齐。当前交易日来自最新请求 K 线，
响应明确标记这一代理来源，没有冒充直接读取 TdxW 进程全局变量。
