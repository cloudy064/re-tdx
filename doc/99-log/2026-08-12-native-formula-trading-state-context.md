# TCalc 交易状态显式上下文

日期：2026-08-12

## 结果

TCalc 静态注册表中的 38 个交易相关名称已重新按“只读状态”和“动作”分界。
其中 34 个零参数状态现可由调用方通过 `formula_scalar_bindings` 或精确
`series[NAME][DATE|TIME]` 注入纯 C++ 解释器；工具不登录账户、不读取交易会话，
也不会从行情推导或伪造这些值。

可注入的状态包括资金、持仓、成本、盈亏、费率、保证金、当日买卖，以及
`ISLAST*`、`LASTSIGNAL` 和买卖价格/距今柱数。`ORDERBUY`、`ORDERSELL`、
`CLOSEALLD`、`CLOSEALLK` 是会改变交易状态的动作，继续保持不可执行，也不能
用显式标量把它们伪装成只读状态。

公式分析会把这 34 个名称报告为 `explicit_context_bindable`；上下文模板将来源标成
`caller-account-strategy-state`。能力文档同时保留 38 个原始注册名，并新增
`live_trading_context_capable_name_count=34`，避免把“已识别”误写成“可自动获取”。
网页的自定义单票公式区提供可选 JSON 编辑器，因此用户可以显式提供自己的状态；
扫描和回测没有接入该上下文，也不会隐式共享账户数据。

## 静态证据

- `output/ida-tcalc-trading-state-handlers-20260812.log`：恢复交易状态字符串分派。
- `output/ida-tcalc-trading-state-runtime-20260812.log`：账户/组合状态先调用宿主回调
  type 91 解析身份，再以 type 90 获取状态记录并广播选定字段；`ISLAST*`、价格和
  柱数类读取解释器内部的委托事件历史。
- `output/ida-tcalc-trading-signal-runtime-20260812.log`：交易动作进入
  `sub_1001C0A0`；`CLOSEALLD/CLOSEALLK` 使用独立动作掩码，而不是数值状态。

静态证据只能证明字段来源和执行边界，不能证明某券商账户在某时刻的真实值。
最终业务对照仍需要用户在合法交易会话中自行导出的状态样本。

## 同批解释器修复

- `IF/IFF` 已按共享 TCalc handler 对齐：前导 missing 不启动输出；出现首个有效条件后，
  只有精确零选择假分支，其他值选择真分支。`IFF` 不再保留与 `IF` 不同的旧投影。
- `AND` 先检查精确零：任一操作数为 0 即为假；没有零时，任一 missing 传播 missing；
  两边都是有效非零值才为真。`OR` 则把 missing 当假，有限值转为 native float 后
  约以 `abs(value)>=1e-5` 判真。这些规则不会改写解释器其余 truth 消费点。
- 裸符号 `MTM` 对参数名改为 ASCII 大小写不敏感，`n=2` 与 `N=2` 使用同一
  两柱滞后，不再悄悄回退默认 12。
- `FROMOPEN` 按 TCalc 默认 A 股两段交易时段计算 1-based 分钟：午休期间夹紧在
  120，13:00 为 121，15:00 为 240。
- `USEDDATANUM` 按 `sub_1000E6A0` 在每根柱广播解释器本次使用的完整输入柱数；
  它不等于逐柱累计的 `BARSCOUNT`。
- `SETCODE` 广播当前证券的 TCalc 市场 ID：`sz/sh/bj` 与 `0/1/2` 分别保持
  `0/1/2`，兼容别名 44 映射为北交所 2，扩展市场保留其原始数值 ID（例如 47）。
- `TDXZXNH` 复用同一市场 ID 归一化；扩展市场别名 `qz/qd/qs/cz/qg` 分别映射
  TCalc ID `28/29/30/47/66`，与数字 ID 产生相同序列，不再因别名误入另一价格分支。
- 本地 360 字节 TNF 证券记录的 `+76` 在 `0..8` 范围内作为缓存
  `price_precision`，注入 K 线文档时来源为 `local-tnf-security-master`，并由
  `MINDIFF` 换算精度；314 字节旧格式没有相同字段证据，保持缺失，不读取该偏移。
调用方显式 `min_tick` 或 `price_precision` 具有绝对优先级；目录补充只使用内存
SecurityCatalog，不执行网络查询。

## 交易信号只读事件 IR

公式求值文档新增 additive `tdx-formula-trade-event-ir-v1`，既有 `points` 数值结果
保持兼容。IR 仅识别顶层调用语句，且 `BUY/SELL/SELLSHORT/BUYSHORT/
BUYSHORT_BUY/SELL_SELLSHORT` 都严格要求 condition、price 两个参数；嵌套调用或其他
名称不会被提升成交易事件。

condition 或 price 为 native missing 时，condition 会清为 0，price copy 仍保留 null。
逐柱 truth 命中形成的 `historical_signal_candidates` 明确是 `offline-per-bar`、
`native_host_action=false`；原生宿主动作只检查最后一柱，且必须满足
`abs(condition-1)<1e-5` 才生成只读 `latest_host_action`。动作位均以十六进制记录：
BUY/SELL/SELLSHORT/BUYSHORT 分别为 `0x1/0x10/0x100/0x1000`；
BUYSHORT_BUY 的 wrapper 为 `0x10000`、host bits 为 `0x1001`；SELL_SELLSHORT 的
wrapper 为 `0x100000`、host bits 为 `0x110`。整个文档固定
`execution_side_effects/order_submission/account_access/network_access=false`，不会
执行委托、读取账户或联网。

精确顶层符号语句 `AUTOFILTER` 会在上述 raw candidates 之外附加
`tdx-formula-autofilter-projection-v1`。该投影依据通达信文档中的连续同向只取首个、
开平信号配对规则，使用一个 `flat/long/short` position 状态；遍历次序固定为
bar 外层、source statement 内层。flat 可接受 BUY/SELLSHORT；long 只接受 SELL 或
原子 `SELL_SELLSHORT`；short 只接受 BUYSHORT 或原子 `BUYSHORT_BUY`；组合动作在
flat 时只采用其开仓半边。raw candidates 与 raw latest 始终保留，新增 filtered
candidates/latest 也全部是 read-only projection，不调用原生宿主交易动作。

这是依据公开文档规则实现的 v1，而不是对原客户端交易宿主状态机的动态重放；同一柱
内原生宿主是否存在另一层去重尚未验证。`CLOSEALLD/CLOSEALLK` 不在 v1 支持动作集，
仍保持不可执行。FormulaLibrary 已显示 raw 总数、AUTOFILTER 启用状态、接受/过滤数、
最终 position，并可逐 primitive 展开 raw、filtered 与 filtered latest；未启用时不会把
空 filtered 字段误显示为“全部被过滤”。页面固定提示这只是离线 IR、不访问账户、不下单。
本批进一步把明细改为真正的懒渲染：关闭的 primitive details 不会立即生成全量 pretty
JSON，展开后每类默认只序列化最近 100 条，并显示总数和省略数量；原始 IR 文档仍保留
在内存中，不增加全量下载或交易入口。

## REF 与基础运算符语义

`REF` 已按原处理函数改为 started gate。前导阶段只有 source 与 offset 同时有效才启动；
启动后当前 source missing 不阻碍它读取过去的 source。后续 offset missing、非正或左
边界越界都继承该 REF 节点上一输出；若尚无上一输出则保持 missing，不做首值平滑。

二元除法现在先把两侧收窄为 float 并优先传播任一 missing；两侧均有效时，分母落入约
`±1e-5` 近零区间会继承该除法节点上一输出，否则按 float 完成除法。`< <= > >=` 也
先收窄为 float，再采用左操作数锚定的 `abs(left)*1e-7+1e-5` 容差；`=` 与 `<>`
保持原语义。内建交叉指标 `KDJ.J` 的 RSV 分子/区间除法复用同一实现，避免与显式
公式源码产生不同的近零继承结果。
- `MINDIFF` 的正数 `formula_scalar_bindings.MINDIFF` 仍具有最高优先级；否则优先
  读取 K 线文档中的正数 `min_tick`，再由整数 `price_precision=0..8` 推导
  `10^-precision`，缺省为 0.01，并按原处理函数在 `1e-5` 处以单精度夹紧。

调用方现在可以在 K 线文档中显式传入 `trading_sessions`，每项只能包含严格
`HH:MM` 的 `start/end`。数组必须含 1–4 段、有序且不重叠，可表达一个跨午夜、
总跨度不超过 24 小时的交易周期；空数组、同起止、非法时间、重叠、超过四段或
超过一昼夜都会拒绝。显式会话同时驱动 `FROMOPEN` 的分段/休市夹紧与
`TOTALFZNUM` 的分钟总数，夜盘加三个日盘段的固定向量得到 555 分钟。

未提供 `trading_sessions` 时，仅 TCalc set-code 0/1/2（深沪京）使用默认 A 股
240 分钟会话。期货、期权、港美股等非 A 股不会再被猜成 A 股时段；其
`FROMOPEN/TOTALFZNUM` 安全传播为 missing，直到调用方提供明确会话或显式标量。

## 聚焦验证

- `tdx-formula-engine-tests --domain language` 通过，覆盖
  `USEDDATANUM/SETCODE/MINDIFF`、默认 A 股时段、跨午夜四段会话、非 A 股 missing
  以及 malformed/重叠/超量会话拒绝。
- `tdx-formula-engine-tests --domain context-and-library` 通过，覆盖调用方显式
  `TOTALFZNUM/MINDIFF` 标量的优先级。
- 第五批 formula language、context-and-library 与 formula strategy focused 全部通过，
  覆盖 IF/IFF、AND/OR、AUTOFILTER 单 position、statement 顺序、组合动作、latest
  guard 及七个系统专家模型的 marker 投影；Level2/server/catalog focused 同批通过。
- 第六批 formula language、context-and-library、strategy、Level2、server-level2 与
  catalog focused 全部通过；language 锁定 REF started gate、float 除法及左锚容差，
  context-and-library 锁定 `KDJ.J` 复用同一除法。
- 本次未重跑完整 CTest 或 full API 契约。聚焦 language 测试已包含五组
  `TDXZXNH` 别名/数字 ID 等价夹具。
- 包含这些语义的第二批产物曾 build/install/restart 到正式 8765，阶段 PID `11468`；EXE
  SHA-256 为
  `3881917c02000fad1dcc8d6e9d956e92022f910932a7c1aad2f0fc3176c0fb54`，部署网页
  `index.html` SHA-256 为
  `f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`。

上述 PID `11468` 是第二批部署阶段。第三批 blocks、formula language 与 Level2
focused 均通过，完整构建和 CTest 123/123 通过；未运行 full API。该阶段正式服务为
PID `17540`，EXE SHA-256
`007f61021734ea56ed9b7106bafbe46bd74e1a01467c539734faa8072f457645`，网页未变，
`index.html` SHA-256 仍为
`f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`。
运行时平安银行 local/day 的 5 根 K 线返回 `price_precision=2` 与
`price_precision_source=local-tnf-security-master`，证明缓存 TNF 元数据已进入
公式输入，且该路径没有网络补取。

PID `17540` 与上述 EXE hash 现只代表第三批历史阶段。第四批 formula language 与
formula strategy focused 均通过；完整构建通过（94.9 秒），CTest 123/123 通过
（50.30 秒）。正式运行时 plain formula 与 BUY IR 合约通过，错误参数数量返回 HTTP
400；连同 health、batch-plan CLI 共五项 focused 全部通过。没有运行 full API；网页
无变化，未重复 npm check/build。

第四批正式服务 PID `44960`，监听 `127.0.0.1:8765`，root 为 `C:\new_tdx`，并显式
include-user。EXE SHA-256 为
`ef22956254d57079c44f8d106979fb773e74a9612db39413fc7249f4d910d808`；部署网页
`index.html` SHA-256 仍为
`f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`。

第五批 `svelte-check` 为 0 errors / 0 warnings，生产构建成功且只有既有 chunk warning；
完整 C++ 构建成功，CTest 123/123。临时与正式 HTTP 样本均通过：平安银行 40 bars
产生 2 个 primitive，AUTOFILTER accepted 16、filtered 22，且
`execution_side_effects=false`；显式启用用户库的 380 条公式全库审计为 0 errors。

第五批阶段正式服务 PID `17908`，监听 `127.0.0.1:8765`；EXE SHA-256 为
`03bb4a68b87cbcda4d1a27534da3ba5914d9c49a1594533e3db496c7ba17af31`，部署网页
`index.html` SHA-256 为
`8ffc02ac014017478efabf576619732b58af123c123697dd03f48fa0773ff37b`。health 确认
`native_cpp=true`、`python_runtime=false`。PID `44960` 与对应哈希现仅为第四批历史，
PID `17908` 与对应哈希现仅为第五批历史。

第六批 `npm run check` 为 0 errors / 0 warnings，生产构建成功且只有既有 chunk warning；
完整 C++ 构建成功，CTest 123/123，用时 49.99 秒。临时与正式 HTTP 合约均通过：
`sdk-callback-invocation` 1804 使用精确 432 B body；平安银行 40 bars 使用上述原生运算；
全库审计为 `total=380, passed=228, errors=0, context=128, period=1, market=3`。

当前正式服务 PID `21416`，监听 `127.0.0.1:8765`；EXE SHA-256 为
`07b972da59da1cb959678c701ffb9be59eba023a2550bc533e4ee76557e7da6a`，部署网页
`index.html` SHA-256 为
`1f202c046bc8f3ff2c36de731027b72112b7c8c206a62b8d13df326b58107c9f`。health 确认
`native_cpp=true`、`python_runtime=false`。这些结果不代表真实下单、账户访问、SDK
回调执行、宿主消息投递或 Level2 授权绕过。

## MA/CROSS/EMA/EXPMA 与 float32 运算收口

- `MA` 从第一个有限源值建立窗口；周期 missing/零/负数以及跨越首个有效值的窗口均
  为 missing。窗口内后续 missing 跳过累加但仍除以固定 `N`，从当前柱向后按 float32
  累加并以 float32 保存商。
- `CROSS` 在两侧首次同时有效前保持 missing，首次有效柱只初始化；随后按
  `abs(left)*1e-7+1e-5` 容差维护严格下方状态，容差带内保持状态。启动后的 missing
  恢复为 TCalc 有限 sentinel 参与状态机，不重新进入前导门禁。
- `EMA/EXPMA` 共用 TCalc handler：首个有效源值作为 float32 seed，每柱动态周期转
  整数且至少夹到 1，周期变化不重新播种；后续源 missing 继承上一 EMA。
- 二元减法和乘法先收窄两个有限操作数为 float32，任一 missing 或收窄后非有限即
  输出 missing，最终差/积也保存为 float32。内建 `"KDJ.J"` 的 RSV 除法、`*100`
  和 `3*K-2*D` 已改用公共除、乘、减 helper，与等价显式公式一致。

## 本批验证与正式样本

相关 formula focused 全部通过；连同 Level2、server、catalog 与网页 focused，
`svelte-check` 为 0 errors / 0 warnings，生产网页构建成功且只有既有 chunk warning，
完整 C++ 构建成功，CTest 126/126，用时 99.91 秒。

正式平安银行 day 80 bars 全库审计为 `formula_count=380`、`reported=380`、
`eligible=335`、`passed=335`、`errors=0`、`context_unavailable=20`、
`future_disabled=20`、`market_inapplicable=4`、`period_inapplicable=1`、
`with_numeric=329`，无运行错误；另一个 40 bars 样本用于本批 runtime focused。

第七批正式 PID 曾为 `40020`，EXE SHA-256
`86fb3a4b674cbb60c817383e3b4f154b0b4482d133a1c03333510ba67c93ed04`，部署网页
`index.html` SHA-256
`b1dc8125928d3d6d0e1359b3ce532d4c0beb90211d82577f0cfc1ab8b2cce6ed`；health 确认
`native_cpp=true`、`python_runtime=false`。PID `21416` 与其哈希只保留为第六批历史。

## NOT 的 started/missing 原生语义

后续 TCalc 静态证据补全了早期有限输入探针没有覆盖的序列边界。`NOT` 使用独立的
started 状态：前导 missing 保持 missing；首个有限值之后，任何后续 missing/sentinel
都输出 `0`。有限输入先收窄为 float32，且只在精确等于零时输出 `1`，其他正负非零
值均输出 `0`。该规则只进入一元 `NOT` helper，不改变全局 truth 或一元负号。

379 条内建公式中有 10 条使用 `NOT`。focused 测试覆盖
`[missing,0,2,missing,-1] -> [missing,1,0,0,0]`、全 missing，以及 REF/显式内部缺口；
正式 POST 的同一 5 点序列也返回 `[null,1,0,0,0]`。

## 第八批验证与部署

本批只运行受影响的 formula `native-operators`、`language`、
`context-and-library`，均通过；同批独立 correlation target/test 通过，`tdx-tool`
目标构建成功，`svelte-check` 为 0 errors / 0 warnings，`npm run build` 成功且只有
既有 chunk warning。没有运行完整 CTest（当前注册 127 项）或 full API 契约。

