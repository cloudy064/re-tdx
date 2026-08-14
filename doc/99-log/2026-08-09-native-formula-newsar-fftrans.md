# TCalc NEWSAR / FFTRANS 原生变换闭环

## 目标与结论

上一轮已确认 `NEWSAR/FFTRANS` 均存在于 TCalc 的 390 条静态注册表，但两者分别
依赖原生 K 线结构和一组复数蝶形辅助函数，不能按名称猜测实现。本轮完成处理函数、
一级辅助链和 32 位原 DLL 固定向量对照，并将两项直接移植到纯 C++ 解释器：

- `NEWSAR(N,S)` 是使用 OHLC、收盘反转和 `S/1000` 加速步长的新 SAR；
- `FFTRANS(X,N)` 是原生分段复数变换。2 次幂段与离散 Hartley 变换一致，
  非 2 次幂段则必须保留原 DLL 的不完整基 2 蝶形；
- `FFTRANS` 在每个分段内读取未来点，已纳入未来函数隔离，只允许显式只读
  执行/绘图，扫描和回测继续拒绝；`NEWSAR` 是普通历史指标。

运行时没有加载 `TCalc.dll`，也没有 Python 转发。Python 仅用于离线驱动 IDA
生成反编译证据。

## 注册与处理函数证据

当前 DLL 指纹下的静态注册记录为：

| 名称 | opcode | 处理函数 | 签名 |
| --- | ---: | --- | --- |
| `NEWSAR` | 1196 | `sub_1002C950` / `0x1002C950` | `NewSAR(N,S)` |
| `FFTRANS` | 1391 | `sub_1000DC80` / `0x1000DC80` | `FFTRANS(X,N)` |

`output/ida-tcalc-next-unlicensed.log` 保存两项处理函数反编译。新增
`output/ida_probe_tcalc_fftrans_helpers.py`，输出
`output/ida-tcalc-fftrans-helpers.log`，锁定 `FFTRANS` 的四个关键辅助入口：

- `sub_1000D910`：使用 `ceil(log2(N))` 位做原生位反转；
- `sub_1000D9F0/sub_1000DA10/sub_1000DA30`：复数加、减、乘；
- `sub_1000DA60`：基 2 蝶形；当 N 不是 2 次幂时不会补零或转入通用 DFT。

处理函数按不超过 1024 点顺序分段，将每个 X 同时写入复数实部和虚部，旋转因子
为 `cos(theta)-i*sin(theta)`，最终只返回实部。N 按 `int(N+0.503)` 取整；有效
范围为 1..1024，最后不足 N 的段按剩余长度缩短。

## 32 位原 DLL 固定向量

新增直调探针 `output/native_probe_tcalc_newsar_fftrans.cpp`，其结果保存为
`output/native-tcalc-newsar-fftrans-probe.json`。

`FFTRANS` 的关键固定向量为：

```text
FFTRANS([1,2,3,4],1) = [1,2,3,4]
FFTRANS([1,2,3,4],2) = [3,-1,7,-1]
FFTRANS([1,2,3,4],4) = [10,-4,-2,约0]
FFTRANS([1,2,3,4,5],3) = [6,-2,2,9,-1]
动态 N=[2,99,3,99,99] = [3,-1,12,-2,4]
```

N=0 或 N=1025 全部缺失；输入前导缺失后，从首个有效点开始分段。非 2 次幂和
动态分段结果证明不能用标准 FFT/DFT 替代原生辅助链。

`NEWSAR` 使用收盘序列
`[10,11,12,13,12,11,10,9,10,11,12,11,10,9,10,11]`，每柱
`HIGH=CLOSE+1`、`LOW=CLOSE-1`。`NEWSAR(3,2)` 的有效输出为：

```text
[9,9.01000023,9.01798058,9.02394485,9.02789688,13,
 12.9919996,12.9860153,12.9820433,12.9701147,12.9462938,
 12.906723,12.8754692,12.8524656]
```

前两柱缺失。向量同时确认：S 要除以 1000；反转由收盘越过投影值触发；反转时
取前 N 柱高/低极值重置；连续创新极值才递增加速；接口没有最大加速参数。
N<1 或 N>总柱数全部缺失。

## 纯 C++ 接入和安全边界

解释器新增 `custom_formula_transform_functions=["FFTRANS","NEWSAR"]`，支持函数
由 203 增至 205，自动符号仍为 54，TCalc 静态注册证据仍为 390。`NEWSAR`
直接使用当前 K 线的 OHLC；`FFTRANS` 复刻 1024 项复数工作区、位反转、旋转因子、
基 2 蝶形、非 2 次幂和动态尾段语义。

`FFTRANS` 被加入 `future_functions`。源码分析会返回
`has_future_function=true/read_only_future_executable=true`；只有调用方显式设置
`allow_future=true` 才可求值，扫描和回测无条件拒绝。这样不会把分段内偷看未来
样本的结果误用于策略历史表现。

真实 API 契约扩展既有 `formula-sequence-statistics-inline-post`，在平安银行 120 根
日线上验证 `FFTRANS(SEQ,4)` 最后四点为 `[474,-4,-2,约0]`，并验证
`NEWSAR(10,2)` 末值为有限数。能力契约同时严格核对 205、变换分组 2 项和名称集合。

## 验证与发布

- `tdx-formula-engine-tests`：原 DLL NEWSAR 向量、FFTRANS 2 次幂、非 2 次幂、
  动态分段和未来函数分析全部通过；
- CTest：101/101；
- 临时服务定向 API：3/3；
- 临时服务 full API：211/211；
- 正式服务定向 API：3/3。

正式 EXE SHA-256 为
`423DFA0F60B42EBD2F6B7F14AF9A0D398B6ED7BEB236A00ADDB343F0AA71CDF9`；服务 PID
29504，仅监听 `127.0.0.1:8765`，健康检查为 `native_cpp=true`、
`python_runtime=false`。网页资源没有变化。
