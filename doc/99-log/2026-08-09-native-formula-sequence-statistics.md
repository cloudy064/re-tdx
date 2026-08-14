# TCalc 时序搜索与统计核心

## 目标与边界

本轮继续从当前指纹的 `TCalc.dll` 静态注册表和 Hex-Rays 处理函数恢复高复用、
不依赖账号授权的公式语义，并直接实现到纯 C++ 源码解释器。官方函数说明用于
核对名称、参数顺序和未来函数属性；数值边界以 DLL 处理函数为准：
[通达信公式函数表](https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html)。

`output/ida-tcalc-formula-sequence-stats.log` 保存本轮反编译和注册窗口证据；生成脚本
为 `output/ida_probe_tcalc_formula_sequence_stats.py`，只用于离线 IDA 自动化，不是
工具的运行时依赖。

## 注册与处理函数证据

| 函数 | opcode | 处理函数 | 结论 |
| --- | ---: | --- | --- |
| `FINDHIGH` | 1025 | `sub_10017CD0` | 偏移窗口内第 T 个高值 |
| `FINDHIGHBARS` | 1026 | `sub_10017DF0` | 对应高值距当前柱数 |
| `FINDLOW` | 1027 | `sub_10017F30` | 偏移窗口内第 T 个低值 |
| `FINDLOWBARS` | 1028 | `sub_10018050` | 对应低值距当前柱数 |
| `FILTERX` | 1085 | `sub_10007A10` | 从后向前保留信号并清除其前 N 柱 |
| `BARSSINCE` | 1126 | `sub_10007060` | 首次成立以来的周期数 |
| `BARSSINCEN` | 1127 | `sub_10006EA0` | 最近 N 柱内最早成立点距当前柱数 |
| `TMA` | 1131 | `sub_10007C10` | `Y=A*Y'+B*X` 递推 |
| `XMA` | 1140 | `sub_1000B370` | 居中移动平均，未来函数 |
| `COVAR` | 1157 | `sub_1000E050` | 样本协方差 |
| `RELATE` | 1158 | `sub_1001ECF0` | 相关系数 |
| `BETAEX` | 1160 | `sub_1000E330` | 两序列协方差/第二序列方差 |
| `BARSLASTS` | 1360 | `sub_10006AF0` | 倒数第 N 次成立距当前柱数 |

注册名源数据同时确认 `sub_1000E330` 对应 `BETAEX`，不是相邻的 `BETA`。
`BETA` 为 opcode 1159、处理函数 `sub_10038980`；它会自动构造当前证券与对应市场
指数的收益率序列，仍依赖宿主选择基准指数，因此本轮没有用 `BETAEX` 冒充它。

## 已实现语义

- `BARSLASTS(X,N)` 从当前柱向前计数第 N 次成立。原版在不足 N 次但至少存在一次
  成立时返回最早可见成立点的距离；完全不存在时保持缺失。
- `BARSSINCE(X)` 首次成立前保持缺失，成立柱为 0，之后逐柱递增。此前解释器把
  首次成立前错误写成 0，本轮一并修正。
- `BARSSINCEN(X,N)` 在滚动 N 柱中选择最早成立点；窗口内无成立点时为缺失。
- `FILTERX(X,N)` 从序列尾部反向处理，保留较晚信号并把其前 N 柱清零。它与
  `XMA` 都进入未来函数清单，只能显式用于只读绘图，继续禁止扫描和回测。
- `TMA(X,A,B)` 以首个有效 `X` 为种子，随后执行 `Y=A*Y'+B*X`。
- `XMA(X,N)` 奇数窗口为 `[-N/2,+N/2]`，偶数窗口为
  `[-N/2,+N/2-1]`；序列边缘只平均现有有效点。
- `COVAR(X,Y,N)` 使用 `N-1` 分母；`N=1` 返回 0，历史不足时缺失。
- `RELATE(X,Y,N)` 使用协方差除以两序列总体方差乘积的平方根；退化窗口沿用前值。
- `BETAEX(X,Y,N)` 使用协方差和除以 `Y` 的方差和；首个退化窗口回退 0。
- 四个 `FIND*` 函数按 `N` 柱偏移、`M` 柱窗口、`T` 名次选择高低值，并可返回
  所选柱到当前柱的距离。

## 接入与固定契约

`native/src/formula_engine.cpp` 新增 12 个函数，解释器支持函数数由 145 增至
157。能力清单新增：

- `custom_formula_sequence_statistics_function_count=12`；
- `custom_formula_sequence_statistics_functions` 精确函数数组。

Svelte 公式库显示“时序/统计核心”计数，不在浏览器中复制计算逻辑。新增
`formula-sequence-statistics-inline-post` 固定契约，在 120 根真实日线载体上用
`TOTALBARSCOUNT-CURRBARSCOUNT+1` 构造确定性序列，锁定：

- `BARSLASTS=3`、`BARSSINCEN=3`；
- 较晚信号保留、前两柱被 `FILTERX` 清零；
- 末端 `XMA=119.5`；
- `COVAR=5`、`RELATE=1`、`BETAEX=0.5`；
- `FINDHIGH/FINDLOW=118/116`，对应柱距为 `2/4`。

## 验证与发布

- 公式引擎单测和契约求值器单测通过；
- CTest：101/101；
- Svelte：0 错误、0 警告，生产构建成功；
- 临时服务专项：4/4；
- 临时服务 full：204/204，205 次网络请求；
- 正式服务专项：4/4。

正式 EXE SHA-256 为
`623863F1A88B6E96950B10031DF204F1B9CB819DE52A50A80EC9EFD5E955177E`；服务 PID
40300，仅监听 `127.0.0.1:8765`。健康检查保持 `native_cpp=true`、
`python_runtime=false`、379 条公式。

## 后续

`BETA(N)` 只有在恢复“证券到对应市场基准指数”的宿主映射并加入跨证券历史
对齐后才能准确实现。下一批仍按静态注册、处理函数、公开数据来源和真实调用形态
四项证据共同筛选，不根据函数名猜测行为。