当前正式 PID 为 `36624`，EXE SHA-256
`6b8b507a32b355a173bf95daac92afb77d73ffeaaf95275afa1d4180179d4499`，部署网页
`index.html` SHA-256
`ca685162fc620086b6caeb9653bceb909dfe8543a293cf00287dea55242ab42f`；health 为
`native_cpp=true`、`python_runtime=false`、`formulas=380`、`blocks=1159`。PID `40020`
及其 EXE/web 哈希现仅为第七批历史。本批结果仍不代表 Level2 授权绕过、SDK 加载、
真实 SDK callback 执行或宿主消息发送。

## SUM/HHV/LLV 滚动核心原生语义

2026-08-13 新取得的 TCalc 反编译证据保存在
`output/ida-tcalc-rolling-core-20260813.log`，其中直接覆盖 `SUM` 的
`sub_10007720`、`HHV` 的 `sub_10018590` 和 `LLV` 的 `sub_100194A0`。实现据此拆成
三个独立 handler：

- `SUM` 扫描首个有限源值，在此之前保持 missing；之后建立 float32 前缀，每次有效
  加法都立即收窄，源 missing 则继承前缀。随后从尾部反向检查每柱周期，只有完整且
  未越过首个有效源值的正周期窗口才覆盖前缀；窗口同样从当前向过去以 float32 累加，
  并跳过 missing。周期 missing、零、负数和历史不足均保留当柱前缀。
- `HHV/LLV` 从首个有限源值开始；动态周期本身 missing 时当柱保持 missing，有限周期
  小于 1 或超过当前历史时都夹到全部可用历史。每个候选先转 float32；比较阈值为
  `abs(candidate)*1e-7+1e-5` 量级，因此容差带内的后出现候选会替换先前候选。
- `HHV` 忽略任意位置的 missing，除非整个窗口都没有有限值；`LLV` 与之不对称：
  leading missing 可以跳过，已有选择后再遇 missing 会清空选择，后续有限值可重新
  建立选择，因而窗口尾为 missing 时本柱输出 missing。输出均保留 float32 精度。

focused 测试覆盖动态的负/零/超长/missing 周期、前导/内部/尾部/all-missing、容差带
以及 float32 累加顺序。formula `native-operators`、`language`、
`context-and-library` 均通过。正式 PID `4716` 上以 `sz/000001` 五日自定义
`NATIVEROLLING` 做临时 HTTP focused runtime，得到
`S=[5,5,4,7,6]`、`H=[5,5,5,4,4]`、`L=[5,null,4,3,3]`。

同批独立 1804 host projection target/test 通过，`svelte-check` 为
0 errors / 0 warnings，`npm run build` 成功且只有既有 chunk warning。本批未运行完整
CTest 或 full API 契约。正式 EXE SHA-256 为
`d44ddaa5c95398cbac5a860963401c02eb682a574e3823b62bf5a38291ef01d8`，部署网页
`index.html` SHA-256 为
`838bb4e58df85fd2ae4d46915142fe1a9a5fe0add2f3e2f8ba557344786f1420`；health 为
`native_cpp=true`、`python_runtime=false`、`formulas=380`、`blocks=1159`。
PID `36624` 与相应哈希现仅代表上一批历史部署。

## SMA、布尔窗口与位置函数的原生语义

同一份 fresh 静态证据 `output/ida-tcalc-rolling-core-20260813.log` 还记录了
`SMA` 的 `sub_10007AE0`、`COUNT` 的 `sub_10017650`、`EXIST` 的
`sub_100167C0`、`EVERY` 的 `sub_10005810`、`MAX` 的 `sub_1000B920` 与
`MIN` 的 `sub_1000BA70`。另一个新日志
`output/ida-tcalc-window-positions-20260813.log` 记录 `BARSLAST` 的
`sub_100069C0` 和 `BARSLASTCOUNT` 的 `sub_10018190`。本批只据此收口以下八个
函数：

- `SMA` 只读取末柱的 `N/M`，通过原生 float-to-int 截断；若 `N<=M` 或 `N<1`，
  整条输出 missing。有效时以首个有限源的 float32 值播种，按
  `(X*M+previous*(N-M))/N` 递推并逐柱收窄。启动后的 missing 还原为 TCalc 的有限
  sentinel 参与计算；结果精确等于 sentinel 或非有限时才映射回 missing。
- `COUNT` 从首个有效源到末柱以约 `1e-5` 比较周期是否为常量。常量近零或负值选择
  全部已有历史；动态近零值保留为空窗，只有实质负值才选择全部历史。周期经原生
  整数截断并夹到可用柱数，窗口内只统计 float32 后距 `1` 小于 epsilon 的值。
- `EVERY` 保留前导 source missing；启动后，周期 missing 或截断后小于 1 时输出 0，
  但不清空连续真值长度。内部 source missing 的有限 sentinel 为非零，按 true 进入
  状态机。`EXIST` 只读取末柱周期，保留 `-N` 和 `i-N` 的 signed-i32 wrap；前导及
  内部 source missing 均保持 missing，有限值绝对值超过 epsilon 时才更新命中位置。
- 二元 `MAX/MIN` 必须等到两侧首次共同有效才启动；之后各操作数先收窄为 float32，
  内部 missing 还原为 sentinel 做普通比较。MAX 的 `right>=left` 与 MIN 的
  `right<=left` 都在相等时选择右侧；解释器原有多参数形式现明确逐项调用同一二元
  handler 左折叠。
- `BARSLAST` 跳过所有前导 missing 和精确零，从首个非零有限值开始输出距离；启动
  后不再检查 sentinel，因此内部 missing 作为非零值把距离重置为 0。
  `BARSLASTCOUNT` 从首个有限源开始，对每个输出柱向历史扫描：missing 跳过，近似
  1 才累计，近似 0 立即停止，其他有限值既不计数也不停止。

focused native-operators 覆盖末柱参数门禁、负 M、float32 recurrence/sentinel、
COUNT 常量与动态 epsilon 分界、EVERY retained run、EXIST i32 wrap、MAX/MIN 右侧
相等选择以及两个位置函数的前导/内部 missing。正式 PID `24992` 上，平安银行 day
40 bars 的本批公式末值为
`S=11.2484588623047/C=0/E=1/X=0/H=11.2299995422363/L=11.1999998092651/B=3/BC=0`，
`errors=0`。

同批 quote-transition、1801/1802 host projection 独立测试、`npm run check/build`
和主程序构建均通过；未运行完整 CTest 或 full API。当前 EXE SHA-256 为
`17815ec422ff519eb69c69ecc3fe7186119b7ad00f2d3f41e6130754319f7c03`，部署网页
`index.html` SHA-256 为
`68b198ebed1345089b9a8df0b9255ca27f6e4c04e1d04fc6391ff0f4b77b428c`；health 为
`native_cpp=true`、`python_runtime=false`。PID `28660` 及其旧 EXE/web 哈希现仅为
前一批历史，PID `4716` 与对应哈希为更早历史。

## window-positions 后续语义与最终部署

同一新日志 `output/ida-tcalc-window-positions-20260813.log` 还直接覆盖
`HHVBARS/LLVBARS` 的 `sub_10018860/sub_10019860` 和 `FILTER/FILTERX` 的
`sub_10007950/sub_10007A10`。极值位置函数每柱先将 N 收窄为 float32 再截断；
period missing 保持 missing，N 非正或超过已有历史时使用全部可用柱。leading source
sentinel 被跳过，internal sentinel 仍参与比较；候选相关容差为相对 `1e-7` 加绝对
`1e-5`，容差带内取后出现位置。FILTER/FILTERX 排除 sentinel，以包含端点的
`value>=1e-5 || value<=-1e-5` 判真；FILTER 正向跳过命中后的 N 柱，FILTERX 反向时
只有 `N<=cursor` 才跳过前置 N 柱，超窗 N 不清空仍可扫描的前缀。

native-operators focused 已覆盖 period missing/全历史钳制、internal sentinel、容差、
inclusive `1e-5` 和 FILTERX 超窗。平安银行 day 40 bars 的结果为
`HHVBARS H=3/LLVBARS L=0/FILTER F=0/FILTERX R=0/errors=0`；FILTERX 只以
`allow_future` 显式启用只读 future 求值。同阶段 server project/catalog focused、
Svelte 0 errors / 0 warnings 与 web build 均通过；完整主程序构建为 104.7 秒，CTest
129/129 为 59.5 秒。

同批 1801/1802 先接入 `/api/v1/level2/project` 与 Level2Lab，严格内联 hex/字段、
384 KiB 且不回显输入；1801 runtime 为输入 52 B、source/host `1/1`、hex
`7440649001007b00000000005100000052000000`，SDK false、network 0、retained false。
其后追加的 1804 POST 只收 `format/payload_hex` 和精确 432 B，UI 显示 105 槽/
420 B。正式 1804 runtime 的 schema 为
`tdx-level2-sdk-1804-host-projection-v1`，first/second copied `2/3`，SDK/storage
false、network 0、retained false。1804 HTTP/UI 追加后仅重跑 project/catalog focused、
Svelte 0/0 与 web build；104.7 秒完整 build 属于追加前阶段。最终 1804 surface 完成
后又运行完整 CTest，129/129、0 failed，约 73 秒（10:38:34—10:39:47）。本轮未运行
full API。

最终正式 PID `4336`；EXE SHA-256
`0c945050aab8db2134cc5e23b5e62a4dc7431e77630e2d53cbfaca0349198c28`，网页
`index.html` SHA-256
`3eba9d193d3b04840b57a709be00907926c20f07cfca6438eadc5c7b1b152339`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `22116` 及中间 EXE
`580ed60e50492f7a5e52d1b1093bf3e9a7726fb15d4f0862712419e1828289c3`、网页
`5e246f8299a167526971062ee4f252614503b3749c51e63c35d507074c4bd0f6` 现仅为历史；
PID `24992` 及上一阶段哈希也不再代表当前部署。

## SUMBARS 与 STDDEV handler 收口

`output/ida-tcalc-sumbarsx-beta-final.log` 直接记录 `SUMBARS` 的
`sub_1001ABE0`。该 handler 只跳过前导 source missing；一旦启动，后续 source 或
target missing 都以 TCalc 的有限 sentinel 进入普通比较/累加，而不是传播 missing。
它从当前柱向左以 float32 逐项累加，并用绝对 `1e-5` 加相对 `1e-7` 容差判断是否
达到目标。当前柱自身即达到阈值时，首个有效柱因左边界钳制输出 `0`，非首柱输出
`1`；focused 判别夹具 source `[10,10,10]`、threshold `10` 的结果是 `[0,1,1]`。
`SUMBARSX` 由另一 handler 实现，本批刻意保持不变。

`output/ida-tcalc-formula-rolling-variance.log` 直接记录 `STDDEV` 的
`sub_1001EA70` 和 sqrt helper `sub_1001E800`。周期只读最终 period 柱，先收窄为
float32 再按原生转换截断，并要求 `N>=2`；输出从首个有效 source 后第 N 柱开始，
使用输出柱之前 N 个价格形成的 N-1 个滞后对数收益。任一价格非正或不高于 epsilon
时，该对收益置零。source 操作数读取、比值、log、running sum、mean、每项平方偏差
累计、variance 与 sqrt/result 均按 handler 的顺序逐步产生 float32 落点，避免只在
最终结果一次性收窄造成数值漂移。

本批只运行 formula `native-operators` focused，结果通过；没有重跑完整 CTest，也没有
运行 full API。129/129 是上一轮最终 1804 surface 阶段的完整 CTest 结果，不能归入
本批。正式服务上的真实平安银行 day 40 bars 样本末柱为 `S=1`、
`V=0.0008861038950271904`，`engine=native`。

当前正式 PID `23140`；EXE SHA-256 为
`25882a4e62eda0d5c1702e2e4da4d8f60739c47ab0927616a8ee13cf0e3b9183`，部署网页
`index.html` SHA-256 仍为
`3eba9d193d3b04840b57a709be00907926c20f07cfca6438eadc5c7b1b152339`。health 与磁盘
产物匹配，`native_cpp=true`、`python_runtime=false`。PID `4336` 和其 EXE 哈希现仅
表示上一阶段历史部署。

## SDK JSON 4653 surface 后的最终仓库与部署基线

本批没有改动公式 handler；新增的是 Level2 `sdk-json-4653` 的独立 domain、CLI
`level2 decode`、严格 POST `/api/v1/level2/decode` 与 Level2Lab 接线。它只接受严格
`{Data}` 和最多 10,000 条精确字段记录，价格按 float32 `/1000`，时间仅保留 opaque
`u16(61 * (atoi(CString::Right(datetime, 4)) % 100))`。恢复的 `35+18*N` 仅为布局语义，
不生成 native body；compact JSON/POST 上限为 384 KiB，POST 只允许
`format/document/limit` 且不回显输入、不读服务器文件、不调用 SDK、不联网或订阅。

4653 domain、server 与 catalog focused 均通过，`svelte-check` 为 0 errors / 0 warnings，
web build 成功且只有既有 chunk warning；随后完整 CTest 为 130/130、0 failed、
50.51 秒。正式 runtime fixture 为 `count=2`、`returned=1`、`reference_price=10`、
`close_price=10.25`、`average_price=10.100000381469727`、
`trade_volume_u32_raw=123`、`time_u16_raw=61`，并保持 SDK false、native body false、
network 0、retained false。
这不表示 Level2 授权绕过、订阅或 SDK 调用能力。

当前正式 PID `12148`；EXE SHA-256
`b8e66b48c283383520f010148554a0cb80c58997601868c514954369854d8594`，部署网页
`index.html` SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `23140` 及其 EXE/web 基线已转为
上一阶段历史记录。

## BACKSET `sub_10017550` 精确语义

原有 BACKSET 的“命中后回填 N 柱”框架保持，但边界现对齐独立 handler：

- `sub_10001280` 只跳过 source 的 leading native missing；输出在首个有效 source 前
  保持 missing，并以该位置作为所有回填的左边界；
- 一旦启动，后续 missing 会还原成有限的大负 sentinel，作为普通非零 float 触发
  BACKSET，而不是传播 missing；
- source 先逐柱收窄为 float32，再以严格
  `abs(value) > 0.000009999999747378752F` 判断；正负恰等于阈值都不触发；
- N 逐柱先转 float32，再走原生 float-to-i32 边界；结果以 1 为下限。每个信号把当前
  与此前最多 N-1 柱置 1，超窗部分裁剪到 first-valid 下界。

focused native-operators 已覆盖 leading missing/超长裁剪、严格 1e-5 阈值、动态 N 的
float-to-i32 与 min-1、post-start sentinel 回填，结果通过。正式 `sz/000001` day 5
runtime 得 `B=[0,1,1,0,0]`、`engine=native`。本批没有运行完整 CTest；上一轮
sdk-json-4653 阶段的 130/130 不能归入本批。

当前正式 PID `17676`；EXE SHA-256
`dc6bbe1ff93295db75a77518a8970f8d3260fd0ad0b6e7ee30af6a01ee060a1d`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `12148` 与其 EXE 哈希现为历史。

## BARSCOUNT 与 DMA targeted handler 语义

`output/ida-tcalc-barscount-dma-targeted-20260813.json` 提供了两项直接伪代码证据：

- `BARSCOUNT sub_10006860` 仅扫描并保留 leading canonical sentinel；首个有效 source
  输出 float32 `0`，之后按 `0,1,2,...` 每柱递增，post-start source sentinel 不再影响
  计数。
- `DMA sub_10018410` 先寻找 X/A 首个共同非 sentinel 柱并用该柱 float32 X 播种。
  后续 A 不做上下界 clamp，负值照常外推；handler 计算
  `upper=A+abs(A)*1e-7+1e-5`，仅在 `upper>1` 时直接选择当柱 X，否则按
  `previous*(1-A)+X*A` 递推。X/A/previous 均为 float32 状态；post-start sentinel 是
  有限原生 float，继续进入 direct-X 或递推，只有输出精确等于 sentinel 或非有限时才
  安全映射为 missing。

调用审计为 BARSCOUNT 13 条公式/18 次，DMA 6 条公式/6 次。focused
`native-operators` 和 `language` 均通过；REF fixture 现用
`BARSCOUNT(CLOSE)+1` 保持既有偏移测试目标，BACKSET leading fixture 直接生成两根前导
missing 后验证 first-valid crop。正式 `sz/000001` day 5 为
`C=[null,0,1,2,3]`、
`D=[11.1899995803833,11.239999771118164,11.25,11.25,11.229999542236328]`，
`engine=native`。

