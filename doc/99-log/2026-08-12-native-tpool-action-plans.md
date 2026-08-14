# TPool `psatt` 动作策略与只读执行计划

## 目标

补齐股票池 XML 中 `psatt` 的结构化解析，并在规则投影和状态机推进时给出原版
宿主将要执行的动作。实现必须保持通达信目录只读，不调用原版 worker、宿主回调
或 Windows 声音/UI 接口。

## DLL 证据

`TPool.dll` 中恢复出的序列化模板为：

```xml
<psatt bdel="%d" ndelnum="%d" ndeltype="%d" baimpool="%d"
       bsound="%d" nsoundtype="%d" soundfile="%s" btip="%d"
       bsavetoblock="%d" blockfile="%s" bclearblock="%d" bsavehis="%d"/>
```

对应结构总长 325 字节：

| 偏移 | 字段 |
| ---: | --- |
| 0 | `bdel` |
| 1 | `ndelnum`（4 字节） |
| 5 | `ndeltype`（4 字节） |
| 9 | `baimpool` |
| 10 | `bsound` |
| 11 | `nsoundtype`（4 字节） |
| 15 | `soundfile`（256 字节） |
| 271 | `btip` |
| 272 | `bsavetoblock` |
| 273 | `blockfile`（50 字节） |
| 323 | `bclearblock` |
| 324 | `bsavehis` |

`sub_10019CC0` 负责 XML 保存，`sub_1001E610` 和 `sub_10047890` 覆盖 XML
读取，其中 `sub_1001E610` 同时进入运行时动作路径。对策略表访问器
`sub_10013FD0` 的完整调用枚举进一步定位到 `0x10020e5e`：运行时记录的
`+16/+17/+21` 分别读取 `bdel/ndelnum/ndeltype`。只有开关非零且窗口大于零时才
遍历历史；类型 `0/1/2/3` 的阈值分别为 `86400/3600/60/1 * ndelnum` 秒，其他类型
保留记录。每条历史记录长 85 字节，日期/时间位于 `+25/+29`，达到阈值后从向量
移除并调用 `sub_10019CC0` 持久化。

同一运行时的 `baimpool` 偏移 `+25` 也不是纯拓扑标志。命中记录非空时，
`0x100222af/0x100236c2` 调用 `sub_10018380`，在
`tpool\<池>\<节点>\<YYYYMMDD>.log` 中读写 XML。记录元素为 `stk`，字段是
`market/code/indate/intime/inprice`；写入前按 `market+code` 查找并移除旧节点，再
追加当前 85 字节记录，因此语义是按节点、按日维护入池记录，而不是改变 flow 拓扑。

两条运行时分支还证明声音、提示窗口和板块写入都处在同一个 `if (baimpool)` 作用
域内。`bclearblock` 只在 `bsavetoblock` 分支中决定写入前是否删除目标 `.blk`，不是
独立动作。`bsavehis` 不在这条 cell-entry 路径中读取；它只在 `sub_10019CC0` 序列化
状态时以 `baimpool && bsavehis` 为条件调用 `sub_10017CC0`，把完整 85 字节状态字段
写入 `tpool\<池>\<节点>\<YYYYMMDD>.dat` XML 快照。

`btip` 也不是 TdxW 宿主回调。`TPool_StartRun` 自行创建 492 字节的提示 CWnd 并保存
到 `dword_10067930`；`sub_10031180` 按 187 字节步长合并记录、刷新窗口、置为
topmost，并按 `tpool/config.ini [NEWTIP] StaySec` 启动定时器，默认 20 秒。构造端
两次 `strncpy(..., 50)` 与随后的 85 字节复制证明单条布局为：池名 51 字节、节点
显示名 51 字节、股票状态 85 字节。股票状态中已确认 market/code、入池日期时间和
价格、收益、现价、涨幅、成交量以及最大涨幅阶段等 14 个字段偏移。

板块保存使用真正的宿主回调，但其契约也已闭合。`TPool_RegisterCallBack` 的第二个
参数保存到 `dword_10067498`；两条运行时分支先把每个 85 字节状态压成 7 字节证券
记录（1 字节 market、6 字节 code），然后以
`name, -1, buffer, security_count*7, 88, 0, 0` 七个参数间接调用。此前把
`CString::GetBuffer(0)` 的参数误计入了回调；TdxW 消费函数 `sub_62B8D0` 的
七参数 `__stdcall` 签名消除了这个歧义。操作号固定为
88。`bclearblock` 打开时先 `unlink(blocknew/<name>.blk)`，随后仍调用同一回调。

