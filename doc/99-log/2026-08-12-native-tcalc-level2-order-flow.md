# TCalc Level2 type-31 订单流与公式上下文

日期：2026-08-12

## 结果

新生成的 `ida/TCalc.dll.i64` 使 L2 订单流函数从“已分类权限边界”推进为
“解释器可执行、数据仍由调用方授权来源提供”。纯 C++ 工具不登录、不读取票据、
不从 L1 推导 L2；它新增：

- `level2 decode --format tcalc-order-flow`，解析宿主回调 type-31 的 184 字节记录；
- 每条记录输出 53 个可直接对应公式上下文的绑定值；
- 解释器支持全部已恢复 L2 注册名的裸符号或函数形式；
- `formulas context-template` 为这些调用生成精确的空时间序列键。
- `tcalc-order-side` 解析 type-104 的 104 字节体，并把 byte 46 映射为
  `ISBUYORDER=(byte46==0)` 标量上下文。

## 原始处理函数

静态注册表映射及 handler 如下：

| 名称 | opcode | handler | type-31 偏移/语义 |
| --- | ---: | --- | --- |
| `L2_VOLNUM(A,B)` | 1222 | `0x10036D40` | `136 + 4*(B+2*A)`，A/B 为 0..1，按原生 `int(x+0.50300002)` |
| `L2_VOL(A,B)` | 1223 | `0x10036E30` | `8 + 4*(B+4*A)`，A/B 为 0..3 |
| `L2_AMO(A,B)` | 1224 | `0x10036EE0` | `72 + 4*(B+4*A)`，A/B 为 0..3 |
| `ACTINVOL/ACTOUTVOL` | 1227/1228 | `0x10036F90/0x10037030` | 四个量级的方向 2/3 求和 |
| `LARGEINTRDVOL/LARGEOUTTRDVOL` | 1230/1231 | `0x100370D0/0x10037160` | 量级 0+1、方向 0/1 求和 |
| `BIDORDERVOL/BIDCANCELVOL` | 1232/1233 | `0x100371F0/0x10037270` | `+152/+156` |
| `AVGBIDPX` | 1235 | `0x100372F0` | `+168` |
| `OFFERORDERVOL/OFFERCANCELVOL` | 1236/1237 | `0x10037370/0x100373F0` | `+160/+164` |
| `AVGOFFERPX` | 1239 | `0x10037470` | `+172` |
| `CUR_BUYORDER/CUR_SELLORDER` | 1225/1226 | `0x100374F0/0x10037570` | `+176/+180` |
| `TRADENUM` | 1216 | `0x100369F0` | `+4`，按原生 `int(x+0.50300002)` |
| `TRADEINNUM/TRADEOUTNUM` | 1217/1218 | `0x10036AA0/0x10036B50` | `+136 + +144` / `+140 + +148` 后原生取整 |
| `LARGETRDINNUM/LARGETRDOUTNUM` | 1220/1221 | `0x10036C00/0x10036CA0` | `+136/+140` 后原生取整 |

`sub_100353B0` 先通过 TCalc 宿主包装 `sub_1000FD60(type=31)` 取得记录，再按
K 线日期合并、继承并裁剪。记录头 `+0` 是日期键；`+4` 当前处理函数只在合并时
保留，工具命名为 `host_auxiliary_f32`，不猜业务含义。

`ISBUYORDER` 不属于 type-31：`sub_1002AA70` 调用
`sub_1000FD60(type=104)`，读取 104 字节体的 byte 46，并以 `byte46==0` 输出 1。
因此 type-31 解码结果明确报告它不可用；type-104 解码器则输出
`formula_scalar_bindings.ISBUYORDER`，解释器会把该标量复制到目标 K 线全部柱。

TCalc 使用位模式 `0xf8f8f8f8` 表示缺失值。它虽然是有限浮点数，却不能进入
计算。解码器现在按位识别并输出 `null`，四个派生量与五个成交笔数字段也严格沿用
原 handler 的 anchor guard，避免约 `-4.04e34` 的假值污染公式结果。