本批未运行完整 CTest；130/130 仅属于上一 sdk-json-4653 阶段。当前正式 PID `9272`；
EXE SHA-256 `0388ce56c670078406e45def79a87f300e71b0621c9191638af954f140400063`，
网页 `index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `17676` 已转为历史部署。

## SLOPE `sub_1000CDD0` targeted 语义

`output/ida-tcalc-slope-targeted-20260813.json` 同时保存 `sub_1000CDD0` 和其 leading
source 扫描 helper `sub_10001280`：

- helper 只跳过 leading canonical sentinel；period 的可用历史以该 first-valid 为左界；
- 每柱 N 转换为 `int(float32(N)+0.503000020980835)`，N 小于 1 或超过
  `index-first+1` 时保持 missing；
- 完整窗口以时间从旧到新的 `1..N` 为自变量做线性回归；位置和/平方和、source 和、
  position×source 和与两项均值按原生顺序落为 float32；最终协方差分子、位置方差分母
  不另存 float32，而是在 x87 宽精度中直接相除，仅最终商写回时落为 float32；
- internal sentinel 不重新触发 first gate，而是有限 float 回归输入；N=1 不特殊返回 0，
  原生零分母产生的 singular/nonfinite 结果通过统一输出边界成为 missing。

379 库只有 `ACCER` 与 `BSQJ` 各调用一次，共 2 formulas / 2 calls；本批没有改动
`WMA/FORCAST`。新增 source `[5941892,49958348,-9642243,1061.6112060546875]`、N=4
判别夹具；正确输出为 `-7742307.5`，而错误的最终分子 float32 落点会输出
`-7742307.0`。focused `native-operators`、`language` 通过。正式 `sz/000001` day 10、
N=5 末柱为 `S=9.5367431640625E-07`、`normalized=8.507353044251431E-08`、
`engine=native`。本批未运行完整 CTest；130/130 仅属于上一 sdk-json-4653 阶段。

该 SLOPE 精度修正的中间部署 PID 为 `11332`；EXE SHA-256
`b79d085fe4ea93c5f56c825a3f202c32f831e2a9941c79136c18f2127494ce87`，网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `4380` 已转为历史部署。

## ABS `sub_1001D9F0` / MOD `sub_1000C990` targeted 语义

`output/ida-tcalc-formula-gap-audit-20260813.json` 给出两项直接证据：

- ABS 只以 leading canonical sentinel 建立 first gate；启动后遇到精确 sentinel 时原样
  保留，其他输入通过原生 float `fabs` 并写回 float32；
- MOD 先拒绝任一精确 sentinel，再将 dividend/divisor 分别按
  `int(float32(value)+0.503000020980835)` 转为 i32，执行保留 dividend 符号的 signed
  remainder 并写回 float32；转换后的 divisor 为 0 时输出 missing。

调用审计为 ABS 35 条公式/61 次调用、MOD 3 条公式/6 次调用；基础 MOD 判别例为
`MOD(5.4,2)=1`。focused `native-operators`、`language` 通过。正式 `sz/000001` day 5
的 2026-08-13 末柱 `CLOSE=11.210000038146973`，ABS 为同值，
`MOD(CLOSE*10,7)=0`，`engine=native`。本批未运行完整 CTest；130/130 仅属于上一
sdk-json-4653 阶段。

该 ABS/MOD 阶段正式 PID 为 `26364`；EXE SHA-256
`5284c5916b00cf158356c765154380280215675784069e6b416c4f56befc3258`，网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `11332` 为 SLOPE 精度修正中间态。

## SQRT `sub_1001D820` / INTPART `sub_1001DD50` targeted 语义

`output/ida-tcalc-sqrt-intpart-gap-audit-20260813.json` 给出两项直接证据：

- SQRT 跳过 leading canonical sentinel；原生 float carry 条件
  `abs_f32(x)*1e-7+x+1e-5<=0` 命中时沿用 previous，包含足够负的输入与 started
  internal sentinel；否则执行 `sqrt(float32(x))` 并写回 float32。首个可用值命中 carry
  时没有 previous，因此保持 missing；
- INTPART 只跳过 leading canonical sentinel；started internal sentinel 继续参与。
  handler 用 `float32(x)-abs_f32(x)*1e-7-1e-5<0` 选取
  `-0.000099999997/+0.000099999997`，再安全截断为 i32 并写回 float32；非有限或越界
  转换得到原生 `INT_MIN`。

调用审计为 SQRT `BOLL-RB/HISV` 2 formulas / 2 calls，INTPART `WSBVOL` 1 formula /
2 calls。focused `native-operators`、`language` 通过。正式 `sz/000001` day 5 的
2026-08-13 末柱 `CLOSE=11.210000038146973`、`SQRT=3.3481338024139404`、
`INTPART=11`、`engine=native`。本批未运行完整 CTest；130/130 仅属于上一
sdk-json-4653 阶段。

上一 SQRT/INTPART 阶段正式 PID `24720`；EXE SHA-256
`9f45aa0a6754496873b7b29a754dd94bbed930c3014dda26b7bb5b5ee6b58e8c`，网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；PID `26364`
为上一 ABS/MOD 阶段部署。

## MEMA `sub_1000AEF0` / EXPMEMA `sub_1000B1A0` targeted 语义

`output/ida-tcalc-mema-expmema-gap-audit-20260813.json` 给出两项完整 handler 证据：

- 两者都只读取末柱 N，并经 float32 安全截断为 i32；seed 窗口的 internal sentinel
  前向继承上一 source，seed 累加、状态、递推和输出逐处落为 float32；
- MEMA 仅在 `first+N<count` 时播种，随后按 `((N-1)*previous+X)/N` 递推；
  EXPMEMA 允许 `first+N<=count`，随后按 `((N-1)*previous+2*X)/(N+1)` 递推；
- seed 后 source sentinel carry previous；previous 已为 sentinel 时保持传播。

调用审计为 MEMA 1 formula / 4 calls、EXPMEMA 4 formulas / 4 calls。分派改用 typed
helper 后，已删除无调用的旧通用 `exponential` helper，EMA/EXPMA 既有语义不变。
focused `native-operators`、`language` 通过。正式 `sz/000001` day 5 live 为
`E=11.229166030883789`、`M=11.235184669494629`、`engine=native`。本批未运行完整
CTest；130/130 仅属于 sdk-json-4653 阶段。

上一 MEMA/EXPMEMA 阶段正式 PID `32380`；EXE SHA-256
`81b2845e4104e6fd22d79f3be288c99b92653189416b31f9f32b7c83ef054ca1`，网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `24720` 为上一 SQRT/INTPART
阶段历史部署。

## AVEDEV/POW targeted 语义及本批 Level2 固定快照

`output/ida-tcalc-next-scalar-series-targeted-20260813.json` 提供 AVEDEV、POW 的完整
伪码与逐指令证据。AVEDEV 固定读取末柱 N，以 leading canonical sentinel 为首个有效值
门槛，并保留 float32 落点/wide 中间值混合语义；调用审计为 CCI 1 formula / 1 call。
POW 使用 float32 operands、mixed-wide gate 与 invalid carry；调用审计为 BOLL-RB
1 formula / 1 call。focused `native-operators`、`language` 通过；正式 `sz/000001`
day 5 live 为 `D=0.015555699355900288`、`P=1.4884006977081299`、`engine=native`。

同批 Level2 CLI-only `sdk-1803-host-projection`/`sdk-18031-host-projection` 对 exact
`32016`/`20012` 字节 body 做 byte-for-byte 完整逻辑状态替换投影，只报告 prior/new
metadata、size 与 SHA-256；不读 previous state、不回显或字段化 body。raw/hex 预读有界，
decoded body 受 `384 KiB` 上限；所有 callback/SDK/storage/message/network/request/
subscription 副作用为 false/0。focused 固定快照 target 已通过；SHA-256 局部实现，最终
未改变 common 公共接口。

上一 AVEDEV/POW + Level2 固定快照阶段正式 PID `30060`；EXE SHA-256
`cca7fdb7cace9a712b177a64322b0b1b9462025f12f7ef551b56074e9df7b91c`，网页
`index.html` SHA-256 为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `32380` 为上一 MEMA/EXPMEMA
阶段历史。本批未运行完整 CTest；130/130 仍只属于 sdk-json-4653 阶段。

## VALUEWHEN `sub_1000B830` / BETWEEN `sub_1001DE70` targeted 语义

证据 `output/ida-tcalc-next-scalar-series-targeted-20260813.json` 给出两项完整 handler：

- VALUEWHEN 只由 condition 决定 leading gate；started 后 exact float32 zero carry held，
  任意 nonzero（含 sentinel）选择 raw float32 selected，selected sentinel 覆盖 held；
- BETWEEN 的 leading gate 只检查两个 bounds 是否同时为 sentinel，value 不参与；started
  后按 raw float32 处理，整理 bounds 顺序，并以 float32 `abs(value)` 执行
  relative/absolute open-tolerance 判定。

调用审计为 VALUEWHEN/HANS123 1 formula / 2 calls、BETWEEN/W106 1 formula / 1 call。
focused `native-operators`、`language` 通过。正式 `sz/000001` day 5 live source
`Q:=CLOSE>11.2;V:VALUEWHEN(Q,CLOSE);B:BETWEEN(CLOSE,11,11.3);` 末柱为
`V=11.229999542236328`、`B=1`、`engine=native`。

当前正式 PID `32284`；EXE SHA-256
`76787fa34f2b53c563e8ebfa70a651bbcce05271ac9873d70b81377073fafacc`，网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `30060` 为上一 AVEDEV/POW +
Level2 固定快照阶段历史。本批仍未运行完整 CTest；130/130 仍只属于 sdk-json-4653 阶段。

## REFX `sub_10019F90` / REFXV `sub_1001A300` targeted 语义

`output/ida-tcalc-refx-limit-price-targeted-20260813.json` 给出完整伪码、逐指令
和直接 helper。两者均以 source/offset 同时非 canonical sentinel 结束
dual-leading gate；started 后不再重新验证 sentinel，以 raw float32 处理 operands/
source，并按原生顺序保留 offset `fabs`、target float32 landing 与 wide tolerance
比较。合法 offset 安全 trunc 为 i32 后前视选取 raw source；无效 REFX 写
missing，无效 REFXV 在 index 0 选当柱 raw source，否则 carry previous raw
output，最终才统一安全映射输出。

调用审计为 `NXTS/WAVEKX/SQJZ/ICHIMOKU` 4 formulas / 15 calls，各为
2/4/8/1 次。focused `native-operators`、`language` 通过，language 中基于旧反向/
夹取行为的预期已纠正。正式 `sz/000001` day 5 live 为
`R=[11.260000228881836,11.25,11.229999542236328,null,null]`、
`V=[11.260000228881836,11.25,11.229999542236328,11.229999542236328,11.229999542236328]`、
`engine=native`。两者仍分类为 `allow_future=true` 时的
`explicit-read-only-lookahead`，不会被 scan/backtest 隐式开启。

最终正式 PID `26104`；EXE SHA-256
`8c8c893ed3d652756361199861e751db906b0a95348e30913f72992adf7ffd4e`，网页
`index.html` SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `32284` 为上一 VALUEWHEN/BETWEEN
阶段历史。本批未运行完整 CTest；130/130 仍只归 sdk-json-4653 阶段。

## LAST `sub_10005970` / NDAY `sub_100165F0` 与 Z/D host-context gate

`output/ida-tcalc-last-nday-targeted-20260813.json` 给出 LAST 210 条、NDAY 177 条完整
指令及直接 helper。两者的整数参数只取末柱并先经过 float32。LAST 只跳过 source
leading sentinel，`A=0` 从首个有效柱起累计历史，窗口用严格 float32 epsilon 判零，
内部 sentinel 按 raw 非零值处理；NDAY 从两个 operands 同时非 sentinel 的首柱启动，
以 float32 magnitude tolerance 比较并维护连续满足计数，内部 sentinel 继续按 raw
float32 比较。调用审计为 `XRDS/QTDS` 2 formulas / 2 LAST calls、`K300/K310`
2 formulas / 2 NDAY calls。

ZTPRICE/DTPRICE 的新静态边界由
`output/ida-tdxw-type120-security-class-targeted-20260813.json` 与
`output/ida-tdxw-security-record-precision-displacements-20260813.json` 共同固定：精确
证券分类依赖 `runtime+283`、`sub_5A3810`、`type120+31`，不能用 TNF `+76` 的
`price_precision` 代替。`B007/C128/C129/C130` 继续 executable approximation，但
`numeric-degraded`、`pure_ohlcv=false`；全库为 375 numeric-safe / 4 degraded，scan 与
backtest 拒绝降级输出。nested surrogate cause 按数据流递归保留，因此
`RGB(ZTPRICE(...))` 同时携带 RGB 与
`ZTPRICE#HOST_TYPE120_CONTEXT_UNRESOLVED`。

formula target build 与 focused `native-operators/context-and-library/language` 均通过；
临时、正式 focused API `health/evaluate/nested/scan/backtest` 均通过。正式
`sz/000001` day 40 末柱 `L=1`、`D=0`，nested causes 为 `RGB + ZTPRICE`。未运行完整
CTest 或 full API。正式 PID `24556`；EXE SHA-256
`39483c732fa9a317cfc4b4674193c93a034f3703dc857508c5b61a63516bb36a`，网页 SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `26104` 与 EXE
`8c8c893ed3d652756361199861e751db906b0a95348e30913f72992adf7ffd4e` 为历史，PID
`32284` 是更早历史。回滚副本为
`output/tdx-tool-refx-pre-last-nday-zd-gate-predeploy-rollback-20260813.exe`。

## UPNDAY `sub_10016330` / DOWNNDAY `sub_10016490` targeted 语义

`output/ida-tcalc-upnday-downnday-targeted-20260813.json` 提供 UPNDAY 135 条与
DOWNNDAY 134 条逐指令证据。两者只取 final N 并经 float32 trunc，以 source-only
leading gate 启动；零输出从 `first+N-1` 初始化，比较从 `first+1` 开始，严格原生
tolerance 驱动连续 run，`run==N` 后回退 `N-1`，post-start sentinel 按 raw float32
参与。调用审计为 UPNDAY 的 `UPN/K300` 2 formulas / 2 calls、DOWNNDAY 的
`DOWNN/K310` 2 formulas / 2 calls。