三个注册槽也已完整归类。第一槽 `dword_10067494` 对应 TdxW `sub_61B630` 的
11 参数数据查询，TPool 直接使用类型 `4/32/102/104/105/111/122/125/126/134`；
第二槽 `dword_10067498` 对应七参数 UI/动作路由，直接操作号为
`9/12/15/31/32/57/88`；第三槽 `dword_1006749C` 对应逻辑四参数通用操作，TPool
以 10 打开编辑器证券选择并返回 25 字节记录，也以 31 在历史处理路径执行
65 字节输入、24 字节输出的辅助计算。工具只发布这些契约，不调用任一宿主函数。

`sub_100396F0` 证明
`nsoundtype=0` 使用 `sound\default.wav`，非零类型只在自定义文件非空时播放，
`PlaySoundA` 标志为 `0x20001`。运行路径还可见删除、提示窗口、
`blocknew\%s.blk` 的可选清空，以及 `_in_pool_his.txt`、`_status_his.txt`
历史文件。

原始静态分析产物为：

- `output/ida-tpool-actions-xrefs.json`
- `output/ida-tpool-action-imports.json`
- `output/ida-tpool-actions.log`
- `output/ida-tpool-action-imports.log`
- `output/ida-tpool-psatt-consumers-20260812.json`
- `output/ida-tpool-retention-semantics-20260812.json`
- `output/ida-tpool-entry-log-semantics-20260812.json`
- `output/ida-tpool-action-dependencies-20260812.json`
- `output/ida-tpool-tip-payload-semantics-20260812.json`
- `output/ida-tpool-block-callback-semantics-20260812.json`
- `output/ida-tpool-callback-callers-20260812.json`
- `output/ida-tpool-host-callback-contracts-20260812.json`

## 纯 C++ 实现

`native/src/cloud/tpool_actions.cpp` 将策略解析为 `delete`、`record-entry-log`、
`sound`、`tip`、`save-block` 和 `save-history`。`baimpool` 对应的
`record-entry-log` 返回路径模板、XML 字段和 `market+code` 去重键，但不会实际创建
目录或日志文件。

其中兼容的 `delete/count/type_id` 字段继续保留，但现在同时返回
`retention_count`、`retention_unit`、`retention_seconds`、`effective`、
`record_scope=stored-pool-history` 和 `record_size_bytes=85`。删除策略的触发范围明确为
`periodic-history-maintenance`；它不会再被 `tpool_action_plans_document` 误附着到
cell-entry 的证券列表。未知类型和非正窗口保留原始配置，但 `effective=false`。
声音、提示和板块动作统一返回 `requires_aim_pool_enabled=true`；开关存在但
`baimpool=0` 时仍可检查原始配置，却不会生成计划。`bclearblock` 只保留在
`save-block.clear_before_save` 中。`save-history` 的触发范围为
`pool-state-serialization`，返回 `.dat` 路径模板和 14 个状态字段，不进入证券
cell-entry 计划。

`tip` 动作现在返回 `host_callback=false`、`ui_owner=TPool.dll`、187 字节三段布局、
14 个已知股票状态字段、默认停留时间和配置键。工具仅描述原窗口会收到的批量
载荷，不创建窗口、不置顶、不启动定时器。

`save-block` 动作现在返回回调注册槽、7 个参数、操作号 88、7 字节证券记录布局及
85 字节来源步长；具体计划还按证券数计算 `security_buffer_bytes`。这些字段仅用于
解释和前端展示，不调用注册回调，也不创建或删除 `.blk`。

`pool inspect` 现在还返回 `host_callback_count=3` 与
`host_callback_contracts`，静态说明三个槽的宿主函数、签名、角色和 TPool 直接使用的
操作号，同时继续返回 `action_policies`、`action_policy_count` 和
`configured_action_count`。单次 flow 投影以及 `pool watch` 状态推进只在证券新进入
目标 cell 时附加 `planned_actions`；每个计划都明确携带：

- `original_host_side_effect=true`
- `execution_mode=planned-only`
- `executed=false`

响应顶层同时返回 `planned_action_count` 和 `host_actions_executed=false`。因此网页、
CLI 和固定 API 可以解释“原版准备做什么”，但不会播放声音、弹窗、删除证券、
写板块或历史文件，也不会写回股票池 XML。

