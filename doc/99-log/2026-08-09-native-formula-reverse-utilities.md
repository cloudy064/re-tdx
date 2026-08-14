# TCalc 纯序列反向工具

## 目标与筛选

在 390 条静态注册证据中继续筛选不依赖 L2、交易账户、券商会话或插件 DLL 的
入口。本轮先统一反编译 13 个候选，证据脚本为
`output/ida_probe_tcalc_next_unlicensed.py`，完整日志为
`output/ida-tcalc-next-unlicensed.log`。

筛选后选择 `CONSTA/ROUND2/EXISTR`：它们完全由调用方序列计算，且与现有
`CONST/ROUND/EXIST` 名称相近但语义不同。其余候选没有被冒充实现：

- `LFS` 依赖 type-103 宿主回调；`LOCALDAYNUM/ISJYDATE` 分别依赖 type-168/122；
- `RAND` 每次处理函数调用都用当前秒重置 CRT 随机种子，不适合确定性扫描回测；
- `NEWSAR` 直接访问 35 字节原生 K 线结构，`FFTRANS` 包含复数 FFT 辅助链，后续
  需要独立向量和更完整边界；
- `MACHINEDATE/MACHINETIME/MACHINEWEEK` 可从系统时钟恢复，但不是历史 K 线
  时点语义，本轮不把不稳定常量并入扫描和回测。

## 处理函数证据

- `CONSTA`：opcode 1385，`sub_1000F360`。参数 N 只读取末柱，C++ 截断后钳制
  到 `0..count-1`，读取 `X[count-N-1]` 并广播；
- `ROUND2`：opcode 1319，`sub_10038870`。精度只读取参数首柱并钳制到 0..4；
  正数先加、负数先减 `0.503000020980835`，再分别 `floor/ceil`；
- `EXISTR`：opcode 1337，`sub_10005590`。N/M 读取末柱；对每个有效当前柱扫描
  `[i-N,i-M]`，N=0 时左界固定为首个有效柱。当前条件缺失时跳过输出，但窗口
  内部缺失哨兵因原生数值比较被判真。

为消除反编译变量恢复歧义，新增 32 位直调
`output/native_probe_tcalc_next_unlicensed.cpp`，输出
`output/native-tcalc-next-unlicensed-probe.json`。固定向量确认：

- `CONSTA([10,20,30,40,50,60],2)` 全部为 40，负数为末柱，超界为首柱；
- `ROUND2(1.235,2)=1.24`、`ROUND2(-1.235,2)=-1.24`，精度 5 按 4 处理；
- `EXISTR` 的累计、1..0、3..1、4..2 窗口均与处理函数一致，内部缺失而当前
  有效的固定样本保持 1。

## 纯 C++ 实现与验证

三项直接进入解释器，没有加载 TCalc.dll，也没有 Python 转发；能力清单的
`custom_formula_core_functions` 由 11 增至 14，支持函数由 200 增至 203，自动
符号仍为 54，静态注册证据仍为 390。

单测覆盖参数取值端、偏移钳制、正负舍入、累计/滚动窗口和缺失哨兵；平安银行
120 根真实日线的 `formula-custom-core-inline-post` 同时对账
`CONSTA(CLOSE,2)`、`ROUND2(1.235,2)` 与 `EXISTR`。CTest 101/101、临时服务
full API 211/211（212 次网络请求）、正式服务专项 3/3 均通过。

正式 EXE SHA-256 为
`6270712056141D0C1B011CC66EAEAD51E711B77CEEC7BEB2054AC2DC5E9BBD49`；服务 PID
18712，仅监听 `127.0.0.1:8765`，健康检查为 `native_cpp=true`、
`python_runtime=false`。网页资源没有变化。