formula target 与 focused `native-operators/language` 通过；正式 `sz/000001` day 800
末柱 `U=0`、`D=1`、`K=0`。本批未跑 full CTest/full API。正式 PID `17880`；EXE
SHA-256 `7f70d91d456598bfc6d3ecef3318e7fbebd162dac612a9d31efef06f73b5f05e`，web SHA-256
仍为 `71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `24556` 与 EXE
`39483c732fa9a317cfc4b4674193c93a034f3703dc857508c5b61a63516bb36a` 为历史。回滚副本：
`output/tdx-tool-last-nday-zd-gate-pre-upnday-downnday-predeploy-rollback-20260813.exe`。

## STD `sub_1001E850` 复核 / LOWRANGE `sub_100072D0` targeted 语义

`output/ida_probe_tcalc_std_lowrange_20260813.py` 生成的
`output/ida-tcalc-std-lowrange-targeted-20260813.json` 包含 STD handler 的
110 条指令，以及它使用的 `VAR sub_1000D5E0` 和
`sqrt sub_1001E800` 的完整复核。结果确认当前 STD 主路径已与目标一致，
因此本批未修改 STD 实现，也未用新推测扩张其语义边界。

LOWRANGE 的 144 条指令则固定了以下语义：

- 只有 source 参与 leading canonical-sentinel gate；通过 gate 后，当柱与回扫
  prior 都取 raw float32；
- 每根柱固定 current，从最新 prior 向索引 0 回扫，严格在
  `current-prior < -1e-5` 时继续计数；相等或恰在 epsilon 边界不视为满足；
- started 后当柱的 internal sentinel，以及回扫命中的原始 leading
  sentinel，均作 raw float32 值参与，不再跳过或提前停止；
- 整数计数结果最终转为 float32 写回。

379 系统库中 LOWRANGE 影响 `YYD` 1 次、`W107` 3 次，即
2 formulas / 4 calls。formula target build 及 focused `native-operators`/`language`
通过；`sz/000001` day 800 的 `R:LOWRANGE(CLOSE)` 末七点为
`[5,0,8,0,1,2,0]`，正式 API 的 40 点页面样本与之一致。本批未运行
full CTest/full API。

同一产物还包含新增的 Level2 1803/18031 typed domain，但它们没有接入
CLI/HTTP/UI；具体结构和离线边界记录在 Level2 日志。当前正式 PID
`24140`；EXE SHA-256
`8571ce09ac27110defa68ed1727a3ed14ce4b834b3c1c944f26eb7ea022cdd7b`，web SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `17880` 与 EXE
`7f70d91d456598bfc6d3ecef3318e7fbebd162dac612a9d31efef06f73b5f05e` 为历史。
回滚副本：
`output/tdx-tool-directional-nday-pre-lowrange-level2-projections-predeploy-rollback-20260813.exe`。

## SAR/未来引用、BARSNEXT 与 REFDATE 原生语义

`output/ida-tcalc-sar-sarturn-targeted-20260813.json` 闭合 SAR/SARTURN：参数取末柱，
N 合法时以最初 N 柱的 tolerant low seed，状态与 OHLC 按 raw float32 推进；
SARTURN 的 leading sentinel 与首个 seed 原样保留，之后才输出 -1/0/1。系统库影响
SAR 3 个公式/4 次调用、SARTURN 2 个公式/2 次调用。

`output/ida-tcalc-included-includedv-targeted-20260813.json` 闭合
INCLUDED/INCLUDEDV 的末柱 selector/limit、self exclusion、固定 `1e-5` 容差、
已命中候选跳过与 backward/forward 距离限制。它们继续标为 future，只有显式
`allow_future=true` 才能作只读图表计算，scan/backtest 仍禁止。

`output/ida-tcalc-barsnext-zig-targeted-20260813.json` 锁定 BARSNEXT 的 raw-f32
sentinel 和 inclusive `±9.999999747e-6` 真值；只从最右真值向左写距离，右侧保持
missing，覆盖 WAVEKX 1/3。`output/ida-tcalc-if-datetoday-refdate-targeted-20260813.json`
锁定 REFDATE 的 final-target raw-f32+ftol、unsigned date compare 与 selected-source
raw-f32 broadcast，覆盖 BSQJ 1/2。正式平安银行 day/20 样本中，BARSNEXT 的
`5e-6` 条件 20 点全 null并标 explicit-read-only-lookahead；REFDATE `1260813`
广播 `11.25`。

聚焦 native-operators/language 均通过，随后单线程完整构建和 CTest `135/135`
通过，0 failed，`80.89 s`；Svelte check 0 errors / 0 warnings。本批未跑 full API。
当前正式 PID `36100`，EXE SHA-256
`a5fb392bf25fe6f2d224bc681212ca4049ad6364391b40b8f5730f311e07da0a`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health
匹配，`native_cpp=true`、`python_runtime=false`。PID `33268` / EXE
`69ac767a2d9cea3bc53899a92d6deed8df89f8c125582bc766b7ff56658e0db0` 为历史。
回滚副本：
`output/tdx-tool-formal-69ac767a-pre-barsnext-refdate-465x-rollback-20260813.exe`。

## ZIG final-threshold gate 与 4655 共享部署

ZIG 本批严格限定在一个已闭合差异：末柱 threshold 以 raw float32 落地，并按原生
`t-abs(t)*1e-7-1e-5 < 0` gate 对负值或无效值早退全零。未改正阈值路径、selector
识别或主状态机。focused 与正式 day/7 判别确认负阈值全 0、正阈值末柱 `11.25`；
future 执行边界仍是 `explicit-read-only-lookahead`。

同批 Level2 4655 依据
`output/ida-tpbus-sdk-4655-downstream-targeted-20260813.json` 增加 CLI-only
companion/raw conditional candidate；exact 46 B companion 来自 prior local clone，raw
shape 为 `39 + 18*signed_i16_count + 120*signed_i8_attach`，包括已锁定的
`count=-1/attach=1/size=141` 合法边界。host gate 不可用且不猜测，只输出 hash/attach
摘要；无 raw conversion、SDK/message/network、HTTP/UI，也不改 `sdk-json-4655`。

focused targets 通过；完整构建及 CTest `136/136`、0 failed、`88.96 s`；Svelte 沿用
本轮 0/0。本批未跑 full API。正式 PID `17824`，EXE SHA-256
`8623ec5d1326cd0dbdb901acab849fc5ee5fd6986549a1ae74c4ee757b9567ed`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `36100` / hash
`a5fb392bf25fe6f2d224bc681212ca4049ad6364391b40b8f5730f311e07da0a` 为历史。回滚
`output/tdx-tool-formal-a5fb392b-pre-zig-4655-rollback-20260813.exe`，SHA-256
`a5fb392bf25fe6f2d224bc681212ca4049ad6364391b40b8f5730f311e07da0a`。

## ZIG selector 尾部判定收口

第二个已闭合差异仅涉及 selector 识别：ZIG 从末端向前检查最多 10 组相邻 raw-f32，
以固定 `1e-5` 判定是否仍是 selector；不再扫描整段并要求全序列恒定。正阈值主状态机
与 selector mapping 均未改。focused `native-operators/language` 通过；正式 day/12
`X=[99,3×11]` 的 ZIG 与 CLOSE selector 全数组相等，仍是
`explicit-read-only-lookahead`。本阶段没有重跑 full CTest，`136/136` 明确属于上一
4655 阶段。

同批只将 4655 evidence 收窄为 compact-v2（`213228 B`、7 functions），wide 旧版另存
targeted-wide，production 行为没有变化。正式 PID `25764`，EXE SHA-256
`0e2b58844f822ea526e57f944484dad6ecac15d7c0994f925f3f2f60985fe694`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `17824` / hash
`8623ec5d1326cd0dbdb901acab849fc5ee5fd6986549a1ae74c4ee757b9567ed` 为历史。回滚副本
`output/tdx-tool-formal-8623ec5d-pre-zig-selector-rollback-20260813.exe` 的 SHA-256 为
`8623ec5d1326cd0dbdb901acab849fc5ee5fd6986549a1ae74c4ee757b9567ed`。

## BARSSINCE first-scan 收口与 4650 共享边界

`output/ida-tcalc-tr-barssince-targeted-20260813.json` 的完整 handler 证据确认，
BARSSINCE 启动扫描按 raw float32，仅 canonical sentinel/exact `±0.0f` 不启动；started
后只输出递增距离。focused 锁定 `1e-50` 下溢全 missing、leading sentinel 后 `1e-8`
启动为 `0..4`。系统库影响为 NXTS 1 formula / 5 calls。

同批 Level2 4650 的 targeted script/JSON 只证明 raw shape；host code/market、prior
state、time、`+408/+804/+72/+496/+80` 依赖仍未闭合，故没有实现 shape-only domain。
`output/ida-tpbus-sdk-4650-previous-state-targeted-20260813.json` 随后闭合了四个指定
地址、22 个完整函数和 58 个 offset xref；确认五字段仍不足，完整契约还需宽泛
quote/vector/identity/object 状态及 live/server time policy，因此维持不实施。

focused native-operators/language 通过；本批未跑 full CTest/full API，`136/136` 仍是
上一 4655 阶段结果。正式 PID `35408`，EXE SHA-256
`47ca2f7ea97c4a88a3a7904ddf496bf4c470428f2ca70c1e6413992b3c21a6b7`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `25764` / hash
`0e2b58844f822ea526e57f944484dad6ecac15d7c0994f925f3f2f60985fe694` 为历史。回滚
`output/tdx-tool-formal-0e2b5884-pre-barssince-rollback-20260813.exe`，SHA-256
`0e2b58844f822ea526e57f944484dad6ecac15d7c0994f925f3f2f60985fe694`。

## MA、BARSLAST 与 BARSLASTCOUNT targeted 收口

三个新 evidence 文件分别闭合 MA 187 条、BARSLAST 126 条、BARSLASTCOUNT 249 条
指令，均无未展开 frontier。MA 的确定差异是 period buffer 的 raw-f32→int，而不是现有
通用 double→int/1e6 clamp；`2.99999999` 是高判别夹具。source 的 canonical sentinel
也按原生 first/window gate 处理。后两者主循环已经吻合，只修 leading finite sentinel。

focused native-operators/language 与 day/800 live 样本通过；三个正式 POST evaluate 合约
分别锁定 `[null,null,11.246665954589844,...]`、`[null,null,0,1,2]`、
`[null,1,2,0,0]`，全部 `native-cpp`、`request_body_retained=false`。未跑 full CTest/
full API，`136/136` 仍是历史 4655 阶段结果。

正式 PID `34664`，EXE SHA-256
`b6e914030227e508e9234846173540b6e981f215a04801094a32908ebd48f34e`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `35408` / hash
`47ca2f7ea97c4a88a3a7904ddf496bf4c470428f2ca70c1e6413992b3c21a6b7` 为历史。回滚
`output/tdx-tool-formal-47ca2f7e-pre-ma-barslastcount-rollback-20260813.exe`，SHA-256 同为
`47ca2f7ea97c4a88a3a7904ddf496bf4c470428f2ca70c1e6413992b3c21a6b7`。

## REF raw-f32 双输入 gate 收口

新脚本/证据 `output/ida_probe_tcalc_ref_targeted_20260813.py` 与
`output/ida-tcalc-ref-targeted-20260813.json` 覆盖 `sub_10019DF0` 163 条指令、
`__ftol2_sse` 及空 frontier。实现不再把有限 canonical sentinel 当普通值：只有 source
和 offset 同时有效才启动；selected source sentinel 保持 missing，started 后 invalid
offset 则按原生 carry 前一 raw output。影响审计为 131 formulas / 554 calls。

native-operators/language 通过；day/5 普通 REF 为
`[null,11.1899995803833,11.289999961853,11.2600002288818,11.25]`。正式 POST 另验证
source sentinel 得到前两柱 missing，以及 offset sentinel 在第三柱 carry
`11.1899995803833`；三者均 native C++、请求体不保留。未跑 full CTest/full API，
`136/136` 仍归历史 4655 阶段。

正式 PID `25428`，EXE SHA-256
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `34664` / hash
`b6e914030227e508e9234846173540b6e981f215a04801094a32908ebd48f34e` 为历史。回滚
`output/tdx-tool-formal-b6e91403-pre-ref-rollback-20260813.exe` 保留该旧 hash。

## 连板天梯共享 Web 部署

本阶段没有改公式解释器；REF 的 targeted 语义与 focused 结果保持不变。共享 Web 新增
`/data/limit-ladder`，严格消费已有只读 v1 schema，并将上游晋级率与本地复算并列展示，
不把连板研究数据解释为交易动作或实时行情。研究行业成员跳转保留真实
`research-industry` family。

Svelte check 0/0、Web build 通过，正式五条 focused GET 中源为 248 条、live、公式复算
mismatch=0；未跑 full CTest/full API。正式 PID `22616`，EXE SHA-256 仍为
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256 为
`da7da0814cb7745e9ab52537ad80c7e84b3cfc0cd033c5c0af8af761247558dd`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `25428` 是上一 REF 阶段。

## 开盘与盘后成交共享 Web 部署

本阶段没有改公式解释器；REF 的 targeted 语义与 focused 结果保持不变。共享 Web 新增
`/data/session-turnover`，严格消费已有只读 v1 schema，并以服务端完成 A 股 / ETF、
活跃状态、七种排序和分页；raw 与实时行情、市值、行业字段均不进入页面，也不产生交易动作。

Svelte check 0/0、Web build 通过；正式五条 focused GET 覆盖 5,515 条 A 股、1,624 条
ETF、3,838 条双活跃、`SZ000001` 和非法 universe=400。本批未跑 full CTest/full API。
正式 PID `34564`，EXE SHA-256 仍为
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256 为
`2bec35c186633960ef0d00e0ca2142aa9b1cd54b5d0e792f0268a5a3061464be`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `22616` 是上一连板天梯阶段。

## 资金信号后续表现共享 Web 部署

本阶段没有改公式解释器；REF 的 targeted 语义与 focused 结果保持不变。共享 Web 新增
`/data/flow-followup`，联合展示两组逐日历史与四组资金分档模型；后续沪深300表现明确是
历史相关性，不是 forecast、investment signal 或交易动作。raw/upstream text 不进入页面，
北向 `1040/0` 占位历史和 current pair 均保持不可用。

Svelte check 0/0、Web build 通过；正式 focused 合约覆盖历史 2,122/2,237 条、占位 466 条、
模型 11/11/16/16 档和非法过滤 400。重启后一次 WinHTTP 12030 经短重试恢复 live；
未跑 full CTest/full API。正式 PID `20508`，EXE SHA-256 仍为
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256 为
`1929f3d7cb4cabeda51998e8f512512f82ac995ab1071800fd43f93d6992f371`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `34564` 是上一成交页面阶段。

## 1803/18031 CLI 部署与公式边界不变

本阶段未改公式解释器；REF 及其 focused 结果保持不变。Level2 仅为已有 1803 depth-record
与 18031 queue-record typed projection 增加独立 CLI orchestration，支持 exact raw/hex 和
18031 显式 identity，不产生公式上下文、交易信号或任何执行副作用。

两个 Level2 affected targets 与 `tdx-tool` 构建通过，完整 CTest `136/136`、0 failed、
72.77 秒；正式 help smoke 通过，未跑 full API。正式 PID `23872`，EXE SHA-256
`4ece5208d192711327888a29879a07a41d7889c42e37d8fc97dbf14642d5d16b`，web SHA-256 仍为
`1929f3d7cb4cabeda51998e8f512512f82ac995ab1071800fd43f93d6992f371`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `20508` 是上一资金页面阶段；rollback 为
`output/tdx-tool-formal-6c4d7c53-pre-record-cli-rollback-20260813.exe`。

## MA 定点证据复核（无生产差异）

新增定点证据 `output/ida_probe_tcalc_ma_targeted_20260813.py` 与
`output/ida-tcalc-ma-targeted-20260813.json`：`TCalc!sub_1000AD00` 共 187 条指令，
唯一直接 helper 为完整的 `__ftol2_sse`（55 条），frontier 为 0。逐指令复核确认当前
`tcalc_moving_average` 已一致实现 source-only leading sentinel gate、逐柱 raw-f32 N 截断、
首有效柱相对历史门禁、current-to-past 累加、窗口内 sentinel 跳过但仍固定除 N，以及每次
累加和最终输出的 float32 落点。

本轮没有发现可判别的 MA 语义错配，因此没有修改 production/tests，也没有构建或运行测试；
正式 PID、EXE/web hash 与上节 1803/18031 CLI 阶段保持不变。

## fnReqData surface 部署与公式边界不变

本阶段未改公式解释器；MA 定点复核仍为“与原生一致、无 production patch”。Level2 仅把
既有 fnReqData typed plan 接到 POST/Level2Lab，输出 12 槽 ABI 元数据，SDK、wire、请求、
订阅和公式交易动作均不执行。

server/catalog、Svelte 0/0、Web build、完整 build 与 CTest `136/136`（66.86 秒）通过。
正式 PID `10404`，EXE SHA-256
`90d95a18162d36441d36d4591eb95f25b5d146e94c4cd72647892f77f59f9b13`，web SHA-256
`1edf97c14f8b17d5e5ef2989709ba47d652a2a847c597edc50a56f46daf91cd2`；health 匹配，
PID `23872` 为历史，rollback 为
`output/tdx-tool-formal-4ece5208-pre-fnreqdata-surface-rollback-20260813.exe`。

## ZIG 百分比状态机最终收口

完整 evidence `output/ida-tcalc-barsnext-zig-targeted-20260813.json` 的
`TCalc!sub_100227A0` 已用于替换百分比 `ZIG` 的简化 pivot 主路径。实现保持此前闭合的
raw-f32 阈值门禁和十组 selector tail，并新增原生局部极值筛选、正/负候选状态、百分比
反转公式、`1e-7 + 1e-5` 比较、float32 插值和末段 repaint。尤其是未达到反向确认阈值的
较早极值不会再被首末点直线抹掉；`[10,11,12,11.5,11],20%` 精确输出原数组。

`ZIGA` 独立绝对价 handler 未改；MA 定点闭包仍是“现实现一致、无 patch”。native-operators、
language 和完整 formula-engine tests 通过；完整 build、CTest `136/136`、0 failed、65.96 秒。
正式 `SZ000001` day/40：`WAVE.RESULT=11.25`，`NXTS` 熊市/牛市天数 `0/0`，均登记未来
函数且保持 explicit-read-only/future 边界。本批未跑 full API。

正式 PID `464`，EXE SHA-256
`cfb8ed2317545c67509b0477bd3b346f72f20954f17c2bc537b360775b042def`，web SHA-256
`1edf97c14f8b17d5e5ef2989709ba47d652a2a847c597edc50a56f46daf91cd2`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `10404` 为上一 fnReqData 阶段；rollback 为
`output/tdx-tool-formal-90d95a18-pre-zig-native-state-rollback-20260813.exe`，其 SHA-256 为
`90d95a18162d36441d36d4591eb95f25b5d146e94c4cd72647892f77f59f9b13`。

## 期权工作台阶段的公式边界

本阶段未改变公式 AST、解释器数值语义或交易事件 IR；ZIG/MA 的上一阶段结论保持不变。
新增内容仅为期权 Web surface，以及 option-expiry / volatility / chain 规则来源的
TDX-root-relative 安全投影。

期权 focused、Svelte 0/0、Web build、完整 build 与 CTest `137/137`、0 failed、65.45 秒通过；
正式 A2609 样本为 62 个合约、31 个行权价、62 个报价/IV，且资源来源不含绝对 TDX 根。
本批未跑 full API suite。

正式 PID `33236`，EXE SHA-256
`cd1793637f52345a43a5978c3e40020848cc7af8321159d95d3d4884b8464cce`，web SHA-256
`40cf58f4cfd5e942404ff98454464be0b444859f05e572f7b373f9ac9e0e8aed`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `464` 为上一 ZIG 阶段；rollback 为
`output/tdx-tool-formal-cfb8ed23-pre-option-surface-rollback-20260813.exe`，SHA-256 为
`cfb8ed2317545c67509b0477bd3b346f72f20954f17c2bc537b360775b042def`。

## 本地参考档案阶段的公式边界

本阶段没有修改公式 AST、数值解释器、未来函数登记或交易事件 IR。新增能力是本地只读的
`/data/local-reference` Web 页面，外加 historical-securities、index-events、fund-reference
来源路径的 TDX-root-relative 投影；它不进入公式上下文，也不触发账户、下单或网络行为。

基金解析按 `TdxW!sub_4F4A50` 允许 ETF 日期为空，只有双端日期存在时才解析 lifecycle；
正式基金目录恢复为 4218 条、其中 2 条状态未定。相关 native/server/catalog focused tests、
Svelte 0/0、Web build、完整 build 与 CTest `138/138`、0 failed（测试耗时合计 63.33 秒）通过。
正式 API smoke 验证 root-relative 来源、零网络与绝对根不泄漏；未跑 full API suite。

正式 PID `26472`，EXE SHA-256
`abbff3821c137e65e08630aa07373325530dd10f1605f040bb3e203fde4f131d`，web SHA-256
`b907273787fe625d39ada3519218e259f84fcc6209f356ea76596e979c5d2abc`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `33236` 为上一期权阶段；rollback 为
`output/tdx-tool-formal-cd179363-pre-local-reference-rollback-20260813.exe`，SHA-256 为
`cd1793637f52345a43a5978c3e40020848cc7af8321159d95d3d4884b8464cce`。

## 港股本地档案阶段的公式边界

本阶段没有修改公式 AST、数值解释器、未来函数或交易事件 IR。重新对照高影响纯 OHLCV 函数后，
CROSS 状态机与既有 TCalc 证据一致；MA 当前实现已覆盖动态周期、前导/内部缺失、float32 倒序
累加等高判别语义，现有证据不足以证明新的错配，因此没有做猜测性修改。

实际变更是非公式的 `/data/hk-reference` 及 HK actions / finance 来源 root-relative 投影。
正式样本为 actions 32400 条、finance 3238 条，响应无绝对根、`network_requests=0`。四个
focused tests、Svelte 0/0、Web build、完整 build 与 CTest `138/138`、0 failed（64.14 秒）通过；
正式页面与 3 个 API 边界合约通过，未跑 full API suite。

正式 PID `11040`，EXE SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`，web SHA-256
`fbd587b6d028419287e6d070af50968795a1b49f7d9d45524104a70b1e43b5ba`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `26472` 为上一阶段；rollback 为
`output/tdx-tool-formal-abbff382-pre-hk-local-reference-rollback-20260813.exe`，SHA-256 为
`abbff3821c137e65e08630aa07373325530dd10f1605f040bb3e203fde4f131d`。