## 验证

- 增量构建：`tdx-tpool-tests`、`tdx-recon-contract-tests`、`tdx-tool`。
- 两个测试程序均通过；覆盖五类配置动作、默认声音、无副作用标志、投影与状态机
  新进入触发。
- 离线代表样本：`output/tpool-action-plan-fixture.xml`，当前检查结果写入
  `output/tpool-action-plan-inspection.json`；其中 5 个已配置动作的第一项为
  `record-entry-log`，路径和去重键均已类型化，执行数为 0。历史求值快照保留在
  `output/tpool-action-plan-evaluation.json`。
- 聚焦 API 报告 `output/api-contract-tpool-actions-focused-20260812.json` 为 4/4。
- 一次完整 API 巡检为 221/224；三个失败都是可转债申购、可交换债和社保持仓的
  稳定上游数量漂移。把固定行数阈值改成结构与对账约束后，TPool 加这三个修正的
  定向复核 `output/api-contract-tpool-actions-final-focused-20260812.json` 为 4/4。
- 正式服务端口 8765 未重启、未改动；临时验证端口已关闭。

精确语义补齐后又执行了增量验证：只重建 `tdx-tpool-tests`、`tdx-tool` 和
`tdx-recon-contract-tests`，TPool 单测及 `formula-evaluation` 契约域均通过，未运行
完整 CTest。代表性只读样本为
`output/tpool-retention-policy-fixture-20260812.xml`，输出
`output/tpool-retention-policy-inspection-20260812.json` 正确得到 3 分钟、180 秒、
`effective=true` 且 `executed=false`。随后补齐的 `baimpool` 入池日志动作也通过同一
TPool 单测、`formula-evaluation` 契约域和代表性 CLI 检查。进一步收紧
`baimpool` 主开关、`bclearblock` 从属关系和 `bsavehis` 序列化范围后，临时 18873
上的 `health/features/openapi/tpool-inline-evaluate-post` 聚焦契约为 4/4，报告写入
`output/api-contract-tpool-semantics-focused-20260812.json`；临时服务已关闭，正式
8765（PID 24096）未操作。全程未运行完整 CTest 或完整 API 契约集。

提示窗口载荷补齐后再次只运行 TPool 专项测试和临时 18874 的四项聚焦契约，结果
仍为 4/4；报告为 `output/api-contract-tpool-tip-focused-20260812.json`。临时端口已
关闭，正式服务保持存活且未重启。

板块回调补齐后，TPool 专项测试与 `formula-evaluation` 契约域通过；临时 18875 的
契约样本已实际启用 `bsavetoblock/bclearblock`，并校验 7 字节记录、动态缓冲区长度
和操作号 88，`health/features/openapi/tpool-inline-evaluate-post` 为 4/4。报告为
`output/api-contract-tpool-block-focused-20260812.json`，临时端口已关闭。

随后用 TdxW 的实际七参数 `sub_62B8D0` 签名纠正了早先把 `GetBuffer(0)` 参数计入
回调的错误，并把三个注册槽拆到独立 `tpool_callbacks.cpp` 类型化发布。仍只执行
增量构建、TPool 单测与 `formula-evaluation` 契约域；代表性 CLI 输出
`output/tpool-host-callback-inspection-20260812.json` 得到三槽、第二槽七参数、操作号
`9/12/15/31/32/57/88` 和第三槽 `10/31`。临时 18876 的
`health/features/openapi/tpool-inline-evaluate-post` 为 4/4，报告为
`output/api-contract-tpool-callbacks-focused-20260812.json`。服务按
`--max-requests=5` 自动退出，正式 8765 仍为 PID 24096；未运行完整 CTest 或完整
API 契约集。

随后已把这里仅作为计划公开的两种每日历史落盘格式实现为独立只读解析器：`.log`
固定 5 字段，`.dat` 固定 14 字段，均沿 `tpool/<池>/<节点>/<YYYYMMDD>` 发现。
实现和验证记录见 [TPool 每日历史文件解析](2026-08-12-native-tpool-daily-history.md)。

## 保留边界

`ndelnum/ndeltype` 的单位和过期条件、每日 `.dat/.log` 的只读物理结构已经闭合；
尚未复刻的是原 worker 的周期触发节奏和真实写回。实际宿主副作用、原版 XML/历史/板块
写回仍不属于当前只读工具的执行范围。
