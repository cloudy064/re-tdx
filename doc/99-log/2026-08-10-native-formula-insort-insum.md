# TCalc INSORT/INSUM 指标横向聚合闭环

## 结果与边界

纯 C++ 公式解释器新增以下两条日线自动上下文：

- `INSORT('板块名','指标名',输出线序号,排序方向)`；
- `INSUM('板块名','指标名',输出线序号,计算方式)`。

它们不是把板块成员的收盘价直接排名或汇总，而是先从恢复的 TCalc 技术指标目录
选择指标，按一基序号选择输出线，再对每个成员执行同一指标。`HY./GN./MY.` 和
无前缀板块查找复用 `BLOCKSETNUM/HORCALC` 已确认的本地目录与成员顺序。

当前自动路径只接受未复权日线，且被调用指标必须是可执行、数值安全、仅依赖
OHLCV 的技术指标。每只证券会额外加载目标区间前 100 根日线作为预热，同一
指标/输出/证券只求值一次。分钟、周线、月线、动态字符串、越界参数、依赖其他
外部上下文的指标，以及 `CLOUDGPS/HYINSORT/GNINSORT./FGINSORT./MYGNBK/MYFGBK`
等 L2 或云专用集合均明确拒绝；没有登录、L2 绕过或插件回调。

[通达信官方函数表](https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html)
给出四个参数及枚举：`INSORT` 的 0/1 为降序/升序，`INSUM` 的 0..5 为求和、
平均、最大、最小、最大成员序号、最小成员序号。

## 原 DLL 精确语义

静态注册表将 `INSORT/INSUM` 映射到 opcode 1246/1247，对应处理函数
`TCalc.dll!sub_10041E80/sub_100431A0`。二者从活动公式目录的 5072 字节记录中
按指标名选择技术公式，检查输出数量，再通过宿主取得板块成员和 K 线并调用嵌套
求值器；直接递归到当前活动公式会被拒绝。

32 位原生探针替换活动目录、板块、K 线和嵌套求值回调后确认：

1. `INSORT` 的 0 为降序、1 为升序，名次从 1 开始；
2. `INSORT` 在成员首个有效点之后向前沿用最近值，首个有效点之前不参与；
3. 名次比较使用固定 `1e-5` 容差，容差内视为并列；
4. `INSUM` 不沿用成员缺失日期，只使用当日有值的成员；
5. 平均值仍除以板块全部成员数，而不是当日有效成员数；
6. 最大/最小成员序号是原始板块成员顺序的一基序号，更新同样使用 `1e-5`
   容差。

固定三成员、五日期向量中，`INSORT` 降序为 `[1,1,1,2,1]`、升序为
`[2,3,3,2,3]`；`INSUM` 求和为 `[15,35,55,120,50]`，平均为
`[5,11.666667,18.333334,40,16.666666]`，最大/最小序号分别为
`[1,1,1,2,1]` 和 `[2,3,2,3,1]`。探针实际触发板块回调 11 次、K 线与嵌套
求值回调各 33 次。

核心证据及 SHA-256：

- `output/ida-tcalc-horizontal-aggregates-handlers.json`：
  `71D90386347AB26425A65165F7C9717704549B437E32A7BF43F34E4DD5C771C2`；
- `output/native_probe_tcalc_insort_insum.cpp`：
  `D9AFFB0D28FF1C817E3C2734486B9865CA60C582E507E5311A73FB45C72B49C2`；
- `output/native_probe_tcalc_insort_insum.exe`：
  `11525B051974E30D5F5F8BD3A2DD0AEB2AEEAFB336AEAAB838FA2B20DA33C189`；
- `output/native-tcalc-insort-insum-probe-v1.json`：
  `CD0C111607A09852F60541349D3AC6695C409E00A9CB44B3782A8CEF9EB4242B`；
- `C:\new_tdx\TCalc.dll`：
  `13FACAA52DAC552C5BE1F63331781219DE9BF798C443AF8F5E4AFC193A02E7F5`。

## 纯 C++ 实现与真实数据

静态分析器只为四个常量参数生成
`INSORT/INSUM#<block>#<formula>#<output>#<mode>` 精确序列键；运行时从服务启动时
已恢复的公式库选择技术指标，避免每个请求再次解析 DLL。跨绑定缓存键包含指标、
输出、市场和证券，因此本次 8 个 KDJ.J 绑定在 42 个银行成员上只执行 42 次，
而不是 336 次。

真实 `HY.银行` 样本解析到研究行业 `881385`，42/42 个成员有本地 `.day`。
2026-06-10 是本机成员日线共同覆盖日，平安银行 KDJ.J 降序/升序排名为 `6/37`，
总和 `3371.558349609375`、平均 `80.27519989013672`，最大/最小值为
`108.33905792236328/11.644400596618652`，对应成员序号 `18/20`。

本机其他成员日线停在 6 月，而公开目标 K 线已到 8 月。因此 2026-08-07 的
`INSUM` 只包含目标证券，平均仍为目标值除以 42；`INSORT` 则继续使用其他成员的
最近值。这不是网络缺数补值，而是两条原生函数不同的日期语义。真实结果文件
`output/native-indicator-aggregate-bank-v1.json` 的 SHA-256 为
`642804B5F2032B98BC050DF7C880E7E9A01B01148D3DB6AF926A1642F95803C1`。

## 覆盖、验证与部署

支持函数由 228 增至 230，自动符号仍为 69；390 条静态注册名的识别并集由
280 增至 282，剩余 108。379 条内置公式保持源码、语法和数值安全 `379/379`，
降级数值输出为 0。覆盖报告
`output/native-formula-coverage-indicator-aggregates-v9.json` 的 SHA-256 为
`CD80F0C67FE3F22A8B9E2822799AB3220C5D9991C1940BB8F0BAA16AA6D8D97D`；下一项审计
`output/native-formula-registry-next-audit-v9.json` 为
`C32A415A9846B2A80C11C7B3A21A972D531F43861D51DF2296414EC065C7E088`。

验证结果：

- CTest `102/102`；
- 候选专项契约 `3/3`；
- 候选 full API `219/219`；
- 正式 8765 专项契约 `3/3`。

候选专项、候选 full、正式专项报告的 SHA-256 分别为
`56D4D4FD8B0660CA424C0A13B5585FEE47E3E7BF3D862BAD42EBD9FAC6674DAD`、
`B6607647DE1C0DF5A1E375E046F9E0A865812D407B4CC16A9ADDEAE4411E94BF`、
`3B34C03AB8292AECC35CECB95468679651B512E1433407681ADE48B984D7FE3F`。

正式 build/dist EXE 均为 21,997,838 字节，SHA-256
`FA2ADB4A5A248B818BC8E09722E9B4979D8FBE108CEB585EF961A6A22C75C211`。服务 PID
37848，仅监听 `127.0.0.1:8765`，健康状态保持 `native_cpp=true`、
`python_runtime=false`。上一正式版保存在
`output/tdx-tool-indicator-aggregate-v9-predeploy-rollback-20260810.exe`，大小
21,905,954 字节，SHA-256
`A9E3ADC1140178115B4922499EF953AFF87B9BFDC636467D0C19016D058625DA`；候选 8875
已关闭。

下一批普通非 L2 候选转为 `ZHBLOCK/SIMIBLOCK`；前者仍缺运行时组合板块提供者，
后者仍缺活动相似证券目录与精确显示顺序。`EXTERNVALUE/EXTERNSTR` 也有价值，但
要先恢复外部文件命名空间、类型转换、缺失哨兵与刷新归属，不能用自造键值替代。
