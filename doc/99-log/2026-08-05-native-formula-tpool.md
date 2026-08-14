# 原生指标计算与 TPool 规则求值

## 本轮结果

- 纯 C++ `formulas calculate` 从 5 类扩展到 13 类：MA、MACD、KDJ、RSI、
  BOLL、CCI、WR、BIAS、DMA、MTM、ROC、TRIX、ATR；
- MA/RSI/BOLL 输出名改为 TCalc 槽位名，并固定输出顺序；公式库同时导出
  参数范围、步长、默认值和输出槽；
- 支持 1/5/15/30/60 分钟和日/周/月周期，可通过多页取得历史预热数据；
- 新增 `/api/v1/formulas/calculate` 和 Svelte 指标验证台；
- 新增纯 C++ `pool inspect`，解析 TPool XML 的 flow、func、stk；
- 从 `TPool.dll.i64` 恢复 `noperate=0..9` 的比较、交叉、拐点和排名语义；
- `pool evaluate` 新增 5/6/7 跨证券排名，按值降序、同值后输入者优先；
- flow 检查新增 cell 归属、8 类启动时机、3 类循环方式、根/汇节点、悬空边
  和环诊断；
- 新增 ST/停牌/退市排除：ST 与退市使用活动证券目录名称/存在性，停牌使用
  所选证券最新日线日期和零成交量交叉判断；无法取得证据时返回
  `filter-unavailable`，不会默认为通过；
- 求值改为按 `stk.cell_id → 本 cell 的 func → 启用 flow` 组合，输出无环图的
  只读拓扑投影；不再把每只股票套用到所有 cell 的规则上；
- 新增 `pool watch`，以 JSONL 输出规则命中和 flow 候选的进入/退出变化；
- `pool evaluate/watch` 已接入 `tdx-source-interpreter-v1`：默认从 TCalc 恢复
  公式库，也可用 `--library` 指定导出 JSON；兼容规则不再限于旧的 26 类
  手写指标，并可复用财务、快照、对应指数和市场广度只读上下文；
- 定时、循环、`emptyps/clr`、宿主回调和池写回仍不执行。

## 真实数据验证

平安银行日线 MACD：

```text
80 points, 2026-04-10 .. 2026-08-05
DIF=0.2092, DEA=0.1673, MACD=0.0838
```

平安银行 30 分钟 KDJ（2 页）：

```text
160 points, 2026-07-08 14:00 .. 2026-08-05 13:30
K=4.73, D=9.24, J=-4.27
```

TPool 结构样例包含深市 `000001` 与沪市 `600000`，规则为日线
`MACD.DIF > 0`。两只证券均返回 `status=evaluated` 且命中。该验证只证明
单规则求值，报告没有把它描述成完整 flow 池结果。

新增排名样例包含 4 只证券，按日线 CCI 执行 `top 2`。最新真实结果为
`SH600000` 与 `SZ000002` 二者命中并沿 `cell 1 → cell 2` 产生 2 个转移候选；结果同时返回从高到低名次、
从低到高名次、排名总体数量和总体是否被 `--limit` 截断。

额外用旧手写子集之外的 `UDL` 构造 TPool 规则，平安银行 120 根日线由源码
解释器返回 `UDL=11.3282`，规则状态为 `evaluated`，只读 flow 投影成功。

## 安全与兼容边界

- `TCalc.dll/TPool.dll` 都是 32 位，而统一工具是 64 位；不在进程内加载；
- TCalc 的公开计算路径依赖宿主行情回调，不能把 DLL 当作接受 OHLC 数组的
  独立函数库；
- 系统恢复公式由源码解释器执行；自定义公式只有在提供可解析正文时才可执行，
  未来函数和未验证外部字段仍会阻止股票池规则进入运行态；
- TPool 求值不启动线程、不回写 XML、历史、板块或池状态；
- `compatibility.safe_to_execute` 固定为 `false`，只有
  `safe_to_evaluate_individual_rules` 可随兼容规则出现而为真；
- 流程投影是从 XML 结构恢复出的只读拓扑语义，输出固定标注
  `runtime_semantics_verified=false`；不能等同原版 worker 的定时、循环和
  状态化迁移。