## MA targeted 闭包与路演阶段边界

新脚本/证据 `output/ida_probe_tcalc_ma_targeted_20260813.py` / `ida-tcalc-ma-targeted-20260813.json`
完整覆盖 `sub_1000AD00` 187 条指令、唯一 ftol2 helper 55 条、frontier 0。对 93 个系统公式所用
MA 逐项复核后，现实现的 source-only leading gate、逐柱 N、内部 sentinel 跳过、倒序 f32 累加、
固定除 N 与输出 f32 均一致；本阶段不改公式 AST、数值语义或 trade-event IR。

非公式变更为路演全市场页和单票面板。正式 2495 条、平安银行 38 条；focused tests、Svelte
0/0、Web build 通过。本阶段未重跑 full CTest，上一阶段 138/138。

正式 PID `5760`，EXE SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`，web SHA-256
`860818fd92289d42134ef08b59cd0142a41b7e94b7b14ec3ee16035f01d54b2b`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `11040` 为上一阶段；executable rollback 不变。

## 扩展市场 Web 阶段的公式边界

本阶段没有修改公式 registry、AST、数值 helper、上下文或 trade-event IR。新增的
`/data/expansion-market` 只消费既有 7727 公开行情文档；上一阶段 MA targeted 闭包及“不做无证据
production 改动”的结论不变。`tdx-native-tests`、Svelte 0/0、Web build 447 modules 通过；未重跑
full CTest/full API，138/138 仍属更早完整阶段。

正式 PID `17772`，EXE SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`，web SHA-256
`7ddbaba11960804a4b51e107ca955e70ded526aeb4b277fb9eab57632bb60ef6`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `5760` 为上一阶段；executable rollback 不变。

## 4654 / 4651 / 4655 surface 阶段的公式边界

本阶段没有修改 formula registry、AST、数值解释器、上下文、未来函数或 trade-event IR；变更仅为
Level2 已有 typed domain 的 HTTP/Level2Lab surface。4654/4651/4655 都是显式内联、只读离线投影，
不进入公式行情上下文，不执行 callback、宿主消息、SDK、订阅、网络或账户动作。4655 的 live host
gate 保持 unresolved，不把 conditional candidate 当成真实宿主状态。

三个 domain 与 server/catalog focused、Svelte 0/0、Web build 447 modules、完整 build 与 CTest
`138/138`、0 failed（66.59 秒）通过；正式 API/page 契约通过，未跑 full API suite。

正式 PID `33660`，EXE SHA-256
`75e11ac73bb68e1190bde4a953e8acba10da393c6987d2fb9eb53145590ae5fb`，web SHA-256
`71a3bc22a8999cb638e8c6e0f520a5d6f521cb6c6aa2ab68f5af6bcb27bb8f39`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `17772` 为上一扩展市场阶段；rollback 为
`output/tdx-tool-formal-6622e99e-pre-level2-465x-surface-rollback-20260813.exe`，SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`。

## 0x053E Web 阶段的公式边界

本阶段未改 formula registry、AST、数值 helper、上下文或 trade-event IR。CROSS 与 MA 再次按
当前完整证据复核，没有形成新的确定错配，因此不做猜测性 production 改动。新增能力仅是个股
页面对成熟 `/api/v1/market/speed` 的可视化；它不改变公式动态行情绑定，也不触发账户或交易动作。

`tdx-native-tests`、Svelte 0/0、Web build 449 modules 与正式 speed/page 样本通过；本批未重跑
full CTest/full API，上一完整基线为 138/138。

正式 PID `12628`，EXE SHA-256
`75e11ac73bb68e1190bde4a953e8acba10da393c6987d2fb9eb53145590ae5fb`，web SHA-256
`9bcb7d1e038a9b830446d22f32a6aeba58f210f414f01c9a3e532eb2c8be7e01`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `33660` 为上一阶段；executable rollback 不变。

## TOPRANGE 原生 float/sentinel 闭包

新脚本/证据 `output/ida_probe_tcalc_toprange_targeted_20260813.py` /
`output/ida-tcalc-toprange-targeted-20260813.json` 闭合 `sub_10007170`：144 instructions、0 direct
callee。TOPRANGE 只在起点跳 canonical sentinel；started 后 current/prior 都是 raw f32，内部和
回扫可达的前导 sentinel 不重新触发缺失边界，计数条件为严格 `difference > 9.999999747e-6`。
production 只新增 TOPRANGE typed helper；LOWRANGE、交易事件 IR、上下文和 schema 均未动。

focused native-operators/language 与完整 build、CTest `138/138`（66.76 秒）通过。正式 native-cpp
向量为 internal `[0,1,0,3]`、leading `[null,null,2,3,4]`。379 系统公式中调用为 0，本次只改善
用户公式；未跑 full API suite。

正式 PID `20808`，EXE SHA-256
`cf4da92cf849ad4cecdce5a15ebb635c53c561db29bba768c00da472eec35a12`，web SHA-256
`9bcb7d1e038a9b830446d22f32a6aeba58f210f414f01c9a3e532eb2c8be7e01`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `12628` 为上一阶段；rollback 为
`output/tdx-tool-formal-75e11ac7-pre-toprange-native-rollback-20260813.exe`。

## 行情流状态页阶段的公式边界

本阶段只给系统页接入既有 `/api/v1/market/stream/status`，没有修改 formula registry、AST、数值
helper、上下文或 trade-event IR。对剩余高调用项再做静态交叉：STD、MA 与 CROSS 已有完整证据且
当前实现吻合；ZTPRICE/DTPRICE 仍依赖未闭合的宿主 type-120/原始市场上下文；MTM、ROC、PSY、
WR、CCI 的系统源码形态是公式引用而不是 TCalc builtin handler。故本批没有用便利实现替代原生证据，
也没有新增猜测性 production 改动。

验证仅为行情流 focused、Svelte 0/0 与 Web build；未重跑 formula/full CTest/full API，最近完整
138/138 仍属 TOPRANGE 阶段。正式 PID 仍为 `20808`，EXE SHA-256 仍为
`cf4da92cf849ad4cecdce5a15ebb635c53c561db29bba768c00da472eec35a12`，web SHA-256 更新为
`6419fd8c46c069a5627aa4278551f9d07de91fb37092294ea1bfff405f7ae66a`；health 匹配，
`native_cpp=true`、`python_runtime=false`。

## PEAK/TROUGH 原生 turning queue

新 evidence `output/ida-tcalc-peak-trough-targeted-20260813.json`（389949 B，SHA-256
`0e79e8308eafb1af852bc6c62ce139a065086f9545857ca813dcbbefdae77ea6`）由
`output/ida_probe_tcalc_peak_trough_targeted_20260813.py` 生成，完整覆盖 PEAK、PEAKBARS、TROUGH、
TROUGHBARS 四个 handler（211/217/211/216 instructions）、共享 ZIG 与 17 个 reachable helper，
frontier=0。

解释器只新增 future-domain 内部 typed turning projection：末柱 raw-f32 order 经原生整数转换，先复用
现有 ZIG，再以 raw-f32 sentinel 和 `abs(value)*1e-7 + 1e-5` 建立首候选，按阶数轮转队列，并把最终
尚未确认端点纳入 value/bars 输出。index 0 pivot、raw-f32 value、distance 与 missing 映射均按 handler
边界；不再使用严格三点局部极值。formula schema、context、trade-event IR、账户与下单路径未改，
`allow_future`/`explicit-read-only-lookahead` 仍是强制只读边界。

native-operators、language、context-and-library focused 与完整 build/CTest `138/138`、0 failed
（66.94 秒）通过。正式 40 根 `SZ000001` day 自定义样本末柱为
`P=11.630000114440918/PB=9/T=11.25/TB=0`；系统 XT 末柱箱底/箱顶/箱高为
`11.47499942779541/11.39739990234375/-0.6762486100196838`。379 库影响为 XT 中 PEAK/TROUGH
各一调用；两项 BARS 补齐 custom compatibility。未跑 full API suite。

当前正式 PID `35944`，EXE SHA-256
`ea666f23ac9af9cb7decb9d2608014bd48194e52f73227e8560a7c6fba0dcbdd`，web SHA-256
`6419fd8c46c069a5627aa4278551f9d07de91fb37092294ea1bfff405f7ae66a`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `20808` / EXE `cf4da92c...35a12` 为上一阶段；
rollback 为 `output/tdx-tool-formal-cf4da92c-pre-peak-trough-native-rollback-20260813.exe`，SHA-256
`cf4da92cf849ad4cecdce5a15ebb635c53c561db29bba768c00da472eec35a12`。

## IF/IFF 输出落点与全库缺口可见性（2026-08-14）

`output/ida-tcalc-if-datetoday-targeted-20260813.json` 的 IF `sub_1000B670` 为 96 instructions、0 direct
callee/frontier；最终选中分支通过 `fstp dword` 写入。production 因此只让 numeric IF/IFF selected
value 经 raw-f32 写回，保留 condition-only leading sentinel gate、started sentinel nonzero、StringSeries
选择和 IFN 独立 handler。证据 SHA-256 为
`8df630be46cb119fa6b1cb8c560b3541d8bf7d63a1c871670e53d4d7c4550971`；379 系统源码为 85
formulas/266 calls。native-operators 锁定 16777217→16777216、0.1 精确 f32、leading/post-start missing、
string IF 与 IFN 不回归；language 域通过。

公式库 Web 的 audit consumer 不再只展示聚合计数。它可展开 `context_bindings_unavailable` 与
`explicit_context_bindings_required` 的合并去重结果、external dependencies，以及 market/period/future
适用性边界。正式 day20 审计为 355/380 passed、20 context unavailable、0 errors；SIGNALS_QS、
L2_AMO、LARGE*/TRADE* 等只列缺口，不下载、不推导、不填零。类型/Svelte check 0/0、Web build
449 modules 通过。

本阶段 formula focused、完整 build 与 CTest `139/139`、0 failed（13.12 秒）通过；还包含新增 115
Level2 test，未跑 full API suite。当前正式 PID `32500`，EXE SHA-256
`44f513e8c5b8978395306743dd6078bf21a0d0826c7a75a1af2fedbfc91754fe`，web SHA-256
`e6dc95ac299879cdeca17fc5c6df230c8c11fdf4c2a5f082f3df6482e3156a64`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `35944` 为上一阶段；rollback 为
`output/tdx-tool-formal-ea666f23-pre-if-tpbus115-native-rollback-20260814.exe`，SHA-256
`ea666f23ac9af9cb7decb9d2608014bd48194e52f73227e8560a7c6fba0dcbdd`。

## ROUND / CEILING / FLOOR 原生取整（2026-08-14）

新 evidence `output/ida-tcalc-scalar-rounding-targeted-20260814.json` 为 143561 B，SHA-256
`24a5974f9ab23d7fd78502e6dae12d6d3b7bf6e7f6505ae50c859bfc5d6b9629`；ROUND、
CEILING、FLOOR handler 分别 54/125/128 instructions。解释器现复现 ROUND 的 per-bar canonical
sentinel 与 raw-f32 `±0.503000020980835`→i32，以及 CEILING/FLOOR 的 leading-only sentinel、
relative+absolute tolerance 和 started sentinel i32 行为。EXP/LN/LOG 未纳入本批。

独立 `native-scalars` 测试域覆盖 f32 折叠、正负偏置、容差内外、leading/internal sentinel 与溢出；
language/native-operators 同时通过。正式 day5 inline 结果为 `R=2/N=-2/C=2/F=-2`，engine=native，
request body retained=false。完整 build/CTest `139/139`、0 failed（12.59 秒）；未跑 full API。

同批 Web 恢复 `/protocol/workflows` 固定云 workflow consumer，Svelte 0/0、build 451 modules；它不改变
公式执行 schema，也不开放任意 URL/path/body。当前正式 PID `20792`，EXE SHA-256
`230837462287aa23136c93598ac4227c5c470a6b3c29155e09d34093af4db1a3`，web index SHA-256
`45b19054a9a142e19e78f34db5422bda668c544d99015008695f12b30d1e6543`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `32500` 为历史；rollback 为
`output/tdx-tool-formal-44f513e8-pre-round-workflows-rollback-20260814.exe`，SHA-256
`44f513e8c5b8978395306743dd6078bf21a0d0826c7a75a1af2fedbfc91754fe`。

## EXP/LN/LOG type-3 与 Series 分流（2026-08-14）

`output/ida-tcalc-scalar-rounding-targeted-20260814.json` 内 EXP/LN/LOG 的 166/167/167 instructions
与 `sub_100013E0` fill helper 已用于实现。直接 numeric AST literal 先落 raw-f32，按末值验证后计算一次并
广播；普通 Series 只跳 leading canonical sentinel，started 后 overflow/domain-invalid/sentinel 都复制
previous raw-f32 output。EXP 的判别为 `abs(f32)*1e-7+x+1e-5 <= 88`；LN/LOG 使用正数 tolerance，
并保留 absolute index 0 的独立起始 gate。没有改 render/trade-event/context/schema；379 系统公式对这三项
为 0 calls，本阶段只补 custom interpreter fidelity。

native-scalars/language focused、完整 build/CTest `139/139`、0 failed（11.32 秒）通过。正式 day5
`EXP(1)` 为全柱 `2.7182817459106445`，`LN(0.5)` 为全柱 `-0.6931471824645996`；同值经
`CLOSE*0+0.5` 构成 Series 时第一柱 null、后续为该 f32 值，`EXP(89)` 全 null。request body 未保留。

同批 Web 只增加 `/protocol/coverage` 本地协议覆盖 consumer；不改变公式或交易状态。本阶段 Svelte
0/0、Web build 453 modules，coverage focused contracts 通过，未跑 full API。当前正式 PID `22240`，
EXE SHA-256 `1ea2ee8694166660aecdb0476f1b9619174e10182b3e71e55ea4bea061a7ac66`，web index SHA-256
`5d90c8cffe68dafcaa0647ef3f7ef9b59118ff7fced8706ec3205eb7257fdb8a`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `20792` 为历史；rollback 为
`output/tdx-tool-formal-23083746-pre-coverage-exp-log-rollback-20260814.exe`，SHA-256
`230837462287aa23136c93598ac4227c5c470a6b3c29155e09d34093af4db1a3`。

## 三角、符号与小数部分标量（2026-08-14）

targeted trigonometric evidence 闭合 ACOS/ASIN/ATAN/COS/SIN/TAN 六个 handler 和 CRT/fill helpers；
fraction-sign evidence 闭合 SIGN/SGN 与 FRACPART。解释器只采纳无外部 buffer context 的语义：
ACOS/ASIN/TAN direct literal type-3、raw-f32 Series、leading sentinel 与 invalid/pole previous carry；
SIGN/SGN 的 ±1e-5 inclusive raw-f32 三态；FRACPART 的 leading-only sentinel、signed 1e-4 adjustment、
native safe i32 和 f32 output。ATAN/COS/SIN 的 Series branch 读取 `Src[6*size]`，没有猜默认 metadata。
证据 hash 分别为 `6e1666d4...f2c9d` 与 `4fb014b0...56d30`；379 系统公式中六三角函数均 0 calls。

native-scalars/language 与完整 CTest `139/139`、0 failed（48.46 秒）通过。正式 day5 literal 输出为
ACOS/ASIN/TAN `1.04719758033752/0.523598790168762/0.546302497386932`，SIGN/SGN `1/-1`，
FRACPART `4294967296`；engine native、body retained=false。本批没有改变 trade-event/context/render/Web，
未跑 full API。

当前正式 PID `16816`，EXE SHA-256
`aedd9b545147943f7d1486f8fea57ebeaa03b5ae446d246faa7ce8e7c94de5a0`，web index SHA-256
`5d90c8cffe68dafcaa0647ef3f7ef9b59118ff7fced8706ec3205eb7257fdb8a`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `22240` 为历史；rollback 为
`output/tdx-tool-formal-1ea2ee86-pre-trig-fraction-sign-rollback-20260814.exe`，SHA-256
`1ea2ee8694166660aecdb0476f1b9619174e10182b3e71e55ea4bea061a7ac66`。

## 共享部署更新：TPBus 115 API / Level2Lab（2026-08-14）

本批没有改变 formula AST、context、render 或交易事件语义；共享 native 变化只把既有 TPBus 115
typed decoder 接入严格内联 POST 与 Level2Lab。115 子体只留 size/SHA-256/有界摘要，不回显 body，
不访问 EventBus/SDK/host/message/network。formula focused 保持通过；完整串行 build 与 CTest
`139/139`、0 failed（49.84 秒），Svelte 0/0、Web build 通过；未跑 full API。

当前 PID `22696`，EXE SHA-256
`8a443ece0a78f59117cd9bc3c93a4c9b330e7041ea92c291b12dc3c53efca2c3`，web index SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `16816` 为历史；rollback 为
`output/tdx-tool-formal-aedd9b54-pre-tpbus-115-rollback-20260814.exe`，SHA-256
`aedd9b545147943f7d1486f8fea57ebeaa03b5ae446d246faa7ce8e7c94de5a0`。

## CONST / ROUND2 raw-f32（2026-08-14）

`output/ida-tcalc-const-round2-targeted-20260814.json` 完整闭合 CONST `sub_1000F3B0` 18 条与
ROUND2 `sub_10038870` 101 条指令，reachable helper=2、frontier=0，SHA-256
`a9a4a50cb18b2aa3840db8c94f1510f065c32465e425636b713bc21ed161a463`。CONST 只取末柱 raw-f32；
ROUND2 只取首柱 precision raw-f32，截整/clamp 0..4 后对全序列应用 f32 scale、符号偏置和 f32
output。379 系统库两者 0 calls，不改变已有系统公式审计计数；CONSTA 与 buffer-dependent
ATAN/COS/SIN 未改。

native-scalars/language focused 和 tdx-tool link 通过。正式 day5 C/P/N 为 `16777216`、
`1.23000001907349`、`-1.23000001907349` 全柱，engine native、body retained=false。本批未改
trade-event/context/render/schema，未重跑 full CTest/API；最近完整 CTest 为 139/139（49.84 秒）。

当前 PID `32380`，EXE SHA-256
`cf4ea0bc7df181f7310fcaedb3d9c53da73ffba5a08c020ef31ef5807640342e`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `22696` 为历史；rollback 为
`output/tdx-tool-formal-8a443ece-pre-const-round2-rollback-20260814.exe`，SHA-256
`8a443ece0a78f59117cd9bc3c93a4c9b330e7041ea92c291b12dc3c53efca2c3`。

## 2026-08-14：CONSTA raw-f32 offset

`output/ida_probe_tcalc_consta_targeted_20260814.py` 生成
`output/ida-tcalc-consta-targeted-20260814.json`（15625 B，SHA-256
`8cab0f00d4fde8e865f965ec048cf465e5f72cbd42c445b827f3b01e7b701c6d`）。证据完整覆盖
CONSTA `sub_1000F360` 36 条与 `__ftol2_sse` 55 条指令，frontier=0。handler 只取 offset 末柱
raw-f32，经 native i32 截整后 clamp；选中 source 以 raw-f32 广播。实现和 scalar focused 锁定
`1.99999999 -> offset 2`、`16777217 -> 16777216`、missing offset→0 与 oversized→oldest。
379 系统库无 CONSTA 调用；ATAN/COS/SIN 的 buffer metadata 分支因生产者证据不足继续保持边界。

native-scalars/language focused 和 tdx-tool link 通过。正式 day5 A/B 为
`11.2600002288818` / `11.25` 全柱，engine native、body retained=false。本批未改 trade-event、
context、render、schema，未重跑 full CTest/API；最近完整 CTest 为 139/139（49.84 秒）。

当前 PID `31856`，EXE SHA-256
`31452ab93214a1ae803c4ddab766cd0f72aff524450a68bee5a1cb829c32d9d1`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `32380` 为上一 CONST/ROUND2 阶段；rollback 为
`output/tdx-tool-formal-cf4ea0bc-pre-consta-rollback-20260814.exe`，SHA-256
`cf4ea0bc7df181f7310fcaedb3d9c53da73ffba5a08c020ef31ef5807640342e`。

## 2026-08-14：IF / IFF / IFN condition 与 RANGE

`output/ida_probe_tcalc_ifn_range_targeted_20260814.py` 生成
`output/ida-tcalc-ifn-range-targeted-20260814.json`（40251 B，SHA-256
`e414d1e1744c9d16826a834d05be81c288f9755b3c56b2eb71082a280b125f39`）。IFN
`sub_1000B750` 96 instructions、RANGE `sub_10016900` 153 instructions，均无 direct helper/frontier；
IF/IFF 的同族规则由既有 IF evidence 闭合。

条件选择现在保持“只由 condition 做 leading gate”，但 exact-zero 比较前先落 raw-f32；started 后
sentinel 仍走 nonzero 分支。IFN 的分支方向保持反向，selected number 写回 raw-f32；字符串 IF 只同步
condition 判定，不改变 StringSeries。RANGE 仅在两个 bounds 同时 leading sentinel 时延迟启动，启动后
value/lower/upper 都按 raw-f32 参与原生双容差条件，内部 sentinel 不再被 generic finite gate 丢弃。
这批不改 trade-event/context/render/schema；IFN/RANGE 在 379 系统源码为 0 direct calls。

native-operators、native-scalars、language focused 与 tdx-tool link 通过。正式 day5 I/J/N/S/R 为
`0.10000000149011612/0.10000000149011612/16777216/1/1`，engine
`tdx-source-interpreter-v1`、body retained=false。未重跑 full CTest/API；最近完整 CTest 为 TPBus 115
阶段 `139/139`（49.84 秒）。当前 PID `14624`，EXE SHA-256
`0c2e62f79911cf82a7c0bbff05c84c0924b2a338b25431a649c23b628ce506a2`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `31856`
为上一阶段；rollback 为 `output/tdx-tool-formal-31452ab9-pre-ifn-range-rollback-20260814.exe`，SHA-256
`31452ab93214a1ae803c4ddab766cd0f72aff524450a68bee5a1cb829c32d9d1`。

## 2026-08-14：ADD / EQUAL / NOT_EQUAL

targeted script `output/ida_probe_tcalc_add_equal_targeted_20260814.py` 生成
`output/ida-tcalc-add-equal-targeted-20260814.json`（63420 B，SHA-256
`64e8386daa893d239c84481dab5772563da44dbd7048d45ef360d9a69fccc3d8`）。ADD/EQUAL/NOT_EQUAL
分别为 147/113/113 instructions；float fill 与 memcpy 两 helper 纳入闭包，frontier=0。

二元加法现在按 raw-f32 读写并传播 canonical sentinel。相等/不等不做 generic finite gate，而是把
解释器 missing 恢复成原生有限 sentinel 后相减：EQUAL 仅在严格 `(-1e-5,+1e-5)` 内为 1，
NOT_EQUAL 在两端点及外侧为 1。因此 missing=missing 为 1、missing<>ordinary 为 1。该修复不改变
trade-event/context/render/schema；379 系统源码剔除字符串/注释后的影响为 ADD 96 formulas/316
occurrences、EQUAL 69/185、NOT_EQUAL 2/2。

新增独立 native-binary test domain；native-binary、language、native-operators、全部 formula-engine
domains 与 tdx-tool link 通过。正式 day5 A/B/F/I/E/N/MM/MV 为
`16777216/0.30000001192092896/1/1/0/1/1/1`，末柱 CLOSE+1=`12.25`，engine
`tdx-source-interpreter-v1`、body retained=false。未重跑 full CTest/API；最近完整 CTest 仍为
TPBus 115 阶段 139/139（49.84 秒）。

当前 PID `2800`，EXE SHA-256
`b8f871b3da1c0ca8a7b939a8a74aa9b96916c99f9e1c718db1d78ddc41ea6d92`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `14624`
为上一阶段；rollback 为 `output/tdx-tool-formal-0c2e62f7-pre-add-equality-rollback-20260814.exe`，
SHA-256 `0c2e62f79911cf82a7c0bbff05c84c0924b2a338b25431a649c23b628ce506a2`。

## 2026-08-14：unary minus compile lowering

`output/native_probe_tcalc_unary_minus.cpp` 在隔离 root 中只调用 TCalc 编译接口并生成
`output/tcalc-unary-minus-compile-probe-20260814.json`（3370 B，SHA-256
`18dcca15710f049f9525c7b28cadf2c0757f32e53ae755deeea367583112ab26`）。compiled nodes 证明
`-CLOSE` 不是 double 直接取负，而是 raw-f32 `-1.0F` 常量与 CLOSE 的 `*`；显式
`-1*CLOSE` 得到相同 graph，`-16777217` 被折为 `-16777216`。production 仍完全离线且不加载 DLL。

runtime 现以独立 `tcalc_negate` 薄委托 `tcalc_multiply`，复用 `sub_10004130` 的 f32 operand/result、
sentinel propagation 和安全 nonfinite 映射。379 系统源码剔除字符串/花括号注释后为 21 formulas/
34 unary-minus occurrences；`%` 无系统调用且无新 handler 证据，明确留边界。native-binary、language、
native-operators、全部 formula-engine domains、tdx-tool link 与正式 day5 smoke 通过；末柱
C/F/M/O=`-11.25/-0.10000000149011612/null/null`，engine native、body retained=false。

本批未跑 full CTest/API；最近完整 CTest 仍为 TPBus 115 的 139/139（49.84 秒）。当前 PID
`28016`，EXE SHA-256 `e56a01e20b2e1a63d6f632a06239588c3afc6cceff8d94f1583dd1e69c3bd0a1`，
web SHA-256 `972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。
PID `2800` 为上一阶段；rollback 为
`output/tdx-tool-formal-b8f871b3-pre-unary-negation-rollback-20260814.exe`，SHA-256
`b8f871b3da1c0ca8a7b939a8a74aa9b96916c99f9e1c718db1d78ddc41ea6d92`。