type-31 解码文档可直接作为 `formulas evaluate --context-file`。执行前的纯 C++
materializer 会按目标日、周或月 K 的实际 `date|time` 合并，而不是依赖固定
`15:00`。日线保持精确日期、前段裁剪和尾部继承；周/月复现 `sub_100353B0`
对日 type-31 记录的原生区间合成：

- 第一根目标柱之前的记录裁掉，避免窗口外的前导残段进入首柱；
- `+4`、两块 4×4 矩阵及 `+136..+164` 求和，`+168/+172/+176/+180`
  取区间最后一条日记录；
- `+156/+164` 除每日撤单量外，分别累计前一日的
  `CUR_BUYORDER/CUR_SELLORDER`；
- 最后一日平均买价不大于 `1e-5` 时回退目标 K 的 `low`，平均卖价不大于
  `1e-5` 时回退 `high`；
- 目标柱必须有日期完全相同的末日记录才能闭合；缺失时丢弃残段并输出缺失值，
  type-31 耗尽后的目标柱继承上一根输出柱。

聚合从解码 JSON 保留的 raw matrix、`host_auxiliary_f32`、raw count 和八个末端
scalar 重建，完成后再生成 53 个公式绑定，不会把已原生取整的计数字段二次相加。
求和中的 `null` 会安全传播，平均价的 sentinel/null 按原函数走低/高价回退，
不会产生约 `-4.04e34` 的假值。materializer 仍拒绝被 `--limit` 截断的上下文、
无效/重复日期及分钟周期。`formulas audit --context-file` 与 evaluate 现在共用同一物化入口，
不会因目标日 K 的收盘时间不是 `15:00` 而误报缺上下文。完整物化后即使某项在整个
窗口都为原生 missing/null，也按合法缺失序列执行；这个例外严格要求 type-31 schema、
完整标志、物化标志、柱数和每个时间戳覆盖一致，普通全 null 模板仍会被拒绝。

网页 Level2 实验室可把 type-31 或 type-104 解码结果以内存/session handoff 送到公式
工作台。内置公式和自定义单票源码的 evaluate 都可携带该上下文；扫描与回测仍不会
隐式复用它，避免把单一合法捕获扩散到其他证券或时段。

## 边界

type-31 是 TCalc 已聚合的日序列，并不等同于现有 `1364/1374` 原始逐笔或
`tpbus 111/112` 盘口推送。缺少从逐笔生成日 type-31 的原生规则和完整样本时，
工具不会用公开成交或五档行情重算并冒充这些字段。`113/114/116` protobuf 的业务 schema 仍然
未知，本轮没有给字段号命名。

## 验证

- `tdx-level2-tests` 通过：固定 184 字节向量覆盖矩阵、派生量、五个计数字段、
  `0xf8f8f8f8` 缺失传播、K 线物化门禁和 type-104 边界；
- 独立 `tdx-level2-formula-context-tests` 覆盖跨周/跨月 raw 求和、末值、撤单前日
  carry、低/高价回退、首段裁剪、缺精确末日、尾部继承与 null 安全传播；
- `tdx-formula-engine-tests` 通过：常量 selector、越界拒绝、裸符号、零参数调用
  及显式授权序列均进入解释器；
- 真实安装目录公式分析返回 380 条公式、277 个支持函数、87 个自动符号；
- CLI 合约确认 `tcalc-order-flow` schema、184 字节记录和 53 个公式绑定键；
- 解释器端到端合约在 2026-08-12 日 K 上得到 `TRADENUM=12`、
  `TRADEINNUM=5`、`LARGETRDOUTNUM=2`，type-104 得到 `ISBUYORDER=1`；
- `1364` 请求仍为
  `000000000000100010005405010036303030303044332211dc05`。

证据文件：

- `output/ida_probe_tcalc_level2_formula_handlers_20260812.py`
- `output/ida-tcalc-level2-formula-handlers-v2-20260812.log`
- `output/ida-tcalc-level2-count-handlers-20260812.log`
- `output/verify-tcalc-order-flow-cli-20260812.json`
- `output/verify-tcalc-order-flow-context-20260812.json`
- `output/verify-tcalc-order-flow-evaluate-20260812.json`
- `output/verify-tcalc-order-side-evaluate-20260812.json`

正式 8765 服务未重启、未替换。