## 2026-08-14：numeric literal node landing

`output/native_probe_tcalc_unary_minus.cpp numeric` 生成
`output/tcalc-numeric-literal-compile-probe-20260814.json`（1094 B，SHA-256
`ff9dcd7110e35c7d91ddae6c9946aa3936b9f5e91b558b112f46f4bc3dabdad8`）。compiled node 中
16777217 的 dword 为 `0x4b800000`，0.1 为 `0x3dcccccd`；因此 source literal 在进入 handler 前
已经是 raw-f32。runtime 只改 `NodeKind::number` 的执行广播，analysis/parser/schema 未改。

剔除字符串和花括号注释后，379 系统源码命中 330 formulas/3425 literals。全部 formula-engine
domains 通过；旧 overflow-to-i32 tests 改用可编译的有限 `2147483648` 节点保留 handler 边界，避免再把不可正常编译的
overflow literal 当成 ordinary node，STICKLINE WIDTH 则精确锁 `double(0.1F)/4`。tdx-tool link 与正式
day5/day40 smoke 通过：I/D/A/C=`16777216/0.10000000149011612/16777216/
11.350000381469727`，CCI=`-45.719879150390625`，engine native、body retained=false。

未跑 full CTest/API；最近完整 CTest 仍为 TPBus 115 阶段 139/139（49.84 秒）。当前 PID `31728`，
EXE SHA-256 `7738a999e23a8f9d09142791a16062dcd30cd676456deb479d8542633bc04517`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `28016`
为上一阶段；rollback 为 `output/tdx-tool-formal-e56a01e2-pre-numeric-literal-rollback-20260814.exe`，
SHA-256 `e56a01e20b2e1a63d6f632a06239588c3afc6cceff8d94f1583dd1e69c3bd0a1`。

### Numeric-literal 同批参数槽补充

公式记录 active parameter 的 f32 已成为 execution、response `parameters` 与派生 MTM N 的唯一
effective value；direct engine 对超出 finite-f32 的参数报错，HTTP 既有 `1e9` cap 不变。全
formula-engine domains 与 link 复验通过，正式 N/P/Q 均为 16777216，retained=false。本批仍未跑
full CTest/API。

最终 PID `33640`，EXE SHA-256
`8fd3b43478fb823cd35213dfcda385afcc6472dc05ce1b7b7b66dc23fae3c49a`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `31728`
为 literal-only 中间态；rollback 为
`output/tdx-tool-formal-7738a999-pre-parameter-float-rollback-20260814.exe`，SHA-256
`7738a999e23a8f9d09142791a16062dcd30cd676456deb479d8542633bc04517`。

## 2026-08-14：OPEN/HIGH/LOW/CLOSE/VOL/AMOUNT f32 入口

targeted script `output/ida_probe_tcalc_price_field_handlers_20260814.py` 生成
`output/ida-tcalc-price-field-handlers-targeted-20260814.json`（555004 B，SHA-256
`33c32e34369cbc449c4be0e4ceee6f4c39e4087744391e229b0f4e28379fb66a`）。四价 handler 均为
58 instructions，直接从 packed 35B record 的 7/11/15/19 写 output f32；VOL/AMOUNT 根函数为
142/153 instructions，证明既有单位换算后的 result landing，同批未猜两条宿主分类 frontier。

`bind_price_fields` 现对四价、AMOUNT、VOL 与 `__RAW_VOLUME` 做 f32 landing；`read_bars` 对普通股票
VOL 明确先 narrow source、再 `/100`、再 narrow result。顶层 points 原行情值不变。379 库直接引用并集
252 条，分项 formulas/calls 为 63/159、90/180、88/190、203/655、56/133、8/13。

native-binary focused、全部 formula-engine domains、tdx-tool link 通过。真实 day5 evidence
`output/formula-price-field-float-live-20260814.json` 的 SHA-256 为
`0172653f3f39d21b4f48ac5269f4b2592e656f91f92a9016b16b5d8ba3dde78f`；正式最后一柱
O/H/L/C/A/V=`11.229999542236328/11.270000457763672/11.180000305175781/11.25/
848358784/755980.8125`，engine native-cpp、body retained=false。未跑 full CTest/API；完整基线仍为
139/139（49.84 秒）。

当前正式 PID `25212`，EXE SHA-256
`9bb181de4c87bdcc2461ec87c0234828dd48dc68ff4242f4cec291d77062a645`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `33640`
为上一参数阶段；rollback 为 `output/tdx-tool-formal-8fd3b434-pre-price-field-float-rollback-20260814.exe`，
SHA-256 `8fd3b43478fb823cd35213dfcda385afcc6472dc05ce1b7b7b66dc23fae3c49a`。

### Auxiliary record+31 / VOLINSTK f32 addendum

`output/ida-tcalc-auxiliary-field-handlers-targeted-20260814.json` 为 438719 B，SHA-256
`c990604f58f8d0ba2fffbce3f58f51baa0922b72948c7ec5151e972fb5e07b15`。ZSTJJ/QHJSJ/HKSHORTVOL
均从 packed record +31 复制 f32；VOLINSTK 对扩展字段 +23 做 u32→f32，普通无字段分支输出 0。
production 只统一环境落点，不合并三种业务名、不放宽 open-interest/HK-short availability gate。

全部 formula-engine domains、link、真实 IFL9 day5 通过；最后 I/C/Z/Q=
`272336/272336/4608.39990234375/4608.39990234375`，body retained=false。未跑 full CTest/API。
最终 PID `27868`，EXE SHA-256
`bfdc14bc0e4c118c21030dc362c6ae61bc821e4a0e1f2e2f95d93f78c5335bd0`，web SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；PID `25212` 为中间态；
rollback 为 `output/tdx-tool-formal-9bb181de-pre-auxiliary-field-float-rollback-20260814.exe`，SHA-256
`9bb181de4c87bdcc2461ec87c0234828dd48dc68ff4242f4cec291d77062a645`。

## 2026-08-14：标量 context handler raw-f32

脚本 `output/ida_probe_tcalc_scalar_context_handlers_20260814.py` 生成
`output/ida-tcalc-scalar-context-handlers-targeted-20260814.json`（30667 B，SHA-256
`8d3c5a49655fa9cdbf6ae94e4887812ae832938b17792645e65ad3f72dfa68e3`）。CAPITAL
`sub_10026870`、TOTALCAPITAL `sub_10026930`、MINDIFF `sub_100269C0`、MULTIPLIER
`sub_10026A40` 分别为 63/49/45/39 instructions，根函数都写 f32 output。证据只用于确认
source/result landing 与 MINDIFF floor；证券分类 helper、市场 divisor 和 type-105 更深宿主语义保持边界。

实现只在 `formula_scalar_bindings`、`symbols`、`series` 中识别四个精确大写名并做 f32 landing；没有
改全局 context 数字规则。当前财务 CAPITAL/TOTALCAPITAL 在既有 `/100` 语义内改为 source-f32→division→
result-f32；MINDIFF 先收窄 source，再与 `float(1e-5)` 比较。explicit scalar、symbol、逐点 series 与
自动 MINDIFF 都有 native-binary 判别夹具；FINANCE/FINVALUE/DYNAINFO 不受影响。

系统源码扫描得到 CAPITAL 11 formulas/14 refs、MINDIFF 2/3，另外两项 0，去重并集 13。全部
formula-engine domains 与 tdx-tool link 通过。真实 `SZ000001` day800 输出
`output/formula-scalar-context-float-live-20260814.json`（303012 B，SHA-256
`f429c750228c19c35bf4dcdd34fc27b7e374b2f2401dacb139e28ae6ab00735c`），最后一柱
C/T/D=`194056000/194059184/0.00999999977648258`。正式 automatic/explicit/series 三条 POST 均 200，
`execution_mode=native-cpp`、request body 不保留。未跑 full CTest/full API；最近完整套件仍为 139/139。

正式 PID `25268`，EXE SHA-256
`5978ba25e10b4fd71ce78837cb5004c746ba03df07cbd462fbbc891094b8f6e6`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；PID `27868` 为历史，rollback
`output/tdx-tool-formal-bfdc14bc-pre-scalar-context-float-rollback-20260814.exe` 的 SHA-256 为
`bfdc14bc0e4c118c21030dc362c6ae61bc821e4a0e1f2e2f95d93f78c5335bd0`。

## 2026-08-14：FINANCE/FINVALUE/DYNAINFO numbered context

`output/ida_probe_tcalc_numbered_context_handlers_20260814.py` 生成
`output/ida-tcalc-numbered-context-handlers-targeted-20260814.json`（357331 B，SHA-256
`99c1c3e7cfac97d6fccb691693130c0fd81e9f1bc12991374189c05ac6df898e`）。FINVALUE
`sub_10011A00` 169 instructions，FINANCE/DYNAINFO shared `sub_10026B20` 2004 instructions；入口均为
末柱 raw-f32 selector→`__ftol2_sse`，最终均写 float output。没有把 live callback、财务查询或行情快照
生成纳入离线解释器，也没有改变 public context contract。

`numbered_context_binding` 现一次解析末柱 selector，复用既有 `NAME#n` series 并逐点做 raw-f32/sentinel
安全映射；这同时保留 FINVALUE as-of series 的日期对齐。高判别测试用首柱/末柱不同 selector，锁定整段
采用末柱 key；另用 16777217、-16777217、16777219 与 0.1 锁 output landing。系统源码覆盖 FINANCE
46 formulas/112 calls、FINVALUE 4/5、DYNAINFO 20/78，去重 59。

native-binary、context-and-library、全部 formula-engine domains 和 link 通过。真实 CLI 证据
`output/formula-numbered-context-float-live-20260814.json`（16878 B，SHA-256
`8afc4760126bc17acaf402e9a2ec82f400c92a195601de8920cddd256bc4110e`）为 SZ000001/day5，
F/D/V=`16777216/-16777216/16777220`。正式 focused POST 200/200/400，成功响应 body retention=false。
未跑 full CTest/API；最近完整基线仍为 139/139。

正式 PID `36312`，EXE SHA-256
`d56acfa73629e315c21eaddf66c7aee0ce18bb6f6b893af123ccac0bdd56a43f`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`。PID `25268` 为历史；rollback
`output/tdx-tool-formal-5978ba25-pre-numbered-context-float-rollback-20260814.exe` 的 SHA-256 为
`5978ba25e10b4fd71ce78837cb5004c746ba03df07cbd462fbbc891094b8f6e6`。

## 2026-08-14：GPJYVALUE/BKJYVALUE/SCJYVALUE final selector

`output/ida_probe_tcalc_professional_value_targeted_20260814.py` 生成
`output/ida-tcalc-professional-value-targeted-20260814.json`（485653 B，SHA-256
`124fe804cd556272799f8ba1fcf5e3a1e29bc54d201bb80c7cb124fbec7fb013`）。三个 type-174 handler
分别为 195/225/170 instructions；三者都从三个参数的末柱 raw f32 取 selector，随后逐柱写 float
结果缓冲。证据没有把 live host callback 或专业记录生成带进离线解释器。

`professional_value_binding` 现只选择一个末柱 `NAME#data#field#date_mode` binding，并复用共同的
raw-f32/sentinel 安全输出。高判别测试让首末柱三元组不同，同时用 16777217、-16777217 和 0.1
锁定整段选择与 f32 landing。系统覆盖 GPJYVALUE 11 formulas/24 calls、SCJYVALUE 11/32、
BKJYVALUE 4/4，去重 15。

native-binary、context-and-library 与 link 通过。真实证据
`output/formula-professional-context-final-selector-live-20260814.json`（19106 B，SHA-256
`d0145fc95476fbdc81eaf142e4e3cf86ba4a6053734c0a29a67eb4b1ebb61f56`）为 SZ000001/day20，
GPJYVALUE 末值 `477634.6875`；正式自动 context、显式 f32 覆盖、未公开 selector 三合同为
200/200/400。未跑 full CTest/API，最近完整基线仍为 139/139。

正式 PID `480`，EXE SHA-256
`1300851173dd12c56f969b421b60ed1dbe2b0e4919b14cc4ad7e58c49ad7a1fc`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`。PID `36312` 为历史；rollback
`output/tdx-tool-formal-d56acfa7-pre-professional-final-selector-rollback-20260814.exe` 的 SHA-256 为
`d56acfa73629e315c21eaddf66c7aee0ce18bb6f6b893af123ccac0bdd56a43f`。

## 2026-08-14：STRCMP final string-handle semantics

`output/ida_probe_tcalc_strcmp_targeted_20260814.py`（SHA-256
`12e65f64d135a833bfea16e0e1d86a5e66b88f28e511cfdc234b041146b2b6cf`）生成
`output/ida-tcalc-strcmp-targeted-20260814.json`（63500 B，SHA-256
`8e1d87490bbadb1f202e24ec8e00740669b050f138aadfad6dcc9e652a610b2f`）。根 handler
`sub_1000F200` 为 108 instructions：从两路参数缓冲末项读取 raw-f32 句柄，截整后通过
`sub_10070DC0` 各取一次字符串，执行一次 `strcmp`，再把单一 0/1 写满输出。句柄小于 1 时原生写满 0；
当前解释器只接收已成功求值的 StringSeries，因此安全对应路径是比较两路 `.back()` 后广播。

生产修改只在 `formula_runtime.cpp` 的 STRCMP 分支。动态字符串 IF 高判别夹具把首/中/末柱选值设为
不同字符串，锁定末柱 `Y` 时全段 S=1/T=0；常量与既有 string-builder 合同仍通过。系统源码精确
覆盖 R标记数/G标记数 2 formulas/32 calls，均为 CODE 对常量代码，不改变现有用户可见布尔结果。

native-binary、language 与 tdx-tool 串行构建通过。真实
`output/formula-strcmp-final-handle-live-20260814.json`（13874 B，SHA-256
`d16532698d4adba82612d95044db6ac00d579da9316e312dde49475058c69513`）为 SZ000001/day5，
S/T 全数组 `[1,1,1,1,1]`/`[0,0,0,0,0]`。正式末柱 Y、末柱 N、错误 arity 三条 POST 为
200/200/400，成功响应 `request_body_retained=false`。未跑 full CTest/API，最近完整 139/139 仍归
TPBus 115 阶段。

正式 PID `4560`，EXE SHA-256
`a005f2fb1e60580087a4f1217e352da296907fd6db100e0a5e20b071a0e120d8`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`。PID `480` 为历史；rollback
`output/tdx-tool-formal-13008511-pre-strcmp-final-handle-rollback-20260814.exe` 的 SHA-256 为
`1300851173dd12c56f969b421b60ed1dbe2b0e4919b14cc4ad7e58c49ad7a1fc`。

## 2026-08-14：SIGNALS_QS final selectors and sparse fill

`output/ida_probe_tcalc_signals_qs_targeted_20260814.py`（SHA-256
`38fe5a843e034142f1e219ac42e53c281b3d3b1a130a61fb2cdbdf6f3ea29396`）生成
`output/ida-tcalc-signals-qs-targeted-20260814.json`（36690 B，SHA-256
`6a5da89e4e86e94bf5ee509fae5d277c3afa475d68ca8e03009a4cf9a8f3e620`）。原生
`sub_100111B0` 为 173 instructions：signal id 与 mode 均读取参数缓冲末柱 raw f32 并截整；type-35
callback 返回排序的日期/value 记录，匹配值写 f32，缺记录时 mode 1 复制前一 raw output、mode 2 写 0，
其他 mode 保持 canonical missing。host callback 与记录来源没有被离线模拟。

`broker_signal_binding` 保留既有 `SIGNALS_QS#id#mode` 公共 key，只改变 selector 选择、f32 落点和
稀疏填补；输入仍必须由 caller 显式提供，分析器继续拒绝动态 selector 作为公开 context contract。
高判别测试另外直接进入 evaluator，用前后柱不同 selector 锁定末柱选择。系统覆盖红绿波段、撑压信号、
R标记数、G标记数、形态大师、主力密码共 6 formulas/13 calls。

context-and-library 与 link 通过。真实
`output/formula-signals-qs-native-semantics-live-20260814.json`（15807 B，SHA-256
`76fc655453dd8ff383139b5d4284d1003fa30d677df9fd2a9aace3311cc9ae4f`）为 SZ000001/day5，
D/C/Z=`[16777216,0.100000001490116,-16777216,3,4]`、`[null,5,5,5,7]`、`[0,5,0,0,7]`。
正式显式 context、缺 binding、动态 selector 三条 POST 为 200/400/400，成功 body 不保留。未跑 full
CTest/API；最近完整 139/139 仍归 TPBus 115。

正式 PID `34560`，EXE SHA-256
`b30373b159169616839e4c312afa70c5b271bec21ad0e3f7900af1f2c4af6760`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`。PID `4560` 为历史；rollback
`output/tdx-tool-formal-a005f2fb-pre-signals-qs-native-rollback-20260814.exe` 的 SHA-256 为
`a005f2fb1e60580087a4f1217e352da296907fd6db100e0a5e20b071a0e120d8`。

## 2026-08-14：L2_AMO final selectors and type-168 rows

`output/ida_probe_tcalc_l2_amo_targeted_20260814.py`（SHA-256
`13df6f7f347247abd1afb44a9963c5c0c1f2c79e533011716bf070e085d33563`）生成
`output/ida-tcalc-l2-amo-targeted-20260814.json`（464291 B，SHA-256
`e5f8699a5d65fe0b99e69338684d090e0e678d9f4416716b81ef53870d33286e`）。根 handler
`sub_10036EE0` 为 56 instructions：first/second 均从末柱 raw f32 截整并按 unsigned `0..3` 校验；
内部 type-168 状态存在时，从每个 184-byte row 的 `72 + 4 * (second + 4 * first)` 复制 f32。

`level2_amount_binding` 保留既有 `L2_AMO#first#second` public key，只改变 final selector 与 raw-f32
落点；输入仍必须由 caller 显式提供。宿主 type-168 callback、记录生成、Level2 获取、凭据、网络和授权
均未模拟。系统覆盖 ZJLX、ZJQDL、ZJBY、SUPAMO、SUP排序共 5 formulas/34 calls。

context-and-library 与 link 通过。`output/formula-l2-amo-final-selector-live-20260814.json`（12491 B，
SHA-256 `2ea9cbd19821e42d5181c3494c16358a8e47ee1ad7808e1e359e38fa2532cc24`）使用真实
SZ000001/day5 K 线和明确的合成 caller-owned L2 context，得到 A=
`[16777216,0.100000001490116,-16777216,null,4]`；它不代表真实 Level2 数据。正式显式 context、缺
binding、动态 selector 三条 POST 为 200/400/400，成功 body 不保留。未跑 full CTest/API；最近完整
139/139 仍归 TPBus 115。

正式进程实例 PID `4560`，EXE SHA-256
`88c48ea38f0762616d1daf05b8a30637de56363b7447eb982f06a52040a3162e`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`。PID `34560` 为历史；更早
STRCMP 进程也曾使用 PID `4560`，属于 PID 复用。rollback
`output/tdx-tool-formal-b30373b1-pre-l2-amo-native-rollback-20260814.exe` 的 SHA-256 为
`b30373b159169616839e4c312afa70c5b271bec21ad0e3f7900af1f2c4af6760`。

## 2026-08-14：RGB raw-f32 channel conversion

`output/ida_probe_tcalc_rgb_targeted_20260814.py`（SHA-256
`155c0d86f08ef77de9da324d4c979185d8e13dcfc0a3130a842414d337d3a9f3`）生成
`output/ida-tcalc-rgb-targeted-20260814.json`（14035 B，SHA-256
`9cefca0bbf8df9783acc4a4ad5a99c01a345e71549ef61ad9a34a8b357dfdb93`）。根 handler
`sub_1000A6A0` 为 84 instructions、0 direct callees/frontier。三通道分别做 raw-f32、x87 toward-zero
i64、low-u32；只有 unsigned `<255` 时保留 low8，否则为254。打包结果按 R/G/B 低到高字节写 f32。

生产 helper 对 finite-f32 区间安全截整，对 missing、NaN、f32 overflow 显式模拟 integer-indefinite；
没有依赖 C++ 未定义浮点转整数，也未改 render IR。系统 RGB 覆盖 6 formulas/18 calls，NXX、QSDK、
RGBAND、DDZ、WAVEKX 共 5/9 使用255。focused 另锁 `RGB(255,-1,256)=0xFEFEFE`、小数截整与
sentinel/overflow。

render-ir 与 link 通过。真实 `output/formula-rgb-native-channel-live-20260814.json`（17257 B，
SHA-256 `fa92a2f21a073f8657218b7ee7a1b05ebd980e757a86edeffe0b7d0120d109b2`）为
SZ000001/day5，R/N/F 全段为254、16646654、197121。正式 native/missing/arity 三条 POST 为
200/200/400，成功 body 不保留。未跑 full CTest/API；最近完整 139/139 仍归 TPBus 115。

正式 PID `33964`，EXE SHA-256
`7194ec950edc472d4f9fd018d0f71106b6dea51f3bc1e7a6bf58ccebb793af68`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`。PID `4560` 为历史；rollback
`output/tdx-tool-formal-88c48ea3-pre-rgb-native-rollback-20260814.exe` 的 SHA-256 为
`88c48ea38f0762616d1daf05b8a30637de56363b7447eb982f06a52040a3162e`。

## 2026-08-14：共享部署身份（Level2Lab 有界展示）

本批仅修改 Level2Lab 的 Svelte 结果展示与授权捕获保留策略，不改公式解释器、后端或 schema。验证为
`npm run check` 0 errors/0 warnings、`npm run build` 成功且仅有既有 >500 KiB chunk 警告；未跑
CTest/API，最近完整 139/139 仍归 TPBus 115。

正式 PID `19348`，EXE SHA-256
`7194ec950edc472d4f9fd018d0f71106b6dea51f3bc1e7a6bf58ccebb793af68`，web index SHA-256
`378da4337ceb42c55444a2ccd4aa3c8f78735a9b8e79517fb962258c892ada00`；health 匹配，native true、
python false。PID `33964` 与 web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee` 为历史身份；Level2 细节见同批
offline-tooling 日志。

## 2026-08-14：共享部署身份（TQLEX/PBRPC/TPool 前端）

本批只修改 TQLEX/PBRPC 的有界表格展示与 TPool XML 浏览器驻留策略，不改公式解释器、后端或
schema。`npm run check` 0 errors/0 warnings，`npm run build` 成功且仅有既有 chunk warning；
`tqlex/pbrpc/tpool` 路由均为 200 并命中新 asset。未跑 CTest/API，139/139 仍归 TPBus 115。

正式 PID `10812`，EXE SHA-256
`7194EC950EDC472D4F9FD018D0F71106B6DEA51F3BC1E7A6BF58CCEBB793AF68`，web SHA-256
`E9FA36DC240FE4B2EB4CA88872008324A0091F7A0DC4F1F1FE43B99B391CB75C`；health 匹配、native true、
python false。PID `19348` 与 web SHA-256
`378DA4337CEB42C55444A2CCD4AA3C8F78735A9B8E79517FB962258C892ADA00` 为历史身份。

## 2026-08-14：共享部署身份（TQLEX/PBRPC 预算）

本批调整 TQLEX/PBRPC 查询预算，不改公式解释器或公式 schema。正式 PID `36440`，EXE SHA-256
`C120DBFAAC9EF67E98B2B4697B9A42AA0DCD6B3CFB655ED65684C3783F89DD38`，web SHA-256
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`；health 匹配，native true、
python false。PID `10812` / web
`E9FA36DC240FE4B2EB4CA88872008324A0091F7A0DC4F1F1FE43B99B391CB75C` 为上一正式身份；文件锁期间的
PID `14592` 未监听、非正式且已停止。rollback 为
`output/tdx-tool-formal-7194ec95-pre-tqlex-pbrpc-budget-rollback-20260814.exe`，SHA-256
`7194EC950EDC472D4F9FD018D0F71106B6DEA51F3BC1E7A6BF58CCEBB793AF68`。

## 2026-08-14：HK FINANCE HTTP/CLI parity 与缺绑定预分类

根因不是 HK selector 不支持，而是 HTTP audit 的 aggregate 落后于 CLI：扩展市场分支没有收集已经由
`is_tcalc_hk_finance_selector` 证明的 bindings，context builder 因而没有物化 FINANCE#3/#7/#34/#42。
HTTP 现复用同一规则。正式 `31:00700` 结果为 380 total、244 passed、0 errors、0 unreported；受影响的
HSL/HSCOL/ZJL/A011/B008/B009/C113/C114/C128/C129/C130/C133/WISEWAY 共 13 条均有最新数值。

解释器同时增加防御门槛：只对 evaluator 确实按键消费的 `FINANCE#/FINVALUE#/DYNAINFO#` 检查 group、
scalar、symbol 或 series 是否已物化。缺失时在 evaluate 前归 `context_unavailable`，并返回完整 required
与精确 unavailable；HK FINANCE#3/#7 focused 锁缺失 2 项无 error、补齐后 passed 2。

coverage recon 改为要求精确 4 条 ZTPRICE/DTPRICE host-type-120 fidelity gate，而不是错误的 0 degraded。
当前 loaded 380=376 safe+4 gated，系统379=375+4；56 presentation surrogates 分开。focused C++ 全通过，
price annotation 合同同时锁 K-line precision 与 constructor default 两种真实来源。正式公式 API 3/3；
full API 212/225、226 requests，三个公式失败均恢复，余 13 项为既有数据基线。报告 SHA-256
`FBEFB32836DF45B7F5E6A7CDBABDB2ED27EAF46E3D9E42D26DD7072258587658`。未跑 full CTest。

正式 PID `33136`，EXE `228F5261F52D9D19B2E33B08D1A5203C79DC42441FD899629BA7FACBAFB3ACD6`，web
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`；PID `21544` 为中间历史。rollback：
`output/tdx-tool-formal-76eb2da6-pre-price-annotation-contract-rollback-20260814.exe`，SHA-256
`76EB2DA6232693C1679F37A01C15FCCC5FFA64E20CDEA33D740D15DF804F869D`。

## 2026-08-14：共享 recon 部署身份

本批只把 13 个滚动市场数据合同从静态数量快照改为单位、raw/source、集合差分和 family sum 不变量；
公式解释器与公式 schema 未改。`tdx-recon-contract-tests` 通过，正式 full API 225/225；本批未跑 full
CTest，139/139 仍归 TPBus 115。

正式 PID `30820`，EXE `1D189E40A11171A098C58D449E5288AEF1537C9E001AB6EF9FDEF87185DBEF9C`，web
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`；PID `33136` 为历史。rollback：
`output/tdx-tool-formal-228f5261-pre-reconciliation-rollback-20260814.exe`，SHA-256
`228F5261F52D9D19B2E33B08D1A5203C79DC42441FD899629BA7FACBAFB3ACD6`。

## 2026-08-14：ZTPRICE / DTPRICE 从 fidelity gate 转为精确显式上下文

定点证据 `output/ida-tcalc-refx-limit-price-targeted-20260813.json` 闭合 Z/D handlers 与纯数值 helper
`sub_100702A0`；`output/ida-tdxw-type120-security-class-targeted-20260813.json` 又证明 type120 +31 来自
`sub_5A3810(security_record)`，不是 TNF category/precision 的直接复制。实现因此接受两个精确 u16 raw binding，
保留末柱 rate、class==3 的 1000 精度、market word 44/2 特殊分支、0.003/0.997/0.503000020980835 bias、Z
的两次转换及中间 float。缺 binding 或非 u16 会拒绝，绝不由规范化 market/code 推导。

分析器移除 `HOST_TYPE120_CONTEXT_UNRESOLVED` 数值 taint，并把两 raw key 同时列入 external、required、
unavailable 与 explicit required；B007/C128/C129/C130 变为 numeric-safe / explicit-context，系统覆盖
379/379、当前覆盖 380/380，degraded 0、explicit 24。recon 合同同步拒绝旧 375+4 快照。另将 HK 自动
FINANCE/FINVALUE/DYNAINFO 缺失键在 audit 求值前精确归为 `context_unavailable`；synthetic HKFIN3/HKFIN7
锁定缺失时 0 errors/2 unavailable，补齐时 2 passed。

formula-engine 全 domain、recon focused 均通过；stage 和正式 full API 都是 225/225。正式 full 报告 SHA-256
`DAEBA5969D839E3D10266EBF1B49393D6188E54A395C62765559FDC38C2E7134`，focused SHA-256
`59877DEDD7B37077C91E79848D1F8D081AE42A84DF70372704509BD4C627A7FE`。实盘 `sz/000001` day 800 的末柱
Z/D 为 12.380000114440918 / 10.130000114440918。未跑 full CTest，139/139 仍归 TPBus115。

正式 PID `31280`，EXE `1D6354054F73F47BDB4A7C17E5F089727EF941C9D7294F3A66E798B7CA02B0A6`，web
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`；PID `30820` 为历史。rollback：
`output/tdx-tool-formal-1d189e40-pre-zd-exact-context-rollback-20260814.exe`，SHA-256
`1D189E40A11171A098C58D449E5288AEF1537C9E001AB6EF9FDEF87185DBEF9C`。

## 2026-08-14：Z/D raw context-template 标量形状

显式上下文模板现在把 `HOST_TYPE120_SECURITY_CLASS_RAW` 与 `HOST_EVALUATOR_MARKET_WORD_RAW` 放到
`formula_scalar_bindings`，各一个 null/u16 占位；即便按 800 根 K 线生成模板也不会出现 1600 个重复空值。
L2、券商私有与账户序列继续使用精确 DATE|TIME。模板 metadata 区分 scalar/series count、source kind 与
value shape，且继续声明不获取、推导或伪造调用方 raw。

formula/server/catalog/recon focused 通过，Svelte 0/0；stage full API 226/226、正式 focused 5/5。正式 B007
template 为 scalar=2/series=0，`sz/000001` day5 末柱 Z/D 保持
12.380000114440918 / 10.130000114440918。full 报告 SHA-256
`A08FD619BE8C993C25584139D80F331ABFBF7578364ED582F959F282020F8954`。本批未跑 full CTest，139/139
仍归 TPBus115。

正式 PID `2536`，EXE `B600B503FC31250019023E1803972149A05F5FE55B737BF226295CF9070755A4`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；PID `31280` 为历史。rollback：
`output/tdx-tool-formal-1d635405-pre-context-template-scalar-rollback-20260814.exe`，SHA-256
`1D6354054F73F47BDB4A7C17E5F089727EF941C9D7294F3A66E798B7CA02B0A6`。

## 2026-08-14：scalar-only 模板不再取 K 线

HTTP/CLI 在模板分析后仅为实际存在的 series bindings 获取 K 线。B007/C128/C129/C130 这类只需两个 host
raw u16 的公式即使传 market/code，也返回 `kline_fetch_skipped=true`、`bar_count=0`，不发行情请求；ZJLX
等序列模板继续从真实 K 线生成 stamps。CLI 用 `sz/999999` 的判别 smoke 已锁定该边界。

server/catalog/recon focused 通过，stage full API 226/226、正式 focused 5/5；full SHA-256
`645B28765465F95F0276716D2E5E178EA76226C40F64685473B51A46883406EF`。未跑 full CTest。
正式 PID `9092`，EXE `559C785798C06C9812D191C0304F28D5AB9C02670BE7D15C90783A8559F7E8C5`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；PID `2536` 为历史。rollback
SHA-256 `B600B503FC31250019023E1803972149A05F5FE55B737BF226295CF9070755A4`。

## 2026-08-14：ATAN/COS/SIN numeric raw-f32

ATAN/COS/SIN 现按 targeted handler 实现 direct literal 末值 raw-f32 广播和普通 numeric Series 逐柱
raw-f32 operand/result；missing 逐点保留。`COS(16777217)` 的原生判别值为 `0.62632298469543457`。
handler 的 `Src[6*size]>0` 分支属于 `6*size+2` float 复合缓冲透传，当前解释器无对应值类型，仍不猜写。
379 系统公式三项均 0 calls，属于 custom formula 补齐；trade-event/context/render/schema 未改。

native-scalars/language focused、tdx-tool link 和真实 day5 CLI/正式 POST 通过；末柱 A/C/S/L 为
`1.4821404218673706 / 0.25168964266777039 / -0.96780800819396973 / 0.62632298469543457`。
未跑 full CTest/API，139/139 仍归 TPBus115。正式 PID `22988`，EXE
`A04ECEEFA4155FDF5662434A64477A51D076873CB0624BB03A58E3943AECAE7C`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；PID `9092` 为历史。rollback
SHA-256 `559C785798C06C9812D191C0304F28D5AB9C02670BE7D15C90783A8559F7E8C5`。

## 当前解释器阶段完整回归（2026-08-14）

390 个 TCalc 静态注册名已重新分类对账，未发现新的证据闭合纯数值漏项；旧审计候选 `INSORT/INSUM` 等
已由当前自动上下文实现覆盖。HK 缺少实际物化的 `FINANCE#/FINVALUE#/DYNAINFO#` 时在 evaluator 前归为
精确 `context_unavailable`，不再进入 error；focused fixture 同时锁定补齐后的 passed 路径。

全目标串行构建通过，完整 CTest 140/140、0 failed、32.87 秒；测试夹具唯一 GCC 15 warning 已以等价定长
LE 写入消除，formula-engine 全域与 context aggregate 随后通过。正式 HK/A 股/期货审计分别为 241/331/230
passed，均 errors=0、unreported=0；A 股精确保留 24 条 context unavailable。本批未跑 full API。
正式实例无需重启，仍为 PID `22988`、EXE
`A04ECEEFA4155FDF5662434A64477A51D076873CB0624BB03A58E3943AECAE7C`、web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`，health native true / Python false。

## 共享 TPool root-relative 部署身份（2026-08-14）

本批没有改变公式求值语义；TPool HTTP catalog/evaluate 的本地文件身份改为 TDX 根相对且拒绝绝对路径。
formula-engine/context focused 继续通过；schema 变更后的完整 CTest 140/140、0 failed、45.82 秒，未跑 full
API。正式 PID `12312`，EXE `37A639B5FD0501D7156DF3977928E55C94F643B1F431466F7A4EC99BFB972986`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；PID `22988` 为历史。rollback
SHA-256 `A04ECEEFA4155FDF5662434A64477A51D076873CB0624BB03A58E3943AECAE7C`。

## 共享 TPBus 4650 preflight 部署身份（2026-08-14）

本批未改变公式语义。Level2 仅新增 CLI-only 4650 raw-shape/caller-gate 预检，完整宿主状态投影仍不闭合；
全目标构建与 CTest 141/141（0 failed、11.36 秒）通过，未跑 full API。正式 PID `18344`，EXE
`365145AE97F6C78CFE8B6DECB08B53F9C75DEA1D222917C90325C0F060BBA172`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；PID `12312` 为历史。rollback
SHA-256 `37A639B5FD0501D7156DF3977928E55C94F643B1F431466F7A4EC99BFB972986`。

## 自动财务上下文分类与共享部署（2026-08-14）

审计器只对 evaluator 会按精确键读取的 `FINANCE#/FINVALUE#/DYNAINFO#` 做物化前置检查；缺键时保留完整
required 集，并把精确缺集写入 `context_bindings_unavailable`，status 为 `context_unavailable`、errors 不增。
group、formula scalar、symbol、series 等 evaluator 已支持的物化形态继续正常执行；显式 Level2/account/private
binding 仍不自动补值。focused 锁定缺 FINANCE#3/#7 与补齐后的 passed 两条路径。

正式 `31/00700` 700 bars 全库 reported 379、passed 240、errors 0、context unavailable 0；正式
`sz/000001` 800 bars reported 379、passed 329、context unavailable 25、errors 0。formula context-and-library、
JSN variants focused、全 build 与 CTest 141/141（0 failed、46.38 秒）通过；未跑 full API。正式 PID `6852`，
EXE `D1E041DD439256E8A86081DDD6682C42755E5C4554109A70E3FFF0CFB7288708`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；PID `18344` 为历史。rollback
SHA-256 `365145AE97F6C78CFE8B6DECB08B53F9C75DEA1D222917C90325C0F060BBA172`。

## 共享统一 UTF-8 / 数字时区部署身份（2026-08-14）

本批未改变公式解释器、trade-event 或上下文语义；共享 native 输出统一使用 code-point 安全前缀与 ASCII
数字时区。native/cloud/JSN/recon focused、全 build、CTest 141/141（0 failed、59.13 秒）以及正式 API
focused 4/4 通过，未跑 full API。正式 PID `34252`，EXE
`C1B2B845AB375956CF48C6653E344070C0B2FECF899C2FEB531AA7DC91529A36`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、Python
false。PID `6852` 为历史；rollback SHA-256
`D1E041DD439256E8A86081DDD6682C42755E5C4554109A70E3FFF0CFB7288708`。

## 公式解释器完成性审计（2026-08-14）

当前 379 系统 + 1 用户公式为 380/380 source/syntax/numeric-safe/presentation-faithful，degraded numeric 与
unsupported presentation 均 0。`allow_future` 的 SZ 000001/800 bars 审计 reported 380、passed 351，余项仅
24 caller-owned explicit context、4 market inapplicable、1 period inapplicable；errors、unreported 与 automatic
context unavailable 均为 0。能力/运行报告 SHA-256 分别为
`49B08451CD009D2717CC44B03795C8EE6B6A4393EE4DC98A4CC67162872763DD` / 
`E6378DE8C5DF9AC6A2FC435296063F7A199773193A80036E41283B16CD427310`。显式 SIGNALS/L2/DDE/type-120 raw
继续由调用方提供，不伪造授权输入。

本审计没有改变解释器代码。当前 build/CTest 141/141（0 failed、59.13 秒），Svelte 0/0、web build 成功，
正式 full API 226/226；报告 SHA-256
`068929D55743A628786011C6C6A74DEDB5C4034E094E71C7D8870DFA474D8F42`。正式 PID `34252`，EXE
`C1B2B845AB375956CF48C6653E344070C0B2FECF899C2FEB531AA7DC91529A36`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、Python false。

## 授权捕获、未来重放与 AUTOFILTER 决策轨迹（2026-08-14）

公式上下文模板现在可与调用方合法捕获严格合并。捕获 schema 要求 ownership 确认，导入器按模板逐项
校验 scalar/series binding 和 `DATE|TIME` stamp；默认不允许缺值，partial 模式只把精确缺口列出。
CLI、POST 与 FormulaLibrary 使用同一 typed domain，输出仍是既有显式上下文，不增加隐式数据源。

未来函数增加 successive-prefix replay：先计算基线 prefix，再逐根追加观测柱，只比较上一 prefix 已存在
的点。每个 repaint event 保留 output、target stamp/index、observed stamp/index、previous/current value 和
age；新追加点固定不计 repaint。该模块显式 read-only，且不把 future formula 开放给 scan/backtest。

AUTOFILTER v1 状态机没有扩充动作集，而是补充解释层。`decision_trace` 以 bar outer / source statement inner
顺序复制每个 raw candidate 的 condition/price，并附加 accepted、position before/after、position changed、
effective action、composite 与 rejection reason。拒绝原因限 `already-long/short`、`requires-flat/long/short`；
接受项 reason 为 null。禁用 marker 时轨迹为空。网页独立组件只在 details 展开后 stringify 最近 100 条，
避免大序列常驻 DOM。

验证包括 formula language、future replay、recon focused，Svelte 0/0，生产 web build 与真实 POST：6 bars
得到 7 decisions、accepted 4、rejected 3、position changes 4、final flat，trade IR 的 execution/order/network
均 false。context-import 与 future-replay 阶段 full API 分别 228/228、229/229；新增 AUTOFILTER contract 后
只跑 focused 1/1，未重跑完整 230 contracts 或 CTest。
最终 context-import/future-replay/AUTOFILTER/formula-route 组合合约为 4/4，报告 SHA-256
`35E135A9B8BEF79B7A2D424FBD28C69DB091C493A6B2D61DB77C3E294AFD34CD`。工作树 EXE/web SHA-256 为
`9055F1ED3EC2C1155B72ECA00FF4ACC74EF5B58BC0F65B388747330A03E01951` /
`E29140C04EEFA00067A40A1AA27754CE5D1B367912875A41D855FDF9781B3964`，未部署正式服务。
