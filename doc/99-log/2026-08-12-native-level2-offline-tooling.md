# Level2 离线工具、SDK 1807 计划与网页接线

日期：2026-08-12

## 本轮结果

- `level2 build --format sdk-1807-plan` 接受 1–100 条 packed 7 字节证券项：
  `market_raw:u8 + code:char[6]`。
- 输出 `tdx-level2-sdk-1807-request-plan-v1`，准确记录
  `fnSubscribeData(list,count,1807,0)` 和本地 callback registry mode 9；
  mode 9 不被描述成 wire 字段。
- 离线工具没有 TdxW 进程内证券 resolver，因此每项保持 `needs-resolution`，
  `sdk_call.ready=false`、`network_request_bytes_built=false`。它不会仅凭市场号和代码
  猜测 SDK 路由或 11 字节本地 fallback。
- `level2 build --format fasthq-subscribe-plan` 只复现
  `FastHQ.Subscribe` 的八个逻辑任务字段：
  `Name/CODE/SC/LX/PkgType/OperType/PushType/BatchPush`。`LX` 保持未分类的
  signed raw 值，订阅/退订只对应 `OperType=1/0`；结果不序列化 TQL body、
  不构造网络字节、不入队也不发送。
- `level2 build --format tdxw-1369` 构造 40 字节 TdxW 内部 IPC 请求；
  `+38` 为 `u16 count`，只接受 `1..1000`，主程序已知调用值为 `11/1000`。
- `level2 build --format tdxw-1371` 构造 48 字节 TdxW 内部 IPC 请求；
  `+37` 为 `side_mode_raw`、`+38` 为 `f32 selected_price`、`+42` 为允许
  `-1` 的 `i32 cursor`、`+46` 为 `1..5000` 的 `u16 count`。
- 两类构造结果都是 TdxW 进程内传输使用的 IPC 字节，不是 SDK 网络请求体；
  工具只输出结果，不连接、不发送，也不据此创建订阅。
- direct 请求保留原有默认 `1364/1374`，并以显式 `--initial` 或 HTTP
  `initial:true` 构造已证实的初始命令 `1363/1373`；输出同时给出 command 与
  `standard|initial`，不把默认分支武断命名为续页。
- direct 响应 varint 在最高 `shift=62` 处先检查剩余有效位，累计价格也在加法前
  检查有符号 64 位边界，畸形捕获不再通过截断或 C++ 溢出伪装成合法价格。
- SDK `1803/18031/1804/1807/18071` 均按已恢复回调固定为单个完整 body，
  拒绝少一字节、多一字节或双份 body；只有 `1801/1802` 保留计数记录数组语义。
- `level2 decode --format sdk-correlation` 解析 TdxW 进程内 SDK 回调关联表的
  25 字节精确记录数组，按 opaque callback keys、证券身份、data type 与
  registry mode 输出。它明确标记为非网络、非会话、非 Token 记录；mode 9
  匹配后保留，其他 mode 匹配后消费。
- `level2 decode --format sdk-json-4653|sdk-json-4655|sdk-json-4671|sdk-json-4680`
  接受调用方合法取得的 UTF-8 SDK JSON：分别归一证据命名的分时记录、逐笔成交、
  首个买侧/末个卖侧委托数量队列和五/十档行情。各格式严格校验记录数、原生容量与
  数值边界，不保留
  未使用的输入字段，也不把 JSON 当作授权或订阅能力。
- 新增 POST `/api/v1/level2/build` 与 `/api/v1/level2/decode`。服务仅处理内存参数和
  内联十六进制，拒绝文件路径；原始解码体限制为 384 KiB，确认头分别为
  `X-TDX-Action: level2-build` 与 `level2-decode`。
- Svelte Level2 实验室现提供多类字节/逻辑/宿主路由计划和解码入口，仍不附加进程、
  不发送订阅、不读取票据、不绕过账号权限。type-31/type-104 解码成功后可把整份
  内存 JSON 通过当前浏览器 session handoff 交给公式工作台；type-31 由 C++ 按目标
  K 线时间戳物化，type-104 提供 `ISBUYORDER` 标量；新增的 `sdk-correlation`
  入口把合法本地表快照关联到证券和 data type。网页不会据此抓取、合成或
  跨证券复用 L2 数据。
- `fasthq-subscribe-plan` 当前保持 CLI-only；它是第八种离线 build format，但不是
  可发送请求，因此网页/API 不把它包装成“订阅”按钮。
- type-31 materializer 现支持 `day/week/month`，分钟周期仍明确拒绝。日线保持
  exact/crop/trailing；周/月严格按 `sub_100353B0` 对日记录执行 raw sum、末值、
  撤单前日 `CUR_BUYORDER/CUR_SELLORDER` carry 与平均价 `low/high` 回退，并对
  leading partial、缺精确末日和 trailing inheritance 使用原生边界。null/sentinel
  安全传播，不会产生约 `-4.04e34` 的假值。

## 同批公共界面一致性

- `/api/v1/features` 保留兼容字段 `api_endpoint`，新增
  `api_operations[{method,path}]` 与 `surface=http|cli-only`。只有 `/` 开头的真实
  HTTP 路径会进入 operations/OpenAPI；`CLI only` 不再成为伪路径。
- strategy scan/backtest 与 Level2 build/decode 只发布 POST；既有真实 GET+POST
  入口继续保留双方法。Svelte 系统页会遍历 OpenAPI 的多种 method，并优先使用
  `api_operations` 回退，按 method+path 唯一展示。
- `PriGS.dat` 用户公式保持显式 opt-in。CLI 只有传入 `--include-user` 才会把只读
  用户库装入分析、求值、扫描、监控、组合策略和回测；网页/API 不默认暴露用户策略
  源码，也不会因 Level2 context bridge 自动启用 PriGS。

## 静态证据边界

`TdxW!sub_69CEF0` 读取七字节证券项，先通过进程内证券表解析；普通 SDK 路由证券
拼成 `SZ/SH + code` 列表并调用 `fnSubscribeData`，北交所或特殊证券进入本地路径。
由于离线工具无法可靠复现 `sub_401AB0/sub_594580` 的当前运行时证券表，本轮只恢复
类型化调用计划，不生成所谓“1807 网络请求体”。批量上限 100 来自原生调用方上限和
核心函数 800 字节 handle-pair 栈区，不外推到更大批次。

`TdxW!sub_69B3F0` 的非 SDK 分支把命令 `1369` 写入 40 字节体，在 `+37`
写固定值 `1`，并把调用方的 `u16 count` 写入 `+38`。其界面调用链分别传入
`11` 与 `1000`，返回的 `1803` 固定体每侧也最多容纳 1000 档，因此离线入口
不把二字节宽度外推成 `65535`。

`TdxW!sub_69B800` 的非 SDK 分支构造命令 `1371` 的 48 字节体：`+37` 保存
方向/模式原值，`+38` 保存用户选中价位的单精度价格，`+42` 保存响应续取使用的
有符号游标，`+46` 保存数量上限。调用链把游标初始化为 `-1`，数量传为 `5000`；
`18031` 返回解析器同样把数量限制在 5000。主程序另有一个对模式原值加 `10` 的
条件分支，但当前证据不能给该开关或 `0/1/10/11` 枚举赋予确定业务名称，所以
公开字段只保留 `side_mode_raw`，不标成买侧或卖侧。

这里的 `1369/1371` 是 TdxW 内部 IPC 命令，`1803/18031` 是另一条 SDK
回调类型枚举。离线工具不会把二者混称，也不会把内部 IPC 字节包装成可发送的
SDK 或 7709 网络请求。

`4671` 目前仍只确认响应/缓存识别和 `GetQueueJson` 使用关系，未找到精确客户端
构造总长及尾字段赋值；因此没有添加推断请求体。

SDK correlation registry 的静态布局来自 `TdxW!sub_68C510`：写入两个回调
key、`market_raw + code[6]`、零保留字节、data type 和 registry mode；
`TdxW!sub_68C750` 按两个 key 与 data type 查找，并以 mode 9 决定是否保留。
当前证据没有给两个 key 命名，因此工具不会将它们推断成 session、token 或
网络 request id。

`FastHQ.Subscribe` 逻辑计划的八个字段来自
`tpbus!sub_10076A7A (HQDataMaintain.cpp:1837-1844)`。静态证据只足以恢复任务名、
证券代码、市场、signed raw `LX`、包/操作/推送字段和批量标志；没有给出 `LX`
枚举终值，也没有恢复独立可发送的序列化体。因此公开结果把
`body_serialized/wire_bytes_built/job_enqueued/subscription_sent` 全部保持为 false。
新增 `field_origins` 明确区分来源：`Name` 来自 IXReq request name 与
`CTAJob_InetTQL` envelope，`CODE/SC/LX/PkgType/OperType/PushType/BatchPush` 来自
IXReq body item。因此静态请求体字段数是 7，连同 envelope `Name` 的逻辑投影为 8，
不再把八个投影字段都含混描述成 body。

## SDK fnReqData 逻辑调用计划

`level2 build --format sdk-fnreqdata-plan` 已覆盖
`1801/1802/1803/1804/18071`。`1801/1802` 要求显式 cursor 与 `1..1500` count；
`1803/1804/18071` 固定使用 cursor 0、count 1。输出恢复 12 个 32 位 ABI 槽，
并给出 25 字节进程内 callback correlation 的精确布局；后者仍缺两个 SDK 输出的
opaque callback key，不能伪装成 session、token 或 request id。

这只是 SDK export logical call plan。live owner window、`fnReqData` export 与 callback
key storage 尚未解析，故 `sdk_call.ready=false`、`invoked=false`、correlation
`ready=false`，且 `abi_stack_bytes_built/wire_bytes_built/network_request_bytes_built/
request_sent` 均为 false；工具既没有构造网络体，也没有调用 SDK。

`18031` 没有并入上述 25 字节模型，而是由独立
`sdk-fnreqdata-18031-plan` 描述。它保留 `side_mode_raw`，仅按调用点将原值 1
映射 selector 0、其余 u8 映射 selector 1；selected price 先保留 f32 bits，再精确
提升为 double ABI bits。cursor/count 固定 `0/1`，registry mode 固定 2 且不占 ABI
槽。回调关联明确为 `host-global-correlation/needs-live-host-context`，key 为 null，
`uses_25_byte_local_registry=false`；因此绝不创建 25B 记录、wire/network bytes 或调用 SDK。

## SDK fnSubscribeData 批量逻辑计划

`sdk-fnsubscribe-batch-plan` 的 CLI 从 `--input FILE` 读取 1..16384 字节 strict
UTF-8；POST `/api/v1/level2/build` 则只接受内联
`{format:"sdk-fnsubscribe-batch-plan",document:{data_type,symbols}}`。document 必须是
精确对象，不接受第三个字段。data type 仅为
`1801/1802/1803`，symbols 必须含 1..100 个大写 `SZ/SH/BJ` 加六位 ASCII 数字；
1807 会明确提示改用既有专用 `sdk-1807-plan`。

输出复原六个 32 位 ABI 槽与逗号分隔证券列表，并为每票发布一条 25 字节、registry
mode 9、匹配后保留的 callback correlation template。两个 opaque key 均为 null，
templates/records 因依赖 SDK 输出而未就绪；host view 没有重放、security resolver 没有
调用，SDK call/ABI stack/wire/network bytes/request/subscription 全部保持 false。这是
静态调用计划，既不加载 SDK，也不打开或绕过授权会话。Level2Lab 已提供 data type
与多行证券输入，并在浏览器侧执行相同 1..100、证券格式和 16 KiB 预检。

## SDK callback 宿主路由计划

`sdk-callback-route-plan` 覆盖 `1801/1802/1803/1804/1807/18031/18071` 七类回调，
CLI 与确认 POST 共用同一 domain builder，Level2Lab 提供 data type、完整 u32
`registry_mode_raw` 以及仅 1807 可用的自动/false/true 三态：

- `1801/1802` 的消息分别为 `0x54E/0x54F`；mode 9 为同步 `SendMessageA`，其他
  mode 为异步 `PostMessageA`，lparam 保留 mode raw。
- `1803` 使用异步 `0x551`，lparam 同样保留 mode raw；`18031` 也使用异步
  `0x551`，但 lparam 固定 2，关联来自 host-global，不创建 25B 记录。
- `1804/18071` 使用异步 `0x54D`，lparam 为对应 data type。
- `1807` 的普通分支为异步 `0x54D`；host time 已推进时还有同步 `0x8B9` 与带额外
  未解析宿主谓词的同步 `0x91E`。自动态不猜条件值，而是同时保留三条候选；false
  只保留普通分支，true 只保留后两条。

除 `18031` 外，六类都要求 25 字节 local registry mode；mode 9 表示匹配回调后保留
记录。两个 opaque key、证券宿主引用、窗口 handle 和 runtime-class 谓词尚未解析，
结果中的 wparam/recipient 继续保持 unresolved。该结果只是 TdxW 进程内宿主路由
元数据：`sdk_callback_invoked/host_message_dispatch_attempted/request_sent/
subscription_sent=false`，`host_messages_sent/network_requests=0`。工具不调用 SDK、
不执行回调、不投递消息、不构造网络字节、不联网，也不构成 Level2 授权绕过。

## SDK callback invocation 离线封套

在上述 route plan 之外，`sdk-callback-invocation` 已贯通 domain、CLI、POST
`/api/v1/level2/decode` 与 Level2Lab，用于把调用方合法取得的 callback ABI 参数和
内联 body 一次性校验、解码、关联。它不是新的网络输入格式，也不读取服务器文件。

body contract 严格按 data type 区分：`1801` 为 `callback_arg5_raw×52` 字节，
`1802` 为 `callback_arg5_raw×40` 字节；`1803/18031/1804/1807/18071` 分别固定为
`32016/20012/432/380/380` 字节，少一字节、多一字节或拼接多个固定体都拒绝。所有
输入继续受 384 KiB 上限约束。`callback_arg5_raw` 必须是 i32；1801/1802 分别将它
解释为 `1..7561/1..9830` 的记录数，另外五类只精确保留完整 i32 原值。
`callback_arg6_raw` 必须在 `0..4294967295`；除 18031 外必须提供同范围的
`registry_mode_raw`，18031 则拒绝该字段并沿用 host-global
correlation。

结果复用各 data type 的既有 SDK decoder 和 `sdk-callback-route-plan`，因此同一文档
同时携带已解码记录、精确 body contract、消息 ID、同步/异步路由候选及 25B 或
host-global correlation。HTTP 仅接受严格扁平白名单和内联 hex，拒绝
`input/path/url/file/endpoint/host_time_advanced/callback_key` 等越界字段。输出固定
`input_retained=false`、`file_accessed=false`，不回显原始 hex/body；整个流程没有 SDK
调用、callback 执行、宿主消息投递、网络请求、订阅或 wire 发送。

## 验证

- 此前阶段基线的原生 CTest 为 121/121；此前完整 API 契约为 211/225，较更早的
  207/225 恢复 4 项且没有新增失败；
  `jsn-discovery-live` 的瞬时超时随后 focused 1/1 通过，余下 13 项均为此前已有的
  静态人口基线或跨市场审计差异，不涉及本批 Level2、公式或公共 schema。
- 当前源码的 `tdx-level2-tests` 聚焦测试通过：除 SDK 1807 计划边界、
  `1369/1371` 的精确请求字节、
  `1363/1364/1373/1374` 精确请求字节、varint/累计价格溢出、五类单体 exact-size，
  以及 `1369/1371` 的 market/count/price 拒绝边界外，也覆盖三类 SDK JSON
  归一化和 `FastHQ.Subscribe` 逻辑字段、订阅/退订及参数拒绝边界。
- 当前源码的 `tdx-server-level2-tests` 聚焦测试通过：覆盖 direct、SDK redirect、
  1807 计划、`1369/1371`、1801/type-104/SDK correlation、三类 SDK JSON、
  格式字段白名单、越界拒绝和 OpenAPI POST-only。
- CLI smoke：`1369` 的 40 字节与 `1371` 的 48 字节 JSON/hex 输出符合固定夹具，
  且都显式标为未发送的 TdxW internal IPC。
- 两项 `sdk-fnreqdata-plan` CLI smoke 通过；输出为 12 槽，correlation 未就绪，
  ready/invoked/sent 保持 false。
- 第三批 Level2 focused 覆盖独立 18031 计划及 CLI 边界并通过；side 1/0/255 的
  selector 折叠、f32→double bits、固定 cursor/count/mode、非有限价格拒绝及
  host-global/no-25B 均有合约。
- 第四批 `tdx-level2-tests` 通过，覆盖 batch 类型、1/100 边界、严格证券身份、
  精确 JSON schema、16 KiB/UTF-8 边界、1807 引导和所有 offline 状态。
- 第五批 `tdx-level2-tests` 与 `tdx-server-level2-tests` 通过；HTTP focused 覆盖
  batch inline document、七类 callback route、1807 三态、mode 9 同步分支、18031
  global/no-25B、u32 最大值精确保留、越界/小数/错误类型及 `input/path/url/file`
  拒绝。`tdx-server-catalog-tests` 同时锁定 POST-only 与“不调用 SDK/不投递消息”边界。
- 第六批 Level2 与 server-level2 focused 覆盖 callback invocation 七类精确长度、
  arg5 i32/count、arg6/mode u32、18031 mode 例外、危险字段拒绝、384 KiB 上限、
  decoder/route 复用和无原文回显；catalog focused 同时锁定公开能力与离线边界。
- `svelte-check`：本批复核为 0 错误、0 警告；Vite 生产构建通过，仅有既有
  chunk warning。
- CLI 夹具 `output/verify-level2-sdk-1807-plan-20260812.json` 确认一条
  `000001` 身份保持 `needs-resolution`，没有生成或发送网络字节。

以下安装信息只属于新增 SDK JSON 与 FastHQ logical plan 之前的阶段基线：当时产物
已安装并重新启动正式 `127.0.0.1:8765`，PID 42900；EXE 为
40,423,382 字节，SHA-256
`1F679AB97BA106FCC7BE0670BDE1B2FE88FE0EDB635797B32121B14480EA4ACE`；网页
`index.html` SHA-256 为
`522C4C9C9E18C57A680AB6732E0A928535F7001770B2F293E754F88D2AE27581`。
构建目录、安装目录和 health 自报哈希一致；完整报告为
`output/api-contracts-level2-formula-trading-state-final-20260812.json`，瞬时重试报告为
`output/api-contracts-jsn-discovery-retry-20260812.json`。
health 确认 `native_cpp=true`、`python_runtime=false`、380 个公式；用户库仅因该次
启动显式传入 `--include-user-formulas` 而加载。

随后包含三类 SDK JSON、FastHQ field origins、fnReqData logical plan、公式与 TPool
HTTP/UI 的第二批产物曾 build/install/restart。该阶段正式 PID 为 `11468`，EXE SHA-256
`3881917c02000fad1dcc8d6e9d956e92022f910932a7c1aad2f0fc3176c0fb54`，网页
`index.html` SHA-256
`f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`；运行时
focused 确认构建/安装/health 哈希一致，并确认 fnReqData 12 槽且
ready/invoked/request_sent 为 false。该 PID/hash 现只保留第二批阶段历史。

第三批完整构建及 CTest 123/123 通过，但未运行 full API。该阶段正式服务更新为
PID `17540`，EXE SHA-256
`007f61021734ea56ed9b7106bafbe46bd74e1a01467c539734faa8072f457645`；网页未变化，
`index.html` SHA-256 仍为
`f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`。
运行时 18031 focused 以 side raw 255 得到 selector 1，10.25f 的 double bits 为
`0x4024800000000000`，关联为 host-global/no-25B，ready/invoked/request_sent 均为 false。

以上 PID `17540` 是第三批阶段历史。第四批完整构建通过（94.9 秒），CTest
123/123 通过（50.30 秒）；没有运行 full API。正式运行时仅做 health、batch-plan
CLI、普通公式、BUY IR、arity 400 五项 focused 合约，全部通过。该阶段正式服务 PID
`44960`，监听 `127.0.0.1:8765`，root 为 `C:\new_tdx` 且显式 include-user；EXE
SHA-256 为
`ef22956254d57079c44f8d106979fb773e74a9612db39413fc7249f4d910d808`。网页本批无变化，
未重复 npm 检查或构建，`index.html` SHA-256 仍为
`f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`。

第五批 Level2、formula language/context-and-library/strategy、server-level2 与 catalog
focused 全部通过；`svelte-check` 为 0 errors / 0 warnings，生产构建成功且仅有既有
chunk warning。完整 C++ 构建成功，CTest 123/123；临时与正式 HTTP 样本均通过，
包括 batch POST、callback route 的 1801/18031/1807 三态和所有 offline 标志。

第五批阶段正式服务 PID `17908`，监听 `127.0.0.1:8765`；EXE SHA-256 为
`03bb4a68b87cbcda4d1a27534da3ba5914d9c49a1594533e3db496c7ba17af31`，部署网页
`index.html` SHA-256 为
`8ffc02ac014017478efabf576619732b58af123c123697dd03f48fa0773ff37b`。health 确认
`native_cpp=true`、`python_runtime=false`。PID `44960` 与相应哈希现仅为第四批
历史部署；PID `17908` 与对应哈希现在也只保留为第五批历史。

第六批 formula language/context-and-library/strategy、Level2、server-level2 与 catalog
focused 全部通过；`npm run check` 为 0 errors / 0 warnings，生产构建成功且仅有既有
chunk warning。完整 C++ 构建成功，CTest 123/123，用时 49.99 秒。临时与正式 HTTP
合约均通过，其中 callback invocation 的 1804 样本严格为 432 B；同批平安银行
40 bars 原生运算与全库审计也通过，审计结果为
`total=380, passed=228, errors=0, context=128, period=1, market=3`。

当前正式服务 PID `21416`，监听 `127.0.0.1:8765`；EXE SHA-256 为
`07b972da59da1cb959678c701ffb9be59eba023a2550bc533e4ee76557e7da6a`，部署网页
`index.html` SHA-256 为
`1f202c046bc8f3ff2c36de731027b72112b7c8c206a62b8d13df326b58107c9f`。health 确认
`native_cpp=true`、`python_runtime=false`；本批仍没有真实 SDK 调用、宿主消息投递、
wire 发送或授权绕过。

## 1807/18071 host quote state transition

新增 `sdk-quote-transition` 纯领域投影。调用方必须同时提供 `1807|18071`、精确
380 B body、schema 为 `tdx-level2-sdk-host-quote-state-v1` 的上一状态和显式 host
context；工具按恢复的 signed epoch、float32 价格边界、OHLC、累计量额、十档和聚合
字段规则生成下一状态。它不从 SDK、网络或进程内宿主暗取状态，也不保留输入体。

同一 builder 已贯通：

- CLI：`level2 project --format sdk-quote-transition`；
- HTTP：确认 POST `/api/v1/level2/project`，严格内联输入、384 KiB 上限；
- Web：Level2Lab 可输入 body/previous/context，并把 `projected_state` 显式复用为下一次
  previous 或暂存为公式上下文；
- OpenAPI：该路径只有 POST，没有误暴露 GET。

这些只是 read-only host state projection；`sdk_loaded=false`，SDK/network/message
计数保持 0，不调用 callback，也不发送宿主消息或网络请求。

## SDK compatibility preflight

CLI-only `level2 preflight --format sdk-compatibility` 根据显式 root 和两个原始选择标志
选择 `SDKPlugins/bin/TdxDataSDKPR.dll|TdxDataSDK.dll`，并校验 host edition `3/17/53`。
有界 PE export parser 只读解析导出表，检查 7 个必需导出和 2 个可选导出；它限制
文件大小并校验 DOS/PE header、section、RVA、字符串范围与可打印名称，从不
`LoadLibrary`，也不读取 token、callback 地址、proxy 或 endpoint。

正式 `C:\new_tdx` 当前没有 `TdxDataSDK.dll`，因此该选择分支返回 `absent`。这只说明
文件不存在，不表示 SDK 加载成功、授权可用或可绕过 Level2 权限；preflight 没有 HTTP
或网页入口。

## 网页与最终验证

数据中心新增板块轮动页，消费既有纯 C++ 接口并显示行业、概念、地区、风格四类
汇总和明细，支持周期、方向、排序、检索、刷新与成员跳转。正式样本返回 515 条、
4 类汇总、`errors=0`；地区无本地成员和缺失当前行情列仍保持明确缺失。

本批 Level2/project/preflight、server/catalog、formula 与板块轮动 focused 全部通过；
`svelte-check` 为 0 errors / 0 warnings，生产构建成功且只有既有 chunk warning。
完整构建成功，CTest 126/126，用时 99.91 秒。正式 18071 runtime 投影确认没有 SDK、
network 或 message 动作；OpenAPI 确认 `/api/v1/level2/project` POST-only。

第七批正式服务 PID 曾为 `40020`；EXE SHA-256 为
`86fb3a4b674cbb60c817383e3b4f154b0b4482d133a1c03333510ba67c93ed04`，部署网页
`index.html` SHA-256 为
`b1dc8125928d3d6d0e1359b3ce532d4c0beb90211d82577f0cfc1ab8b2cce6ed`；health 为
`native_cpp=true`、`python_runtime=false`。PID `21416` 和对应哈希现仅是第六批历史。
以上不构成 Level2 授权绕过、SDK 加载、SDK callback 执行、宿主消息发送或真实订阅。

## 25 B SDK correlation registry transition

新增 CLI-only `level2 project --format sdk-correlation-transition`，对显式提供的 25 B
registry 做纯状态投影。输入使用 strict JSON，递归校验 UTF-8，并对相应字段执行 u32
边界检查；registry 采用不超过 10,000 条的保守上限。

恢复的转换规则为：

- callback 以 `(type,key1,key2)` 选择首个匹配；mode 9 retained，其他 mode 在任何
  host lookup 前从 registry 消费；
- upsert 以 `(type,mode,market,code)` 选择首个匹配，命中只替换 callback keys，未命中
  才 append；
- 当前投影不会继续执行宿主证券解析。SDK load/callback、host lookup、消息投递、wire、
  network、credentials 和 entitlement bypass 标志均保持 false。

这不是 SDK callback 入口，也没有 HTTP/Web 入口；它只复现显式输入下的 registry
transition，不取得授权、会话、SDK 数据或宿主状态。

## 热点历史网页与本批验证

网页新增 `/data/hot-history` 和 StockWorkbench 热点历史面板，消费 210 条纯本地记录，
覆盖 203 只证券且上游网络请求为 0。仅 `native_host_eligible=true` 的记录生成安全
证券链接，`analysis` 只按纯文本渲染；数字 market 别名保持数字类型。由于服务端负责
分页和排序，前端禁用了只对当前页生效的本地表头排序。

本批受影响验证通过：独立 correlation target/test、formula `native-operators`、
`language`、`context-and-library` 通过，`tdx-tool` 目标构建成功；`svelte-check` 为
0 errors / 0 warnings，`npm run build` 成功且只有既有 chunk warning。没有运行完整
CTest（当前注册 127 项）或 full API 契约。

当前正式 PID 为 `36624`，EXE SHA-256
`6b8b507a32b355a173bf95daac92afb77d73ffeaaf95275afa1d4180179d4499`，部署网页
`index.html` SHA-256
`ca685162fc620086b6caeb9653bceb909dfe8543a293cf00287dea55242ab42f`；health 为
`native_cpp=true`、`python_runtime=false`、`formulas=380`、`blocks=1159`。correlation
smoke 命中索引 0、消费后得到 `projected_count=1`，全部副作用标志为 false；热点历史
smoke 返回 `catalog_count=210`、`returned=2`、`network_requests=0`，数字 market
回显 `0`，`sh603137` 有 1 条可安全跳转记录，且部署 UI asset 存在。PID `40020` 及其
EXE/web 哈希现仅为第七批历史。本批不声称 Level2 授权绕过、SDK 加载、真实 SDK
callback 执行、宿主消息发送或真实订阅。

## SDK 1804 到 TdxW host slots 的离线投影

`level2 project --format sdk-1804-host-projection` 首阶段新增为 CLI 工具，严格要求
`--encoding raw|hex` 和一个解码后恰为 432 B 的合法捕获 body。它复现
`output/ida-tdxw-level2-remaining-sdk-20260812.log:1072-1103` 中
`sub_68C750` 在 `sub_525600` 前的局部数组准备过程：

- 先把 105 个 f32 槽（420 B）全部清零，槽 0 保持零；
- 源 `+8/+216` 的两个 f64 价格窄化为 f32 并写入槽 1/2；
- 源 `+424/+428` 的两个 count 以 u32 原始位写入槽 3/4；
- 源 `+16/+224` 的两组 f32 数量分别复制到槽 5..54 和 55..104；count 大于
  50 时只把复制项数夹到 50，槽 3/4 的原始 count 不改写；
- 未取得方向枚举证据，输出继续使用 `first/second`，不猜买卖侧。

raw 输入少于或多于 432 B 均拒绝；hex 文本限制 16 KiB、必须为偶数个十六进制
数字且解码后仍须精确 432 B，未知 CLI 参数也拒绝。输出不保留输入 body，并明确
`sdk_called=false`、`sdk_callback_invoked=false`、`callback_executed=false`、
`host_storage_call_attempted=false`、`sub_525600_called=false`、wire/request false、
`network_requests=0`。首阶段没有 HTTP/Web 入口，也不加载 SDK、不执行 host storage 或消息
投递、不发送网络请求，也不绕过授权。

独立 target/test 已通过。正式 PID `4716` 使用
`output/verify-level2-sdk-1804-20260812.hex` 做 focused CLI runtime：源为 432 B，
投影为 105 槽/420 B，first 的 raw/copied 为 `2/2`，second 为 `3/3`，上述全部
副作用标志保持 false。

## 板块历史回测网页与本批验证

Svelte 新增 `/data/block-backtest`，直接使用既有
`tdx-block-backtest-native-v1`。板块主表支持五类 category、begin/end、adjustment、
服务端 sort/order 和 limit；只有 `block_id` 非空、本地 match 恰为 1 且 code 为六位
数字时才允许成员下钻。换块时先清除上一块成员，避免新请求失败后发生 identity/data
错配；同块刷新则可安全保留旧数据。成员证券只有 sz/sh/bj 与六位 code 均合法时才能
进入个股工作台。

页面固定展示 `membership_basis=upstream-server-selection` 和
`historical_membership_reconstructed=false`，明确成员是当前上游服务选择而不是历史
时点成分重建；loading/error/empty/cache/stale 状态均有独立呈现，且没有为当前页设置
本地 `sortValue`。正式 live focused 样本中，industry 主表 normalized 56 条；板块
`880310` 返回 normalized 40 名成员，首项为 `bj/920088`。

本批其余增量验证为 formula `native-operators`、`language`、
`context-and-library` 通过，`svelte-check` 为 0 errors / 0 warnings，`npm run build`
成功且只有既有 chunk warning。该 1804 CLI 首阶段没有运行完整 CTest，也没有运行
full API 契约。
当前正式 PID `4716`，EXE SHA-256
`d44ddaa5c95398cbac5a860963401c02eb682a574e3823b62bf5a38291ef01d8`，部署网页
`index.html` SHA-256
`838bb4e58df85fd2ae4d46915142fe1a9a5fe0add2f3e2f8ba557344786f1420`；health 为
`native_cpp=true`、`python_runtime=false`、`formulas=380`、`blocks=1159`。
PID `36624` 及其 EXE/web 哈希现在只表示上一批历史部署。

## SDK 1801/1802 到 20 B TdxW host record 的离线投影

首阶段新增两个 CLI 格式：

- `level2 project --format sdk-1801-host-projection`
- `level2 project --format sdk-1802-host-projection`

两者都要求 `--input BODY --encoding raw|hex --market 0..2 --code 6digits`。1801 输入
必须为非空 `count*52` B，1802 为非空 `count*40` B；解码后统一不超过 384 KiB，
少一字节、多一字节、空 clear-only body、非法市场或证券身份均拒绝。该 CLI 不承接
SDK null-source/clear-only 分支；首阶段尚未接 HTTP/Web。

领域层依据 `output/ida-tdxw-level2-remaining-sdk-20260812.log:956-1066` 恢复
TdxW host projection：

- epoch-ms 与 volume 都直接按 signed i64 解析；`sub_689E20` 先以有符号除法将毫秒
  除以 1000（向零截断），再用 localtime 计算
  `second+60*(minute+60*(hour-6))`，存其低 u16；
- 完整复现 market `0/1/2` 与六位 code 的 security classifier，以及 class
  `2/3/4/5/13/14/15/16/22/23` 使用 10、其余使用 100 的 lot divisor；
- 价格边界显式按 x87 路径处理：`price*10000+0.503000020980835` 后向零截为 signed
  i64，非有限/越界使用 integer-indefinite，再把低 i32 写入记录；volume 用 signed
  除法，商与同号余数均保留；
- 每个 source record 精确产生 20 B host record。每份 callback 文档声明先清除旧数组
  并以本批全部记录替换。1801 的 `first_raw/second_raw/qualifier_raw` 只表示原始
  offset，不推测语义；1802 使用证据确认的 `order_id_raw`、
  `side_or_cancel_qualifier_raw` 与 `action_raw`，其 record-type 映射保持原始值可审计。

输出不保留 body，并明确 `sdk_called/sdk_callback_invoked/callback_executed`、host
storage、`sub_68C170/sub_68C200`、host message、wire、network、send/request 与
entitlement bypass 全部为 false/0。它不加载 SDK、不执行 callback、不写宿主状态、
不投递消息、不发送请求，也不声称取得或绕过 Level2 权限。

独立 180x target/test 通过；1801 fixture 生成
`843039300000f6ffffffff004433221188776655`，1802 fixture 生成
`4038204e0000feffffffff4243000000d4c3b2a1`，两者均为精确 20 B 且上述副作用为零。

## LimitReview 网页与本批验证

Svelte 数据中心新增 `/data/limit-review`，消费既有
`tdx-market-limit-review-native-v1`，提供当日、市场历史、指定日期和年度行为四个
服务端分页视图；只有安全 sz/sh/bj 六位身份可跳入 StockWorkbench 新增的单票
“涨跌停复盘”面板。主页面和单票面板均只渲染归一化字段，默认隐藏 `raw`，不自动
轮询，并明示“盘后逐步更新、非实时”。刷新失败只保留同一查询的旧数据；指定日或
单票动态资源缺失正常展示空态，不与实时盘口或 `market limit-quality` 混用。

正式 focused 样本中，current/history/daily/annual 的 `returned/source_rows` 分别为
`3/165、3/484、3/111、3/129`；security 查询为空且 `missing_sources=1`。本批增量
验证通过 formula `native-operators`、独立 quote-transition、独立 180x projection、
`npm run check/build` 和 `tdx-tool` 主程序构建；没有运行完整 CTest 或 full API。

当前正式服务 PID `24992`；EXE SHA-256 为
`17815ec422ff519eb69c69ecc3fe7186119b7ad00f2d3f41e6130754319f7c03`，部署网页
`index.html` SHA-256 为
`68b198ebed1345089b9a8df0b9255ca27f6e4c04e1d04fc6391ff0f4b77b428c`；health 为
`native_cpp=true`、`python_runtime=false`。PID `28660` 及其旧 EXE/web 哈希现在只
表示前一批历史部署，PID `4716` 与对应哈希表示更早历史阶段。

## window-positions、180x HTTP/UI 与最终验收

`output/ida-tcalc-window-positions-20260813.log` 的其余四个 handler 已独立落地：
`HHVBARS/LLVBARS` 每柱先将 period 收窄为 float32 再截断，period missing 保持
missing，非正/超长周期钳到全部可用历史；只跳过 leading source sentinel，internal
sentinel 仍参加带相对 `1e-7` 加绝对 `1e-5` 容差的比较，容差带内选择后出现位置。
`FILTER/FILTERX` 排除 sentinel，以包含端点的 `±1e-5` 判真；FILTER 正向跳过后续
N 柱，FILTERX 反向仅在 `N<=cursor` 时跳过前置 N 柱，超窗 N 保留前缀 eligible。
平安银行 day 40 bars 的 focused 结果为 `H=3/L=0/F=0/R=0/errors=0`，依次对应
HHVBARS/LLVBARS/FILTER/FILTERX；FILTERX 只在显式 `allow_future` 的只读模式执行。

1801/1802 host projection 随后接入确认 POST `/api/v1/level2/project` 与 Level2Lab。
POST 对它们严格只允许 `format/market/code/payload_hex`，只收内联 hex，保持非空
52/40 B 整数倍、market `0..2`、六位 code 与 384 KiB 边界；路径、文件、
previous/context 和未知字段拒绝。响应不回显 payload，`input_retained=false`。正式
1801 样本返回 `tdx-level2-sdk-1801-host-projection-v1`、`input_size=52`、source/host
`1/1`、host hex `7440649001007b00000000005100000052000000`，并保持
`sdk_called=false`、`network_requests=0`、`input_retained=false`。

最终又将 `sdk-1804-host-projection` 接入同一 POST/UI。1804 只允许
`format/payload_hex`，内联 body 必须精确 432 B；market/code 及其他字段均拒绝。
Level2Lab 的统一面板新增 1804 选项，选择后隐藏证券身份，显示 105 槽/420 B，且已
打入最终网页产物。正式 runtime 返回
`schema=tdx-level2-sdk-1804-host-projection-v1`、`input_size=432`、
`slot_count=105`、`byte_size=420`、first/second copied `2/3`；
`sdk_called=false`、`host_storage_call_attempted=false`、`network_requests=0`、
`input_retained=false`。三类 HTTP 投影仍不加载 SDK、不执行 callback、不写 host
storage、不投递消息、不联网、不发送请求，也不绕过授权。

验证按阶段记录：formula native-operators、server project/catalog focused 通过，
`svelte-check` 为 0 errors / 0 warnings，网页 production build 成功；1804 HTTP/UI
追加后再次运行受影响的 project/catalog focused、Svelte 0/0 与 web build，均通过。
104.7 秒完整主程序构建发生在最终 1804 surface 追加前；最终 surface 完成后又运行
完整 CTest，129/129、0 failed，用时约 73 秒（10:38:34—10:39:47）。本轮没有运行
full API 契约。

最终正式服务 PID `4336`；EXE SHA-256 为
`0c945050aab8db2134cc5e23b5e62a4dc7431e77630e2d53cbfaca0349198c28`，部署网页
`index.html` SHA-256 为
`3eba9d193d3b04840b57a709be00907926c20f07cfca6438eadc5c7b1b152339`。health 与磁盘
哈希匹配，`native_cpp=true`、`python_runtime=false`。PID `22116` 及中间 EXE
`580ed60e50492f7a5e52d1b1093bf3e9a7726fb15d4f0862712419e1828289c3`、网页
`5e246f8299a167526971062ee4f252614503b3749c51e63c35d507074c4bd0f6` 现仅为历史；
PID `24992` 及其上一阶段 EXE/web 哈希同样不代表当前部署。

## 后续正式部署基线更正

公式侧 SUMBARS/STDDEV 收口后，当前正式服务已更新为 PID `23140`；EXE SHA-256 为
`25882a4e62eda0d5c1702e2e4da4d8f60739c47ab0927616a8ee13cf0e3b9183`。本次没有
网页改动，部署 `index.html` SHA-256 仍为
`3eba9d193d3b04840b57a709be00907926c20f07cfca6438eadc5c7b1b152339`；health 与磁盘
匹配并保持 `native_cpp=true`、`python_runtime=false`。PID `4336` 及其 EXE 哈希现
仅是上一阶段历史部署，Level2 的离线/无 SDK、无 host storage、无网络边界没有变化。

本批只运行并通过 formula `native-operators` focused，未重跑完整 CTest 或 full API；
上文 129/129 是上一轮最终 1804 surface 阶段的完整 CTest 结果。

## SDK JSON 4653 domain、CLI、POST 与 Level2Lab 最终闭环

`tpbus ProtocolSZSDK2TDX::SetAnsData` 的 `sub_10082805` 直接支持了新的纯领域
`normalize_level2_sdk_json_4653`。顶层只接受精确 `{Data}`，Data 必须是最多 10,000 条
的数组；每条只允许 `datetime/closePrice/averagePrice/tradeVolume/reference_price`，
datetime 为非空 ASCII 数字串，首条 reference_price 必填，后续可省略。close、average
与 reference 都先收窄为 float32，再按原生 float32 除以 1000；tradeVolume 严格为
u32。

输出声明 `native_header_size=35`、`native_record_size=18` 和 `35+18*N` 恢复布局，但
只发布结构化语义，`native_body_built=false`，不会构造或回显 native body。datetime
只保留 `datetime_raw/right4_raw` 与
`time_u16_raw = u16(61 * (atoi(CString::Right(datetime, 4)) % 100))`；
`time_semantics_resolved=false`，不把它命名为日期、分钟或秒。

该 domain 已接入 `level2 decode --format sdk-json-4653`；CLI 对调用方显式本地 UTF-8
输入执行相同 384 KiB/10,000 边界。确认 POST `/api/v1/level2/decode` 只允许
`format/document/limit`，limit 为 `0..10000`，document compact 后最多 384 KiB；
路径、服务器文件、market/code、depth 与其他字段全部拒绝，响应不回显原 document。
Level2Lab 提供内联 JSON、limit 与明确的 float32/opaque-time 安全说明。POST/UI 不读
服务器文件；所有层都不调用 SDK/callback、不访问 host storage、不投递消息，不生成
native body/wire/request，不联网、不发送订阅、不保留输入，也不取得或绕过 Level2 权限。

`tdx-level2-sdk-json-4653-tests`、`tdx-server-level2-tests`、
`tdx-server-catalog-tests` 均通过；`svelte-check` 为 0 errors / 0 warnings，production
web build 成功且仅有既有 chunk warning。完整 CTest 最终为 130/130、0 failed、
50.51 秒。正式 runtime fixture 为 `count=2`、`returned=1`、header
`reference_price=10`，首条 `close_price=10.25`、
`average_price=10.100000381469727`、`trade_volume_u32_raw=123`、
`time_u16_raw=61`；`sdk_called=false`、`native_body_built=false`、
`network_requests=0`、`input_retained=false`。

当前正式服务 PID `12148`；EXE SHA-256
`b8e66b48c283383520f010148554a0cb80c58997601868c514954369854d8594`，部署网页
`index.html` SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `23140` 及其 EXE/web 哈希现只代表
上一阶段历史部署。

## BACKSET 收口后的部署基线更正

公式侧 `BACKSET` 已按 `TCalc!sub_10017550` 收口 first-valid/leading missing、启动后
sentinel 触发、严格 float32 `1e-5` 信号门槛，以及 N 的 float-to-i32、min-1 与
first-valid 裁剪。`native-operators` focused 通过，正式 `sz/000001` day 5 返回
`B=[0,1,1,0,0]`、`engine=native`。本批没有运行完整 CTest；上文 130/130 仍只属于
上一 sdk-json-4653 阶段，Level2 的离线和无 SDK/网络/订阅边界没有改变。

当前正式 PID `17676`；EXE SHA-256
`dc6bbe1ff93295db75a77518a8970f8d3260fd0ad0b6e7ee30af6a01ee060a1d`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `12148` 及其 EXE 哈希现为上一
阶段历史部署。

## BARSCOUNT/DMA 收口后的部署基线更正

公式侧依据 `output/ida-tcalc-barscount-dma-targeted-20260813.json` 收口 BARSCOUNT 的
leading sentinel/first-valid `0`/post-start 连续递增，以及 DMA 的 joint-first-valid、
float32 state、无 clamp、严格 upper-tolerance/direct-X 与 internal-sentinel 边界。
BARSCOUNT 影响 13 条公式/18 次调用，DMA 影响 6 条/6 次；`native-operators`、
`language` focused 通过，REF 的 `BARSCOUNT+1` 和 BACKSET leading fixture 已同步修正。
正式 `sz/000001` day 5 为 `C=[null,0,1,2,3]`、
`D=[11.1899995803833,11.239999771118164,11.25,11.25,11.229999542236328]`、
`engine=native`。本批未运行完整 CTest；130/130 仍属于上一 4653 阶段，Level2 离线
安全边界没有变化。

当前正式 PID `9272`；EXE SHA-256
`0388ce56c670078406e45def79a87f300e71b0621c9191638af954f140400063`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `17676` 及其 EXE 哈希现为历史。

## SLOPE 收口后的部署基线更正

公式侧依据 `output/ida-tcalc-slope-targeted-20260813.json` 的
`sub_1000CDD0/sub_10001280`，收口 SLOPE 的 first-valid gate、
`int(float32(N)+0.503000020980835)` period、first-relative 完整历史、`1..N` 回归、
累计/均值 float32 落点、最终分子/分母 x87 宽精度且仅商写回 float32、internal
sentinel 及 N=1 singular/no-forced-zero 边界。影响只有 `ACCER/BSQJ` 2 条公式/2 次
调用，`WMA/FORCAST` 未改。新增 source
`[5941892,49958348,-9642243,1061.6112060546875]`、N=4 判别夹具，正确输出为
`-7742307.5`，而错误的最终分子 float32 落点会输出 `-7742307.0`；
`native-operators` 与 `language` focused 通过。正式 `sz/000001` day 10、N=5 末柱为
`S=9.5367431640625E-07`、`normalized=8.507353044251431E-08`、`engine=native`。
本批未运行完整 CTest；130/130 仍属于上一 4653 阶段，Level2 离线安全边界不变。

该 SLOPE 精度修正的中间部署 PID 为 `11332`；EXE SHA-256
`b79d085fe4ea93c5f56c825a3f202c32f831e2a9941c79136c18f2127494ce87`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `4380` 及其 EXE 哈希现为历史。

## ABS/MOD 收口后的部署基线更正

公式侧依据 `output/ida-tcalc-formula-gap-audit-20260813.json` 收口
`ABS sub_1001D9F0` 与 `MOD sub_1000C990`。ABS 只跳过 leading canonical sentinel，
启动后仍保留精确 sentinel，其他输入以原生 float `fabs` 写回 float32；影响 35 条
公式/61 次调用。MOD 拒绝任一精确 sentinel，按
`int(float32(value)+0.503000020980835)` 转换 dividend/divisor 后执行 signed i32
remainder 并写回 float32，转换后 divisor 为 0 输出 missing；影响 3 条公式/6 次调用，
基础判别为 `MOD(5.4,2)=1`。

`native-operators`、`language` focused 通过。正式 `sz/000001` day 5 的 2026-08-13
末柱 `CLOSE=11.210000038146973`，ABS 为同值，`MOD(CLOSE*10,7)=0`，
`engine=native`。本批未运行完整 CTest；130/130 仍属于上一 4653 阶段，Level2 离线
安全边界不变。

该 ABS/MOD 阶段正式 PID 为 `26364`；EXE SHA-256
`5284c5916b00cf158356c765154380280215675784069e6b416c4f56befc3258`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `11332` 是 SLOPE 精度修正中间态。

## SQRT/INTPART 收口后的部署基线更正

公式侧依据 `output/ida-tcalc-sqrt-intpart-gap-audit-20260813.json` 收口
`SQRT sub_1001D820` 与 `INTPART sub_1001DD50`。SQRT 跳过 leading canonical
sentinel，以原生 float 条件 `abs_f32(x)*1e-7+x+1e-5<=0` 决定 carry previous；足够负
的输入和 started internal sentinel 命中 carry，其他输入执行 `sqrt(float32(x))` 并写回
float32。其影响为 `BOLL-RB/HISV` 2 条公式/2 次调用。

INTPART 只跳过 leading canonical sentinel，started internal sentinel 继续参与；它按
`float32(x)-abs_f32(x)*1e-7-1e-5<0` 选取 `-0.000099999997/+0.000099999997` 调整量，
安全截断为 i32 后写回 float32，非有限或越界转换得到原生 `INT_MIN`。其影响为
`WSBVOL` 1 条公式/2 次调用。`native-operators`、`language` focused 通过。正式
`sz/000001` day 5 的 2026-08-13 末柱 `CLOSE=11.210000038146973`、
`SQRT=3.3481338024139404`、`INTPART=11`、`engine=native`。本批未运行完整 CTest；
130/130 仍属于上一 4653 阶段，Level2 离线安全边界不变。

上一 SQRT/INTPART 阶段正式 PID `24720`；EXE SHA-256
`9f45aa0a6754496873b7b29a754dd94bbed930c3014dda26b7bb5b5ee6b58e8c`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；PID `26364`
是上一 ABS/MOD 阶段部署。

## MEMA/EXPMEMA 收口后的部署基线更正

公式侧依据 `output/ida-tcalc-mema-expmema-gap-audit-20260813.json` 收口
`MEMA sub_1000AEF0` 与 `EXPMEMA sub_1000B1A0`。两者固定使用末柱 N 的 float32→安全
i32 转换；seed internal sentinel 前向填充，seed/post-seed 状态及输出遵循 float32
落点。MEMA 使用严格 `first+N<count` 与 `((N-1)*previous+X)/N`，EXPMEMA 使用
`first+N<=count` 与 `((N-1)*previous+2*X)/(N+1)`；post-seed source sentinel carry
previous。影响分别为 MEMA 1 条公式/4 次调用、EXPMEMA 4 条/4 次。typed helper 接线
后删除了无调用的旧 `exponential` helper，EMA/EXPMA 未变，Level2 离线安全边界亦未变。

`native-operators`、`language` focused 通过。正式 `sz/000001` day 5 live 为
`E=11.229166030883789`、`M=11.235184669494629`、`engine=native`。本批未运行完整
CTest；130/130 仍只属于 sdk-json-4653 阶段。

上一 MEMA/EXPMEMA 阶段正式 PID `32380`；EXE SHA-256
`81b2845e4104e6fd22d79f3be288c99b92653189416b31f9f32b7c83ef054ca1`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `24720` 现仅代表上一
SQRT/INTPART 阶段历史部署。

## SDK 1803/18031 fixed snapshot 与 AVEDEV/POW 最终部署

新增 CLI-only `sdk-1803-host-projection`、`sdk-18031-host-projection`，分别只接受 exact
`32016`、`20012` 字节 callback body。projection 不伪造字段或结构转换，而是按
byte-for-byte 语义声明整份逻辑状态替换；输出仅含 prior/new replacement metadata、size
与 SHA-256，previous state 不要求、不读取、不保留，body 不回显。raw/hex 文件输入先做
有界预读，decoded body 同时服从 `384 KiB` 和 exact-size 校验。callback/SDK、host
storage/message、wire/network/request/subscription 相关动作与计数全为 false/0；focused
`tdx-level2-sdk-fixed-snapshot-projection-tests` 已通过。SHA-256 实现最终局部化在固定快照
projection TU 内，未改变 common 公共接口。

公式侧新证据 `output/ida-tcalc-next-scalar-series-targeted-20260813.json` 含 AVEDEV、
POW 的完整伪码与逐指令提取。AVEDEV 的末柱 N、leading gate、float32/wide 混合语义影响
CCI 1 条公式/1 次调用；POW 的 float32 operands、mixed-wide gate、invalid carry 影响
BOLL-RB 1 条公式/1 次调用。`native-operators`、`language` focused 通过；正式
`sz/000001` day 5 live 为 `D=0.015555699355900288`、`P=1.4884006977081299`、
`engine=native`。

上一 AVEDEV/POW + Level2 固定快照阶段正式 PID `30060`；EXE SHA-256
`cca7fdb7cace9a712b177a64322b0b1b9462025f12f7ef551b56074e9df7b91c`，部署网页
`index.html` SHA-256 为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `32380` 仅为上一 MEMA/EXPMEMA
阶段历史。本批未运行完整 CTest；130/130 仍归 sdk-json-4653 阶段。

## VALUEWHEN/BETWEEN 收口后的部署基线更正

公式侧继续使用 `output/ida-tcalc-next-scalar-series-targeted-20260813.json`：
`VALUEWHEN sub_1000B830` 以 condition-only leading gate、started exact float32 zero carry、
任意 nonzero（含 sentinel）选择 raw float32 selected，并允许 selected sentinel 覆盖 held；
影响 HANS123 1 条公式/2 次调用。`BETWEEN sub_1001DE70` 的 leading gate 仅检查 bounds
是否同时为 sentinel，value 不参与；started 后读取 raw float32、排序 bounds，并以
float32 `abs(value)` 执行 relative/absolute open-tolerance 判定；影响 W106 1 条公式/
1 次调用。Level2 SDK 1803/18031 fixed snapshot 的 CLI-only、exact-size 与全零副作用边界
保持不变。

`native-operators`、`language` focused 通过。正式 `sz/000001` day 5 live source
`Q:=CLOSE>11.2;V:VALUEWHEN(Q,CLOSE);B:BETWEEN(CLOSE,11,11.3);` 末柱为
`V=11.229999542236328`、`B=1`、`engine=native`。当前正式 PID `32284`；EXE SHA-256
`76787fa34f2b53c563e8ebfa70a651bbcce05271ac9873d70b81377073fafacc`，部署网页
`index.html` SHA-256 仍为
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `30060` 仅为上一 AVEDEV/POW +
Level2 固定快照阶段历史。本批仍未运行完整 CTest；130/130 仍归 sdk-json-4653 阶段。

## REFX/REFXV 收口后的最终部署基线

公式侧依据 `output/ida-tcalc-refx-limit-price-targeted-20260813.json` 收口
`REFX sub_10019F90` 与 `REFXV sub_1001A300`：两者均采用 source/offset
dual-leading sentinel gate，started 后使用 raw float32 operands/source，保留 offset
`fabs`、target landing 与 wide tolerance 原生顺序，再安全 trunc 为 i32 选择前视
source。无效 REFX 输出 missing；无效 REFXV 在 index 0 选当柱 raw source，其余
carry previous raw output。调用影响为 `NXTS/WAVEKX/SQJZ/ICHIMOKU`
4 formulas / 15 calls（分别 2/4/8/1）。

formula `native-operators`、`language` focused 通过，language 旧反向/夹取预期已纠正。
正式 `sz/000001` day 5 live 为
`R=[11.260000228881836,11.25,11.229999542236328,null,null]`、
`V=[11.260000228881836,11.25,11.229999542236328,11.229999542236328,11.229999542236328]`、
`engine=native`。该前视语义仍只属于 `allow_future=true` 的
`explicit-read-only-lookahead`，未改变 Level2 的 CLI-only、无 SDK/无网络/无订阅边界。

最终正式 PID `26104`；EXE SHA-256
`8c8c893ed3d652756361199861e751db906b0a95348e30913f72992adf7ffd4e`，部署网页
`index.html` SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`。health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`；PID `32284` 及其 EXE 基线现为上一
VALUEWHEN/BETWEEN 阶段历史。本批未运行完整 CTest；130/130 仍只归属
sdk-json-4653 阶段。

## LAST/NDAY 与 Z/D gate 后的共享部署边界

公式侧新增 `output/ida-tcalc-last-nday-targeted-20260813.json`：LAST
`sub_10005970` 为 210 条指令，NDAY `sub_100165F0` 为 177 条指令；末柱参数、leading
gate、float32 epsilon 与连续计数语义已经 focused 收口，实际影响分别为
`XRDS/QTDS` 2 formulas / 2 calls 和 `K300/K310` 2 formulas / 2 calls。

`output/ida-tdxw-type120-security-class-targeted-20260813.json` 与
`output/ida-tdxw-security-record-precision-displacements-20260813.json` 证明 TNF
`+76 price_precision` 不能替代 ZTPRICE/DTPRICE 的 `runtime+283`、`sub_5A3810`、
`type120+31` 上下文。四条相关公式 `B007/C128/C129/C130` 保持 executable
approximation，但为 numeric-degraded、`pure_ohlcv=false`；coverage 为 375 safe /
4 degraded，scan/backtest 拒绝，嵌套原因递归保留。本更正不改变 Level2 的 CLI-only、
无 SDK/host storage/网络/订阅边界。

formula target build 及 `native-operators/context-and-library/language` focused 通过，临时与
正式 focused API `health/evaluate/nested/scan/backtest` 通过；正式 `sz/000001` day 40
末柱 `L=1`、`D=0`，nested causes 为 `RGB + ZTPRICE`。未运行 full CTest/full API。
当前正式 PID `24556`，EXE SHA-256
`39483c732fa9a317cfc4b4674193c93a034f3703dc857508c5b61a63516bb36a`，web SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `26104` 与 EXE
`8c8c893ed3d652756361199861e751db906b0a95348e30913f72992adf7ffd4e` 为历史，PID
`32284` 是更早历史。回滚副本为
`output/tdx-tool-refx-pre-last-nday-zd-gate-predeploy-rollback-20260813.exe`。

## UPNDAY/DOWNNDAY 后的共享部署基线

公式侧 `output/ida-tcalc-upnday-downnday-targeted-20260813.json` 固定 UPNDAY
`0x10016330`/135 instructions 与 DOWNNDAY `0x10016490`/134 instructions：final N
float32 trunc、source leading gate、`first+N-1` 零初始化、`first+1` 比较、严格 tolerance、
`run==N` 回退 `N-1` 及 started sentinel raw 语义均已收口。系统库影响为
`UPN/K300` 2 formulas / 2 UPNDAY calls、`DOWNN/K310` 2 formulas / 2 DOWNNDAY calls。

formula target 与 focused `native-operators/language` 通过；正式 `sz/000001` day 800
末柱 `U=0`、`D=1`、`K=0`。未运行 full CTest/full API。当前正式 PID `17880`，EXE
SHA-256 `7f70d91d456598bfc6d3ecef3318e7fbebd162dac612a9d31efef06f73b5f05e`，web SHA-256
仍为 `71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `24556` 与 EXE
`39483c732fa9a317cfc4b4674193c93a034f3703dc857508c5b61a63516bb36a` 为历史。回滚副本为
`output/tdx-tool-last-nday-zd-gate-pre-upnday-downnday-predeploy-rollback-20260813.exe`。
以上只更正共享部署身份，不改变 Level2 的离线、CLI-only、无 SDK/网络/订阅边界。

## SDK 1803 depth / 18031 queue typed-domain-only 投影

新增两个纯 domain 投影，均未接入 CLI、HTTP 或 Level2Lab/UI。它们不是
既有 fixed-snapshot projection 的替代，而是把已有 handler 证据限定在类型化字段
投影层：

- `sdk-1803-depth-record-projection` 只接受 exact `32016 B` body。它保留
  `first/second` 两侧 raw 顺序，不推断买卖方向；两个 u32 count 分别
  clamp 到 1000。每条 13 B 记录为 `f32 price` + `u32 volume` +
  `zero u8` + `u16 auxiliary` + `rank u8` + `zero u8`，其中 source f64
  price 明确缩窄到 f32。两侧分别按 unsigned volume 选 top 3，严格
  greater-than 使并列保持最早候选；同时保留原生 candidate slots
  `{-1,0,0}` 的第二/第三候选缺失时 record-zero fallback。
- `sdk-18031-queue-record-projection` 只接受 exact `20012 B` body，u32
  count clamp 到 5000。它复用 1801/1802 已收口的 security classifier；
  `sub_594680` predicate 为真时对 u32 quantity 作无符号截断 `/10`，
  否则 `/1` 原值保留。每条 6 B 记录为 `zero u16 + u32 quantity`，
  并保留证据绑定的 `first_raw` / `second_output_raw=0` 输出顺序，不命名
  买卖方向。

两个 domain 都不回显原始 body，不保留输入；不加载或调用 SDK，不执行
callback，不投递 host message，不读写 host storage/state，不构造或发送
request/subscription/network bytes，也不构成 Level2 权限绕过。
`tdx-level2-sdk-1803-depth-record-projection-tests` 和
`tdx-level2-sdk-18031-queue-record-projection-tests` 均通过；既有
`tdx-level2-sdk-180x-host-projection-tests` 中的 1801/1802 classifier regression 也通过。

同批公式侧完成 STD 无改动复核及 LOWRANGE targeted 收口，详见 formula
日志；只作共享部署边界记录。本批未运行 full CTest/full API。当前正式
PID `24140`；EXE SHA-256
`8571ce09ac27110defa68ed1727a3ed14ce4b834b3c1c944f26eb7ea022cdd7b`，web SHA-256
`71216a8f097a97e1b1275db1c7bff0ed8a25d74c2b6839e05b3fad2ae04323e9`；health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `17880` 与 EXE
`7f70d91d456598bfc6d3ecef3318e7fbebd162dac612a9d31efef06f73b5f05e` 为历史基线。
回滚副本：
`output/tdx-tool-directional-nday-pre-lowrange-level2-projections-predeploy-rollback-20260813.exe`。

## 1803/18031 surface 与 4654/4651 双快照状态投影

1803 depth-record 与 18031 queue-record 已从 typed domain 接到确认 POST
`/api/v1/level2/project` 和 Level2Lab。1803 只接受 exact 32016 B inline hex，独立
clamp 两侧 raw count 并投影 13 B packed records；18031 只接受 exact 20012 B，
clamp raw count 并投影 6 B records。接口不接受路径/上传，不回显原 body，不调用
SDK、不写宿主状态、不投递消息、不联网或绕过授权。

`output/ida-tpbus-sdk-4654-4651-targeted-20260813.json` 支持两项新的 CLI-only
transition。`sdk-4654-dual-snapshot-transition` 要求调用者显式提供 nonempty decoded/
raw 文件，decoded 必须 exact 48 B，输出无条件 replacement state。
`sdk-4651-dual-snapshot-transition` 要求 decoded 至少 6 B 且 `+2` little-endian u32
为 `0xffffffff`，再按 previous raw logical size 为 0 或新 raw size 不小于 previous
决定 replace，否则 retain。previous size 只按 signed-int logical boundary 校验，
不读取/猜测 capacity。两项只输出大小、SHA-256、ready 与决策；decoded/raw body
不回显，不执行 raw-to-decoded 转换，也不调用 SDK/callback/宿主存储/网络。它们
没有 HTTP/UI；“dispatcher-qualified”只描述调用方已提供的两个非空输入，不代表
本工具执行了 tpbus dispatcher 或解码。4651 retained `projected_state` 已修为严格
九字段，并用连续两次 `--state-file` replay 锁定。

4654 正式 CLI exact-48 样本返回 ready=true，decoded/raw SHA-256 均为
`c3302bdab1288f5523babe39e64b1543f2a06f4704a86737c62e35ae11a1c0d5`，全部 SDK/
callback/host/network 标志为 false/0。新旧 fixed snapshot、4654/4651 聚焦 target
均通过；随后单线程完整构建和 CTest `135/135`、0 failed、`80.89 s`；Svelte check
0/0。本批未跑 full API suite。当前正式 PID `36100`，EXE SHA-256
`a5fb392bf25fe6f2d224bc681212ca4049ad6364391b40b8f5730f311e07da0a`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health
匹配，`native_cpp=true`、`python_runtime=false`。PID `33268` / EXE
`69ac767a2d9cea3bc53899a92d6deed8df89f8c125582bc766b7ff56658e0db0` 为历史。
回滚副本：
`output/tdx-tool-formal-69ac767a-pre-barsnext-refdate-465x-rollback-20260813.exe`。

## SDK 4655 companion/raw conditional candidate

新证据 `output/ida-tpbus-sdk-4655-downstream-targeted-20260813.json` 完整覆盖
dispatcher、shape validator 与 downstream handler。新增 CLI-only
`sdk-4655-companion-raw-transition` 接受 caller 从 prior local state 克隆的 exact 46 B
companion，以及已 dispatcher-qualified 的 raw。raw exact shape 为
`39 + 18*signed_i16_count + 120*signed_i8_attach`；有符号边界
`count=-1/attach=1/size=141` 同样合法，attach 复制区从 offset 21 起并只输出 SHA-256
摘要，不回显 bytes。

`sub_10079A02` 的 host-state gate 输入不在离线请求中，保持 unresolved；结果只称
conditional candidate，不声称 host replacement。该投影不进行 raw-to-companion 或其他
raw conversion，不调用 SDK/callback，不发 host message 或 network request。它只接 CLI，
没有 HTTP/UI，也未修改独立的 `sdk-json-4655` adapter。

4655 focused target 及同批 ZIG focused target 通过；完整单线程构建与 CTest
`136/136`、0 failed、`88.96 s`；Svelte 沿用本轮 0 errors / 0 warnings。本批未运行
full API suite。正式 PID `17824`，EXE SHA-256
`8623ec5d1326cd0dbdb901acab849fc5ee5fd6986549a1ae74c4ee757b9567ed`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `36100` / EXE
`a5fb392bf25fe6f2d224bc681212ca4049ad6364391b40b8f5730f311e07da0a` 为历史。回滚副本
`output/tdx-tool-formal-a5fb392b-pre-zig-4655-rollback-20260813.exe` 的 SHA-256 为
`a5fb392bf25fe6f2d224bc681212ca4049ad6364391b40b8f5730f311e07da0a`。

## 4655 compact evidence 与 ZIG selector 共享部署

4655 targeted evidence 已从 wide closure 收窄为 compact-v2：`213228 B`、7 functions；
旧 wide 版本另存 targeted-wide。此变更只减少证据范围与体积，不改变 4655 production
domain、CLI 或 offline boundary 的任何语义。

共享产物中的 ZIG selector 改为末端最多 10 组 raw-f32 相邻值、固定 `1e-5` 判断，
不再要求全序列恒定；正阈值主状态机未动。focused native-operators/language 通过，正式
day/12 `X=[99,3×11]` 的 ZIG 与 CLOSE selector 数组完全相等，且仍为
`explicit-read-only-lookahead`。本阶段未重跑 full CTest；先前 `136/136` 只属于 4655
阶段。

当前正式 PID `25764`，EXE SHA-256
`0e2b58844f822ea526e57f944484dad6ecac15d7c0994f925f3f2f60985fe694`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `17824` / EXE
`8623ec5d1326cd0dbdb901acab849fc5ee5fd6986549a1ae74c4ee757b9567ed` 为历史。回滚副本
`output/tdx-tool-formal-8623ec5d-pre-zig-selector-rollback-20260813.exe` 的 SHA-256 为
`8623ec5d1326cd0dbdb901acab849fc5ee5fd6986549a1ae74c4ee757b9567ed`。

## SDK 4650 targeted 证据与 BARSSINCE 共享部署

新增 `output/ida_probe_tpbus_4650_targeted_20260813.py` 及对应 JSON。当前证据可闭合
4650 raw shape，但不能脱离 host code/market、prior state、time 和对象状态
`+408/+804/+72/+496/+80` 复现完整分支，故本批没有实现 shape-only projection，避免
把局部 validator 误称为 host transition。后续
`output/ida-tpbus-sdk-4650-previous-state-targeted-20260813.json` 已闭合四个指定地址、
22 个完整函数和 58 个关键 offset xref：五字段可作 raw previous metadata，但完整输出
还依赖大量 quote/vector/identity/object 状态及 live/server time policy；typed 输入会
退化为宿主对象镜像，所以继续明确 do-not-implement。

共享产物中的 BARSSINCE 依据
`output/ida-tcalc-tr-barssince-targeted-20260813.json` 修正为 raw-f32 first scan：只跳
canonical sentinel/exact `±0.0f`，started 后计数不重置。`1e-50` 全 missing，leading
sentinel 后 `1e-8` 输出 `0..4`；NXTS 影响 1 formula / 5 calls。

focused native-operators/language 通过；未运行 full CTest（`136/136` 仍属上一 4655
阶段）或 full API。正式 PID `35408`，EXE SHA-256
`47ca2f7ea97c4a88a3a7904ddf496bf4c470428f2ca70c1e6413992b3c21a6b7`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `25764` / EXE
`0e2b58844f822ea526e57f944484dad6ecac15d7c0994f925f3f2f60985fe694` 为历史。回滚
`output/tdx-tool-formal-0e2b5884-pre-barssince-rollback-20260813.exe` 的 SHA-256 为
`0e2b58844f822ea526e57f944484dad6ecac15d7c0994f925f3f2f60985fe694`。

## MA/BARSLAST family 共享部署与 Level2 边界

本批 Level2 只复核现有 tpbus dispatcher 差集。4650 虽已有 raw shape 与 previous-state
targeted evidence，但输出仍依赖宽 quote/vector/identity/object 状态及 live/server time；
因此没有新增低价值 shape-only domain，也没有改变已有 SDK/HTTP/UI 安全边界。

共享 EXE 更新来自公式 MA/BARSLAST/BARSLASTCOUNT 的 raw-f32 修正。focused 两域、真实
day/800 及三个公式 HTTP 合约通过；未运行 full CTest/full API，`136/136` 仍属于上一
4655 阶段。正式 PID `34664`，EXE SHA-256
`b6e914030227e508e9234846173540b6e981f215a04801094a32908ebd48f34e`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 匹配且
`native_cpp=true`、`python_runtime=false`。PID `35408` / EXE
`47ca2f7ea97c4a88a3a7904ddf496bf4c470428f2ca70c1e6413992b3c21a6b7` 为历史。回滚
`output/tdx-tool-formal-47ca2f7e-pre-ma-barslastcount-rollback-20260813.exe` 的 SHA-256 为
`47ca2f7ea97c4a88a3a7904ddf496bf4c470428f2ca70c1e6413992b3c21a6b7`。

## SDK 4671 request builder 只读结论与 REF 共享部署

新增 `output/ida_probe_tpbus_4671_request_targeted_20260813.py` 与
`output/ida-tpbus-sdk-4671-request-targeted-20260813.json`。`sub_1006764B` 仅 22 条指令，
通过两个已闭合 vector helper resize 后复制 `this+728` 的宿主已有 opaque vector；它不
构造 4671 字段。既有 dispatcher `sub_1007CB4E` 接受 4671 做通用校验，但没有对应的
专用 host-state consumer。由此本批明确不实现低收益 identity-copy/hash domain，不把
宿主缓存误称成已恢复协议，也不改变已有 SDK/HTTP/UI/授权边界。

共享 EXE 更新仅来自 REF 双 raw-f32 gate。focused 两域、day/5 和三条正式公式 POST
契约通过；未运行 full CTest/full API，历史 `136/136` 仍归 4655。

正式 PID `25428`，EXE SHA-256
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb`；health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `34664` / EXE
`b6e914030227e508e9234846173540b6e981f215a04801094a32908ebd48f34e` 为历史；回滚
`output/tdx-tool-formal-b6e91403-pre-ref-rollback-20260813.exe` 的 SHA-256 为旧 EXE hash。

## 连板天梯共享 Web 部署与 Level2 不变边界

本阶段未新增 Level2 domain、CLI、HTTP 或 UI；4671 仍明确停在 opaque cached-vector
identity copy 的只读结论，没有把它包装成协议能力。共享 Web 新增的仅是成熟市场接口
`/data/limit-ladder`：两分类、六 activity、八 sort、分页与严格本地板块身份跳转，raw 和
实时字段均不展示。

Svelte check 0/0、Web build 通过；正式五条 focused GET 得到 248 条 live 行、复算
mismatch=0，非法 category 返回 400。本批未跑 full CTest/full API。

正式 PID `22616`，EXE SHA-256
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256
`da7da0814cb7745e9ab52537ad80c7e84b3cfc0cd033c5c0af8af761247558dd`；health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `25428` / 旧 web hash
`40726ee7c6053aa03eaa4adf5c3f866c5b6ee01c38aa7c995eb74cd511662eeb` 为历史。

## 开盘与盘后成交共享 Web 部署与 Level2 不变边界

本阶段未新增 Level2 domain、CLI、HTTP 或 UI；4671 opaque cached-vector identity copy
仍保持只读结论。共享 Web 新增的仅是成熟市场接口 `/data/session-turnover`：A 股 / ETF、
四类 activity、七类 sort、服务端分页与严格证券身份跳转，raw 和虚构实时字段均不展示。

Svelte check 0/0、Web build 通过；正式五条 focused GET 得到 A 股 5,515 条、ETF 1,624 条
live 记录，双活跃 3,838 条、单票命中 1 条，非法 universe 返回 400。本批未跑 full
CTest/full API。

正式 PID `34564`，EXE SHA-256
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256
`2bec35c186633960ef0d00e0ca2142aa9b1cd54b5d0e792f0268a5a3061464be`；health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `22616` / 旧 web hash
`da7da0814cb7745e9ab52537ad80c7e84b3cfc0cd033c5c0af8af761247558dd` 为历史。

## 资金信号后续表现共享 Web 部署与 Level2 不变边界

本阶段未新增 Level2 domain、CLI、HTTP 或 UI；4671 opaque cached-vector identity copy
仍保持只读结论。共享 Web 新增的仅是成熟市场接口 `/data/flow-followup`：两组历史、四组
分档模型、日期/可用性控制和显式北向占位审计；不回显 raw/upstream text，也不产生交易、
SDK、消息或网络协议侧副作用。

Svelte check 0/0、Web build 通过；正式 focused 合约得到历史 2,122/2,237 条、占位 466 条、
模型 11/11/16/16 档、current unavailable 与非法过滤 400。重启后一次 WinHTTP 12030 经
短重试恢复 live；本批未跑 full CTest/full API。

正式 PID `20508`，EXE SHA-256
`6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`，web SHA-256
`1929f3d7cb4cabeda51998e8f512512f82ac995ab1071800fd43f93d6992f371`；health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `34564` / 旧 web hash
`2bec35c186633960ef0d00e0ca2142aa9b1cd54b5d0e792f0268a5a3061464be` 为历史。

## 1803 / 18031 record projection CLI 补齐

已有 `sdk-1803-depth-record-projection` 与 `sdk-18031-queue-record-projection` domain、POST、
Level2Lab 现补 CLI-only 入口。新独立 command TU 只负责 exact raw/hex 预读与参数校验：
1803 为 32,016 B；18031 为 20,012 B 且要求 market 0..2、六位 code。raw/hex 上限分别
384/768 KiB；未知参数拒绝。它们继续输出 13 B / 6 B packed records、raw/effective/clamped
counts 和 classifier/divisor 证据，不回显 body，不调用 SDK、callback、host storage、message、
wire 或 network，也不绕过授权。

两个 affected targets 构建/运行通过，实际覆盖 raw 1803、hex 18031、packed bytes、大小、
identity、未知参数和全 offline flags；`tdx-tool` 构建通过。完整 CTest `136/136`、0 failed、
72.77 秒，正式 help smoke 含两个格式。HTTP/UI schema 未改，未跑 full API。

正式 PID `23872`，EXE SHA-256
`4ece5208d192711327888a29879a07a41d7889c42e37d8fc97dbf14642d5d16b`，web SHA-256
`1929f3d7cb4cabeda51998e8f512512f82ac995ab1071800fd43f93d6992f371`；health 与磁盘
匹配，`native_cpp=true`、`python_runtime=false`。PID `20508` / 旧 EXE hash `6c4d7c53...f3bc2`
为历史；rollback 为 `output/tdx-tool-formal-6c4d7c53-pre-record-cli-rollback-20260813.exe`，
SHA-256 `6c4d7c5324d0978e14fe9c2855f255a8e83a26423c8377e3f6a4b353b90f3bc2`。

## fnReqData 单证券计划 POST / Level2Lab 收口

`sdk-fnreqdata-plan` 与 `sdk-fnreqdata-18031-plan` 从 typed domain/CLI 扩展到现有 POST
`/api/v1/level2/build` 和 Level2Lab。普通计划 data type 只允许 1801/1802/1803/1804/18071；
1801/1802 强制显式完整 u32 cursor 与 count 1..1500，另外三类固定 cursor=0/count=1 并拒绝
override。18031 只接受 market 0..2、六位 code、u8 side-mode 和 finite float32 price，输出
selector、原 float bits 与 promoted double bits；其 correlation 仍是 unresolved host-global，
不伪造 25 B registry。

HTTP 顶层均为严格 flat whitelist，input/path/url/file 被拒绝；响应补
input_retained=false/file_accessed=false，domain 原有 SDK invoked、wire、network、request、
subscription 和 entitlement-bypass 边界不变。Level2Lab 只增加枚举、游标/数量、raw side 与
价格控件，不增加文件、句柄、token 或授权入口。

`tdx-server-level2-tests` / `tdx-server-catalog-tests` 通过，Svelte check 0/0、Web build 435
modules 通过（仅既有 chunk warning），完整单线程 build 与 CTest `136/136`、0 failed、
66.86 秒通过。正式 API smoke：1801 cursor=`4294967295`/count=`1500`/12 slots；1803
fixed 0/1；18031 side=1→selector=0、10.25F→`0x4024800000000000`、no-25B/no execution；
fixed override 与 path 注入均返回 400。

正式 PID `10404`，EXE SHA-256
`90d95a18162d36441d36d4591eb95f25b5d146e94c4cd72647892f77f59f9b13`，web SHA-256
`1edf97c14f8b17d5e5ef2989709ba47d652a2a847c597edc50a56f46daf91cd2`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `23872` / EXE `4ece5208...d5d16b` 为历史；
rollback `output/tdx-tool-formal-4ece5208-pre-fnreqdata-surface-rollback-20260813.exe` 的
SHA-256 为 `4ece5208d192711327888a29879a07a41d7889c42e37d8fc97dbf14642d5d16b`。

## ZIG 解释器阶段的 Level2 边界

本阶段只修改百分比 `ZIG` 的纯解释器状态机；Level2 typed domain、CLI、POST、Level2Lab、
SDK/callback/message/network/authorization 边界和 schema 均未改变。4650 的完整状态投影仍因
宽 previous-host-state 与 live-time policy 未闭合而保持 do-not-implement，不新增 shape-only
或授权相关入口。

公式 focused、完整 build 与 CTest `136/136`、0 failed、65.96 秒通过；未跑 full API。
正式 PID `464`，EXE SHA-256
`cfb8ed2317545c67509b0477bd3b346f72f20954f17c2bc537b360775b042def`，web SHA-256
`1edf97c14f8b17d5e5ef2989709ba47d652a2a847c597edc50a56f46daf91cd2`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `10404` / 旧 EXE `90d95a18...f9b13` 为历史；
rollback `output/tdx-tool-formal-90d95a18-pre-zig-native-state-rollback-20260813.exe` 的 SHA-256
为 `90d95a18162d36441d36d4591eb95f25b5d146e94c4cd72647892f77f59f9b13`。

## 期权工作台阶段的 Level2 边界

本阶段没有修改 Level2 typed domain、SDK JSON、callback/message、授权或 wire 行为。新增的是
非 Level2 的期权数据页 `/data/options`，以及期权到期/波动率/链响应的 root-relative 资源来源
投影；它不会调用 Level2 SDK，也不提供文件、路径、token、handle 或订阅入口。

期权 focused、Svelte check 0/0、Web build、完整 build 与 CTest `137/137`、0 failed、65.45 秒
通过。正式 A2609 目录/链/到期/波动率合约通过，响应中的规则和节假日文件仅为 TDX-root-relative
路径且不含 `C:\new_tdx`；本批未跑 full API suite。

正式 PID `33236`，EXE SHA-256
`cd1793637f52345a43a5978c3e40020848cc7af8321159d95d3d4884b8464cce`，web SHA-256
`40cf58f4cfd5e942404ff98454464be0b444859f05e572f7b373f9ac9e0e8aed`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `464` / EXE `cfb8ed23...42def` 为历史；rollback
`output/tdx-tool-formal-cfb8ed23-pre-option-surface-rollback-20260813.exe` 的 SHA-256 为
`cfb8ed2317545c67509b0477bd3b346f72f20954f17c2bc537b360775b042def`。

## 本地参考档案阶段的 Level2 边界

本阶段没有修改 Level2 typed domain、SDK JSON、callback/message、授权、订阅或 wire 行为。
新增 `/data/local-reference` 只读页面以及 historical-securities、index-events、fund-reference
的共享 root-relative 来源投影；三个接口均读取本地 TDX 资源，`network_requests=0`，不提供
文件、路径、token、handle 或 SDK 入口。

`TdxW!sub_4F4A50` 证据用于修正非 Level2 的 ETF 空日期兼容：正式基金目录现返回 4218 条，
2 条 lifecycle 未定；三类来源均为 TDX-root-relative，响应不含 `C:\new_tdx`。相关 focused、
Svelte 0/0、Web build、完整 build 与 CTest `138/138`、0 failed（测试耗时合计 63.33 秒）通过；
本批未跑 full API suite，Level2 离线/无授权绕过边界保持不变。

正式 PID `26472`，EXE SHA-256
`abbff3821c137e65e08630aa07373325530dd10f1605f040bb3e203fde4f131d`，web SHA-256
`b907273787fe625d39ada3519218e259f84fcc6209f356ea76596e979c5d2abc`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `33236` / EXE `cd179363...64cce` 为历史；rollback
`output/tdx-tool-formal-cd179363-pre-local-reference-rollback-20260813.exe` 的 SHA-256 为
`cd1793637f52345a43a5978c3e40020848cc7af8321159d95d3d4884b8464cce`。

## 港股本地档案阶段的 Level2 边界

本阶段没有修改 Level2 typed domain、SDK JSON、callback、宿主消息、授权、订阅或 wire 行为。
新增能力是非 Level2 的 `/data/hk-reference`，以及 HK actions / finance 本地来源的共享
TDX-root-relative 投影；Web 不提供文件、路径、token、handle 或 SDK 入口，两接口
`network_requests=0`，正式响应不含 `C:\new_tdx`。

正式样本为公司行动 32400 条、财务 3238 条；`HK00001` 分别命中 51 / 1，页面 200，非法
四位码 400。相关四个 focused tests、Svelte 0/0、Web build、完整 build 与 CTest `138/138`、
0 failed（64.14 秒）通过；本批未跑 full API suite，既有 Level2 离线/不绕过授权边界不变。

正式 PID `11040`，EXE SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`，web SHA-256
`fbd587b6d028419287e6d070af50968795a1b49f7d9d45524104a70b1e43b5ba`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `26472` 为上一阶段；rollback
`output/tdx-tool-formal-abbff382-pre-hk-local-reference-rollback-20260813.exe` 的 SHA-256 为
`abbff3821c137e65e08630aa07373325530dd10f1605f040bb3e203fde4f131d`。

## 路演 Web / MA evidence 阶段的 Level2 边界

本阶段没有修改 Level2 typed domain、SDK、callback/message、授权、订阅或 wire 行为。新增的是
非 Level2 的固定路演 Web surface，以及 MA 的只读 IDA 闭包；路演虽使用 TQLEX，但 Web 不开放
任意 entry/key/body，不能用于 SDK 或授权绕过。既有 4650 do-not-implement 结论保持：缺少稳定、
窄化的 previous-host-state 与 live-time 契约时不构造伪完整投影。

路演/recon focused、Svelte 0/0、Web build 通过；本阶段未重跑 full CTest，上一阶段 138/138。
正式 PID `5760`，EXE SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`，web SHA-256
`860818fd92289d42134ef08b59cd0142a41b7e94b7b14ec3ee16035f01d54b2b`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `11040` 为上一阶段；executable rollback 不变。

## 扩展市场行情 Web 阶段的 Level2 边界

本阶段没有修改 Level2 typed domain、SDK、callback/message、授权、订阅或 wire 行为。
`/data/expansion-market` 使用公开 7727 合约目录、快照、分时与逐笔 GET；它不接收 SDK DLL、token、
callback key、服务器、文件或原始请求，也不改变 4650 do-not-implement 和其他离线投影边界。
正式聚焦验证覆盖目录、快照、历史分时、历史逐笔、A 股拒绝与页面 200；`tdx-native-tests`、
Svelte 0/0、Web build 447 modules 通过。本批未重跑 full CTest/full API，138/138 属上一完整阶段。

正式 PID `17772`，EXE SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`，web SHA-256
`7ddbaba11960804a4b51e107ca955e70ded526aeb4b277fb9eab57632bb60ef6`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `5760` 为上一阶段；executable rollback 不变。

## 4654 / 4651 / 4655 HTTP 与 Level2Lab 收口

固定 `POST /api/v1/level2/project` 新增三条严格分派，直接复用既有 typed domain：

- `sdk-4654-dual-snapshot-transition`：精确 48 B decoded companion + 显式 raw，投影无条件 replace；
- `sdk-4651-dual-snapshot-transition`：decoded 至少 6 B 且 `+2` LE u32=`0xffffffff`，可带严格九字段
  previous state；按 raw logical size replace/retain，返回的 `projected_state` 可原样继续回放；
- `sdk-4655-companion-raw-transition`：精确 46 B caller-cloned companion + dispatcher-qualified raw；
  host-state gate 离线不可求值，只输出 conditional candidate，绝不声称 actual host replacement。

三类顶层对象均为精确白名单，仅收内联 hex/previous，HTTP body 总上限 384 KiB。path、URL、file、
upload、server、token、handle、endpoint、payload/document/encoding 等无关字段均拒绝；响应不含原始
hex/body，`input_retained=false`、`file_accessed=false`，且 SDK/callback/message/network/request/
subscription/entitlement side effects 全为 false/0。Level2Lab 新面板实现同一格式选择、长度与 shape
校验，并支持 4651 state replay；不提供路径或上传。

`tdx-level2-sdk-4654-dual-snapshot-transition-tests`、
`tdx-level2-sdk-4651-dual-snapshot-transition-tests`、
`tdx-level2-sdk-4655-companion-raw-transition-tests`、`tdx-server-level2-project-tests` 与
`tdx-server-catalog-tests` 全部通过。Svelte check 0/0，Web build 447 modules（仅既有 chunk warning），
完整单线程 build 与 CTest `138/138`、0 failed（66.59 秒）通过。

正式 focused：4654 的 48+12 B 输入返回 replace；4651 首次 8+7 B replace，随后用返回 state 提交
8+6 B 得 retain 且 projected raw size 仍为 7；4655 的 46+57 B 输入为 dispatcher-qualified，
`host_state_gate.evaluated=false`、`actual_host_replacement_claimed=false`；非法 path 返回 400。
`/api/v1/features`、`/api/v1/openapi.json` 与 `/protocol/level2` 新 bundle 均核验通过。未跑 full API suite。

当前正式 PID `33660`，EXE SHA-256
`75e11ac73bb68e1190bde4a953e8acba10da393c6987d2fb9eb53145590ae5fb`，web SHA-256
`71a3bc22a8999cb638e8c6e0f520a5d6f521cb6c6aa2ab68f5af6bcb27bb8f39`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `17772` / EXE `6622e99e...918f8` / web
`7ddbaba1...60ef6` 为上一阶段；rollback 为
`output/tdx-tool-formal-6622e99e-pre-level2-465x-surface-rollback-20260813.exe`，SHA-256
`6622e99ebfde43729203ed20a82abe4164812012e55876578ae49633a60918f8`。

## 0x053E Web 阶段的 Level2 边界

本阶段没有修改 Level2 SDK、callback、project domain、消息、授权、订阅或 wire 行为。个股新增的
“涨速盘口”消费公开 L1 `0x053E` `/api/v1/market/speed`，与 Level2Lab 及 465x 离线投影完全分离；
页面不提供 SDK DLL、token、handle、服务器、文件、路径或原始请求入口。未恢复含义的 0x053E
尾字段保持 raw，不借 Level2 名义猜测。

`tdx-native-tests`、Svelte 0/0、Web build 449 modules 和正式 speed/page 样本通过；本批没有重跑
full CTest/full API，上一 465x surface 的 138/138 保持有效。

正式 PID `12628`，EXE SHA-256
`75e11ac73bb68e1190bde4a953e8acba10da393c6987d2fb9eb53145590ae5fb`，web SHA-256
`9bcb7d1e038a9b830446d22f32a6aeba58f210f414f01c9a3e532eb2c8be7e01`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `33660` 为上一 465x 阶段；rollback 不变。

## TOPRANGE 解释器阶段的 Level2 边界

本阶段只修改公式 TOPRANGE 数值 helper 与 focused tests，并生成 TCalc targeted evidence；没有改变
Level2 SDK、callback、project schema、授权、订阅、消息或 wire 行为。上一 4654/4651/4655
HTTP/Level2Lab 与 0x053E 涨速 Web 能力保持原有安全边界。

formula focused、完整 build 与 CTest `138/138`、0 failed（66.76 秒）通过；未跑 full API suite。
正式 TOPRANGE 边界样本和既有 speed/page smoke 均通过。

正式 PID `20808`，EXE SHA-256
`cf4da92cf849ad4cecdce5a15ebb635c53c561db29bba768c00da472eec35a12`，web SHA-256
`9bcb7d1e038a9b830446d22f32a6aeba58f210f414f01c9a3e532eb2c8be7e01`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `12628` 为上一阶段；rollback 为
`output/tdx-tool-formal-75e11ac7-pre-toprange-native-rollback-20260813.exe`，SHA-256
`75e11ac73bb68e1190bde4a953e8acba10da393c6987d2fb9eb53145590ae5fb`。

## 行情流 Hub 状态 Web 阶段的 Level2 边界

本阶段没有修改 Level2 SDK、callback、project/decode schema、授权、订阅、消息或 wire 行为。
系统页新增的状态卡固定消费现有 L1 `/api/v1/market/stream/status`，展示本地 SSE Hub 的订阅与轮询
健康，并把 `FastHQ.Subscribe` 未调用的边界常显；它本身不会创建订阅或触发网络请求，也没有
SDK DLL、token、handle、server、path、file 或 raw request 入口。

`tdx-market-stream-tests`、Svelte 0/0 与 Web build 449 modules 通过；正式空闲态为
`tdx-market-l1-stream-status-v1` / `0x0547`、subscriber/security=0/0、effective interval=15000 ms、
FastHQ=false，系统页新 bundle 为 `assets/index-AXPa2fLz.js`。本批未重跑 full CTest/full API；
最近完整 138/138 仍属 TOPRANGE 阶段。

正式 PID 仍为 `20808`，EXE SHA-256 仍为
`cf4da92cf849ad4cecdce5a15ebb635c53c561db29bba768c00da472eec35a12`，web SHA-256 为
`6419fd8c46c069a5627aa4278551f9d07de91fb37092294ea1bfff405f7ae66a`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。上一 web `9bcb7d1e...8be7e01` 为历史，rollback 不变。

## PEAK/TROUGH 解释器阶段的 Level2 边界

本阶段只修改公式 future-domain 的 PEAK/PEAKBARS/TROUGH/TROUGHBARS 投影并新增 TCalc targeted
evidence；没有修改 Level2 SDK、callback、project/decode schema、授权、订阅、消息或 wire 行为。
上一 4654/4651/4655 HTTP/Level2Lab 以及行情流状态页的离线/只读边界保持不变。
公式 evidence SHA-256 为 `0e79e8308eafb1af852bc6c62ce139a065086f9545857ca813dcbbefdae77ea6`。

formula 三个 focused 域、完整 build 与 CTest `138/138`、0 failed（66.94 秒）通过；正式自定义和
内置 XT 样本均以 `explicit-read-only-lookahead` 执行。本批未跑 full API suite。

当前正式 PID `35944`，EXE SHA-256
`ea666f23ac9af9cb7decb9d2608014bd48194e52f73227e8560a7c6fba0dcbdd`，web SHA-256
`6419fd8c46c069a5627aa4278551f9d07de91fb37092294ea1bfff405f7ae66a`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `20808` 为上一行情流状态页阶段；rollback 为
`output/tdx-tool-formal-cf4da92c-pre-peak-trough-native-rollback-20260813.exe`，SHA-256
`cf4da92cf849ad4cecdce5a15ebb635c53c561db29bba768c00da472eec35a12`。

## TPBus PushType 115 批容器（2026-08-14）

新增收窄脚本/证据 `output/ida_probe_tpbus_push_115_targeted_20260813.py` 与
`output/ida-tpbus-push-115-targeted-20260813.json`。JSON 为 91268 B，SHA-256
`a7dbb5161c541c3bf3e584d83acf6ac0e428e9f47d491cb473274de1b50c77d7`，完整包含 dispatcher
`sub_1007BFBE`、sequence initializer `sub_101A5AD0`、LE-u16 length reader `sub_101A6060` 与 LE-u32
reader `sub_10066392` 的 pseudocode、instructions、direct calls。dispatcher 的 36 个其他 runtime/
EventBus/host side-effect call 均分类为离线 envelope 外，不据此执行或猜测。

public `Level2Tpbus115BatchDecodeRequest` 与独立 `level2_tpbus_115.cpp` 只解包调用方已取得的 PushBody：
首个 outer u16-length segment 内迭代 u32 discriminator、u16 body length 和 body；0/1 映射 push 111，
其他映射 112；zero body 返回 null/skip，不完整 tuple 以明确 stop reason 停止，outer trailing 只计数。
非空 body 仅输出 source order、raw discriminator、mapped type、byte size、SHA-256 和可用时既有 111/112
摘要，原文不保留/不发出。offline boundary 固定 EventBus/host/SDK/callback/message/wire/network/
request/subscription/authorization 为 false/0，entitlement bypass=false。

CLI-only `level2 decode --format tpbus-115 --input FILE --encoding raw|hex` 采用独立有界 reader；raw
预读/解码上限 384 KiB，hex 文本也先限长，拒绝 xor/protobuf 等跨格式选项。没有 HTTP/Level2Lab
入口。独立 focused 覆盖混合 111/112、discriminator 0/1/other、zero skip、outer/inner 截断、trailing、
384 KiB、raw/hex CLI 与全部零副作用；完整 build/CTest `139/139`、0 failed（13.12 秒）通过。

正式 CLI 1 B body 样本映射 112、`summary_available=false`、EventBus=false、network=0；同批 IF/audit/
page smoke 通过，未跑 full API suite。当前正式 PID `32500`，EXE SHA-256
`44f513e8c5b8978395306743dd6078bf21a0d0826c7a75a1af2fedbfc91754fe`，web SHA-256
`e6dc95ac299879cdeca17fc5c6df230c8c11fdf4c2a5f082f3df6482e3156a64`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `35944` 为上一 PEAK/TROUGH 阶段；rollback 为
`output/tdx-tool-formal-ea666f23-pre-if-tpbus115-native-rollback-20260814.exe`，SHA-256
`ea666f23ac9af9cb7decb9d2608014bd48194e52f73227e8560a7c6fba0dcbdd`。

## 共享部署更新：取整标量与云工作流 Web（2026-08-14）

本批未改变 Level2 domain、CLI、HTTP/UI、SDK/host/message/network 或授权边界。共享 EXE 变化来自
ROUND/CEILING/FLOOR 的 TCalc raw-f32/i32 原生语义；证据
`output/ida-tcalc-scalar-rounding-targeted-20260814.json` 为 143561 B，SHA-256
`24a5974f9ab23d7fd78502e6dae12d6d3b7bf6e7f6505ae50c859bfc5d6b9629`。Web 仅恢复既有
固定 TQLEX/PBRPC master-detail workflow consumer，不触碰 Level2 授权，也不提供任意 URL/path/body。

native-scalars/language/native-operators、Svelte 0/0、Web build、完整 build 与 CTest `139/139`、0 failed
（12.59 秒）通过；正式 workflow 与公式 smoke 通过，未跑 full API suite。当前正式 PID `20792`，
EXE SHA-256 `230837462287aa23136c93598ac4227c5c470a6b3c29155e09d34093af4db1a3`，
web index SHA-256 `45b19054a9a142e19e78f34db5422bda668c544d99015008695f12b30d1e6543`；
health 匹配，`native_cpp=true`、`python_runtime=false`。PID `32500` 为历史；rollback 为
`output/tdx-tool-formal-44f513e8-pre-round-workflows-rollback-20260814.exe`，SHA-256
`44f513e8c5b8978395306743dd6078bf21a0d0826c7a75a1af2fedbfc91754fe`。

## 共享部署更新：EXP/LN/LOG 与协议覆盖页面（2026-08-14）

本阶段没有改变 Level2 API/domain/CLI、SDK、host callback、消息、网络或授权边界；TPBus 115 仍是
CLI-only 离线解包。共享 EXE 变化仅来自公式 EXP/LN/LOG 的 raw-f32 type-3/Series 原生语义，证据仍为
`output/ida-tcalc-scalar-rounding-targeted-20260814.json`。Web 新 `/protocol/coverage` 只读展示 cloud/JSN
模板的 typed-command 覆盖，不发送云请求、不下载 JSN，也不接触 Level2。

native-scalars/language、Svelte 0/0、Web build、完整 build 与 CTest `139/139`、0 failed（11.32 秒）
通过；其中现有 Level2/TPBus 115 tests 全部通过。coverage 与正式 formula smoke 通过，未跑 full API。
当前正式 PID `22240`，EXE SHA-256
`1ea2ee8694166660aecdb0476f1b9619174e10182b3e71e55ea4bea061a7ac66`，web index SHA-256
`5d90c8cffe68dafcaa0647ef3f7ef9b59118ff7fced8706ec3205eb7257fdb8a`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `20792` 为上一阶段；rollback 为
`output/tdx-tool-formal-23083746-pre-coverage-exp-log-rollback-20260814.exe`，SHA-256
`230837462287aa23136c93598ac4227c5c470a6b3c29155e09d34093af4db1a3`。

## 共享部署更新：scalar trigonometric/fraction/sign（2026-08-14）

本批仅改变公式 scalar domain，并新增两份 TCalc targeted evidence；Level2、TPBus、SDK、host、message、
network、authorization 与所有 HTTP/UI 合约不变。ACOS/ASIN/TAN、SIGN/SGN、FRACPART 的实现与边界
见 formula 日志；ATAN/COS/SIN 依赖未知 buffer metadata，未猜实现。

native-scalars/language、tdx-tool link 与完整 CTest `139/139`、0 failed（48.46 秒）通过，其中现有
Level2/TPBus 测试全部通过。正式 formula smoke 通过，未跑 full API。当前 PID `16816`，EXE SHA-256
`aedd9b545147943f7d1486f8fea57ebeaa03b5ae446d246faa7ce8e7c94de5a0`，web index SHA-256 沿用
`5d90c8cffe68dafcaa0647ef3f7ef9b59118ff7fced8706ec3205eb7257fdb8a`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `22240` 为上一阶段；rollback 为
`output/tdx-tool-formal-1ea2ee86-pre-trig-fraction-sign-rollback-20260814.exe`，SHA-256
`1ea2ee8694166660aecdb0476f1b9619174e10182b3e71e55ea4bea061a7ac66`。

## TPBus PushType 115 HTTP / Level2Lab 收口（2026-08-14）

既有 `Level2Tpbus115BatchDecodeRequest` 与 `decode_level2_tpbus_115_batch` 现由 POST
`/api/v1/level2/decode` 和 Level2Lab 复用。HTTP body 精确只允许 `format`、`payload_hex`、`limit`，
解码后上限 384 KiB；path/file/url/document 等均被通用严格白名单拒绝。Level2Lab 新增
`tpbus-115` 格式及首段/inner tuple/零长跳过/截断边界说明，不提供路径、上传、EventBus、SDK 或
消息控件。

domain 仍只消费首个 LE-u16 segment，inner 按 raw 顺序解出 `u32 discriminator + u16 body_len +
body`；0/1→111，其余→112，body_len=0 成功消费并跳过，三类 inner 截断在失败游标停止。
非空 body 仅保留 tuple 顺序、raw discriminator、mapped type、size、SHA-256 及捕获 Error 后降级的
111/112 摘要；不回显 body。`input_body_retained/body_bytes_emitted/event_bus_accessed/sdk_called/
callback_executed/host_message_dispatch_attempted/request_sent/subscription_sent/entitlement_bypass=false`，
`network_requests=0`。

独立 `tdx-level2-tpbus-115-batch-tests`、server Level2、catalog focused 与 Svelte check 0/0 通过；
Web production build 453 modules（仅既有大 chunk warning）。完整串行 build 与 CTest `139/139`、
0 failed（49.84 秒）。正式 API fixture `0700000000000100aa` 返回 1 条 mapped 111、summary unavailable、
无 body；outer truncated 返回 `first-segment-truncated`，path 返回 HTTP 400，OpenAPI 与
`/protocol/level2` 均为新版本。未跑 full API suite。

当前正式 PID `22696`，EXE SHA-256
`8a443ece0a78f59117cd9bc3c93a4c9b330e7041ea92c291b12dc3c53efca2c3`，web index SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `16816` / EXE `aedd9b54...e5a0` 为上一阶段；rollback
为 `output/tdx-tool-formal-aedd9b54-pre-tpbus-115-rollback-20260814.exe`，SHA-256
`aedd9b545147943f7d1486f8fea57ebeaa03b5ae446d246faa7ce8e7c94de5a0`。

## 共享部署更新：CONST / ROUND2（2026-08-14）

本批只改变 formula scalar domain 并新增 CONST/ROUND2 targeted evidence；Level2、TPBus 115、SDK、
host callback、EventBus、message、network、authorization 及其 HTTP/UI 合同不变。native-scalars、
language、tdx-tool link 与正式 formula smoke 通过；本批未重跑 full CTest/API，最近完整 CTest 仍为
TPBus 115 surface 阶段 `139/139`、0 failed（49.84 秒）。

当前 PID `32380`，EXE SHA-256
`cf4ea0bc7df181f7310fcaedb3d9c53da73ffba5a08c020ef31ef5807640342e`，web index SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `22696` 为上一 TPBus 115 阶段；rollback 为
`output/tdx-tool-formal-8a443ece-pre-const-round2-rollback-20260814.exe`，SHA-256
`8a443ece0a78f59117cd9bc3c93a4c9b330e7041ea92c291b12dc3c53efca2c3`。

## 共享部署更新：CONSTA（2026-08-14）

本批只改变 formula scalar domain 并新增 CONSTA targeted evidence；Level2、TPBus 115、SDK、host
callback、EventBus、message、network、authorization 及 HTTP/UI 合同均不变。native-scalars、language、
tdx-tool link 与正式 formula smoke 通过；未重跑 full CTest/API，最近完整 CTest 仍为 TPBus 115
surface 阶段 `139/139`、0 failed（49.84 秒）。ATAN/COS/SIN 的隐藏 buffer metadata 仍明确未闭合。

当前 PID `31856`，EXE SHA-256
`31452ab93214a1ae803c4ddab766cd0f72aff524450a68bee5a1cb829c32d9d1`，web index SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `32380` 为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-cf4ea0bc-pre-consta-rollback-20260814.exe`，SHA-256
`cf4ea0bc7df181f7310fcaedb3d9c53da73ffba5a08c020ef31ef5807640342e`。

## 共享部署更新：IF / IFF / IFN / RANGE（2026-08-14）

本批只改变 formula scalar/support semantics，并新增 `output/ida-tcalc-ifn-range-targeted-20260814.json`
（40251 B，SHA-256 `e414d1e1744c9d16826a834d05be81c288f9755b3c56b2eb71082a280b125f39`）；Level2、TPBus 115、
SDK、host callback、EventBus、message、network、authorization 与 HTTP/UI 合同均不变。IF-family
condition 改为 raw-f32 exact-zero，RANGE 改为 bounds-only leading gate 与 raw-f32 tolerance；详细语义见
formula 日志。native-operators、native-scalars、language、tdx-tool link 与正式 formula smoke 通过；
未重跑 full CTest/API，最近完整 CTest 仍为 TPBus 115 surface 阶段 `139/139`、0 failed（49.84 秒）。

当前 PID `14624`，EXE SHA-256
`0c2e62f79911cf82a7c0bbff05c84c0924b2a338b25431a649c23b628ce506a2`，web index SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `31856` 为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-31452ab9-pre-ifn-range-rollback-20260814.exe`，SHA-256
`31452ab93214a1ae803c4ddab766cd0f72aff524450a68bee5a1cb829c32d9d1`。

## 共享部署更新：ADD / EQUAL / NOT_EQUAL（2026-08-14）

本批只改变 formula operator semantics，并新增
`output/ida-tcalc-add-equal-targeted-20260814.json`（63420 B，SHA-256
`64e8386daa893d239c84481dab5772563da44dbd7048d45ef360d9a69fccc3d8`）；Level2、TPBus 115、
SDK、host callback、EventBus、message、network、authorization 与 HTTP/UI 合同均不变。ADD 使用
raw-f32/sentinel，EQUAL/NOT_EQUAL 使用原生 strict/inclusive ±1e-5；详细语义见 formula 日志。
native-binary、language、native-operators、全部 formula-engine domains、tdx-tool link 与正式 smoke
通过；未重跑 full CTest/API，最近完整 CTest 仍为 TPBus 115 surface 阶段 `139/139`、0 failed
（49.84 秒）。

当前 PID `2800`，EXE SHA-256
`b8f871b3da1c0ca8a7b939a8a74aa9b96916c99f9e1c718db1d78ddc41ea6d92`，web index SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `14624` 为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-0c2e62f7-pre-add-equality-rollback-20260814.exe`，SHA-256
`0c2e62f79911cf82a7c0bbff05c84c0924b2a338b25431a649c23b628ce506a2`。

## 共享部署更新：unary minus raw-f32（2026-08-14）

本批仅修正 formula runtime 的一元 `-`：隔离 compile probe 证明 TCalc 将其降为 raw-f32
`-1.0F * value`，production 复用已闭合 multiply primitive。Level2、TPBus 115、SDK、host callback、
EventBus、message、network、authorization、HTTP/UI 合同均不变；production 不加载 TCalc.dll。
native-binary、language、native-operators、全部 formula-engine domains、tdx-tool link 与正式 formula
smoke 通过；未跑 full CTest/API，最近完整 CTest 仍为 TPBus 115 surface 的 `139/139`、0 failed
（49.84 秒）。

当前 PID `28016`，EXE SHA-256
`e56a01e20b2e1a63d6f632a06239588c3afc6cceff8d94f1583dd1e69c3bd0a1`，web index SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `2800` 为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-b8f871b3-pre-unary-negation-rollback-20260814.exe`，SHA-256
`b8f871b3da1c0ca8a7b939a8a74aa9b96916c99f9e1c718db1d78ddc41ea6d92`。

## 共享部署更新：numeric literal raw-f32（2026-08-14）

本批只把 formula numeric AST node 的执行值从 parser double 落到原生 raw-f32；Level2、TPBus 115、
SDK、host callback、EventBus、message、network、authorization、HTTP/UI 合同均不变。全部
formula-engine domains、tdx-tool link、正式 literal/CCI smoke 通过；未跑 full CTest/API，最近完整
CTest 仍为 TPBus 115 surface 的 139/139（49.84 秒）。

当前 PID `31728`，EXE SHA-256
`7738a999e23a8f9d09142791a16062dcd30cd676456deb479d8542633bc04517`，web index SHA-256 沿用
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 与磁盘匹配，
`native_cpp=true`、`python_runtime=false`。PID `28016` 为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-e56a01e2-pre-numeric-literal-rollback-20260814.exe`，SHA-256
`e56a01e20b2e1a63d6f632a06239588c3afc6cceff8d94f1583dd1e69c3bd0a1`。

### 共享部署补充：formula parameter f32

同批将 formula active parameter 的 execution/response 与派生 MTM N 统一为 f32；HTTP 参数上限
没有放宽，Level2/TPBus/SDK/host/message/network 边界均不变。全 formula-engine domains 与 link
复验通过；未跑 full CTest/API。

最终 PID `33640`，EXE SHA-256
`8fd3b43478fb823cd35213dfcda385afcc6472dc05ce1b7b7b66dc23fae3c49a`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `31728` 为中间态；rollback 为
`output/tdx-tool-formal-7738a999-pre-parameter-float-rollback-20260814.exe`，SHA-256
`7738a999e23a8f9d09142791a16062dcd30cd676456deb479d8542633bc04517`。

## 共享部署更新：core K-line raw-f32 fields（2026-08-14）

本批仅改变 formula environment：OPEN/HIGH/LOW/CLOSE/VOL/AMOUNT 按 TCalc packed record/result
buffer 落 raw-f32，普通股票 VOL 先收窄源再除 100；原行情 point、schema、Level2/TPBus/SDK、
callback/message/network/authorization 边界均不变。targeted evidence 为
`output/ida-tcalc-price-field-handlers-targeted-20260814.json`（555004 B，SHA-256
`33c32e34369cbc449c4be0e4ceee6f4c39e4087744391e229b0f4e28379fb66a`）。

native-binary、全部 formula-engine domains、tdx-tool link、真实 day5 CLI 与正式 POST 通过；未跑 full
CTest/API，最近完整 CTest 仍为 TPBus 115 的 139/139（49.84 秒）。当前 PID `25212`，EXE SHA-256
`9bb181de4c87bdcc2461ec87c0234828dd48dc68ff4242f4cec291d77062a645`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配，native true、
python false。PID `33640` 为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-8fd3b434-pre-price-field-float-rollback-20260814.exe`，SHA-256
`8fd3b43478fb823cd35213dfcda385afcc6472dc05ce1b7b7b66dc23fae3c49a`。

### 共享部署补充：auxiliary K-line raw-f32

本批仍仅改变公式环境，Level2/TPBus/SDK/host/message/network/authorization 边界不变。ZSTJJ/QHJSJ/
HKSHORTVOL 的 packed +31 f32 与 VOLINSTK u32→f32 证据位于
`output/ida-tcalc-auxiliary-field-handlers-targeted-20260814.json`（438719 B，SHA-256
`c990604f58f8d0ba2fffbce3f58f51baa0922b72948c7ec5151e972fb5e07b15`）。

全部 formula-engine domains、tdx-tool link、真实 IFL9 day5 与正式 POST 通过；未跑 full CTest/API。
最终 PID `27868`，EXE SHA-256
`bfdc14bc0e4c118c21030dc362c6ae61bc821e4a0e1f2e2f95d93f78c5335bd0`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `25212`
为上一中间态；rollback 为 `output/tdx-tool-formal-9bb181de-pre-auxiliary-field-float-rollback-20260814.exe`，
SHA-256 `9bb181de4c87bdcc2461ec87c0234828dd48dc68ff4242f4cec291d77062a645`。

## 共享部署更新：scalar formula context raw-f32（2026-08-14）

本批只收窄 CAPITAL/TOTALCAPITAL/MINDIFF/MULTIPLIER 的公式上下文落点；Level2、TPBus、SDK、
callback/message/network/authorization 边界均未改变。targeted evidence 为
`output/ida-tcalc-scalar-context-handlers-targeted-20260814.json`（30667 B，SHA-256
`8d3c5a49655fa9cdbf6ae94e4887812ae832938b17792645e65ad3f72dfa68e3`）。

formula focused、全部 formula-engine domains、link、真实 day800 CLI 与三条正式 POST 通过；未跑 full
CTest/API，139/139 仍归 TPBus 115 阶段。当前 PID `25268`，EXE SHA-256
`5978ba25e10b4fd71ce78837cb5004c746ba03df07cbd462fbbc891094b8f6e6`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `27868`
为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-bfdc14bc-pre-scalar-context-float-rollback-20260814.exe`，SHA-256
`bfdc14bc0e4c118c21030dc362c6ae61bc821e4a0e1f2e2f95d93f78c5335bd0`。

## 共享部署更新：numbered formula context（2026-08-14）

本批只修改 FINANCE/FINVALUE/DYNAINFO 的末柱 selector 与 f32 output 落点；Level2、TPBus、SDK、
callback/message/network/authorization 边界全部不变。targeted evidence 为
`output/ida-tcalc-numbered-context-handlers-targeted-20260814.json`（357331 B，SHA-256
`99c1c3e7cfac97d6fccb691693130c0fd81e9f1bc12991374189c05ac6df898e`），不声称重现宿主数据源。

formula focused、全部 formula-engine domains、link、真实 day5 CLI 与 3 条正式 POST 通过；未跑 full
CTest/API，139/139 仍归 TPBus 115 阶段。当前 PID `36312`，EXE SHA-256
`d56acfa73629e315c21eaddf66c7aee0ce18bb6f6b893af123ccac0bdd56a43f`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `25268`
为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-5978ba25-pre-numbered-context-float-rollback-20260814.exe`，SHA-256
`5978ba25e10b4fd71ce78837cb5004c746ba03df07cbd462fbbc891094b8f6e6`。

## 113/114/116 protobuf descriptor 复核与共享部署（2026-08-14）

对当前安装全部 EXE/DLL/OCX/PYD 重扫后，`MaintainData.HQPUSHPB` 和 `HQPUSHPB` 仍只命中
`C:\new_tdx\tpbus.dll`；该模块没有 `.proto`、protobuf descriptor 或 message 注册字符串。安装中
可见的 `protocol_mp.proto`、`QuantDB.proto` 等属于 PBRPC/量化数据库路径，没有主题 xref；既有
EventBus registry 快照也没有精确主题或 `MaintainData.*` 消费者。因此 113/114/116 继续只走
`tdx-level2-protobuf-wire-v1` 的字段号/wire type 检查，不命名业务字段，不新增 typed decoder。

本批共享二进制改动只涉及 GPJYVALUE/BKJYVALUE/SCJYVALUE 的末柱 selector 与 f32 输出；Level2
API、SDK、callback/message/network/authorization 边界不变。formula focused、link、真实 day20 CLI
与 3 条 POST 通过；未跑 full CTest/API，139/139 仍归 TPBus 115 阶段。

当前 PID `480`，EXE SHA-256
`1300851173dd12c56f969b421b60ed1dbe2b0e4919b14cc4ad7e58c49ad7a1fc`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `36312`
为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-d56acfa7-pre-professional-final-selector-rollback-20260814.exe`，SHA-256
`d56acfa73629e315c21eaddf66c7aee0ce18bb6f6b893af123ccac0bdd56a43f`。

## 共享部署更新：STRCMP final-handle broadcast（2026-08-14）

本批共享二进制改动仅涉及公式解释器 `STRCMP` 的末柱字符串句柄比较与广播；Level2、TPBus、SDK、
callback/message/network/authorization 边界均未改变。targeted evidence 为
`output/ida-tcalc-strcmp-targeted-20260814.json`（63500 B，SHA-256
`8e1d87490bbadb1f202e24ec8e00740669b050f138aadfad6dcc9e652a610b2f`）。

formula native-binary/language、link、真实 day5 CLI 与 3 条正式 POST 通过；未跑 full CTest/API，
139/139 仍归 TPBus 115 阶段。当前 PID `4560`，EXE SHA-256
`a005f2fb1e60580087a4f1217e352da296907fd6db100e0a5e20b071a0e120d8`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `480`
为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-13008511-pre-strcmp-final-handle-rollback-20260814.exe`，SHA-256
`1300851173dd12c56f969b421b60ed1dbe2b0e4919b14cc4ad7e58c49ad7a1fc`。

## 共享部署更新：SIGNALS_QS sparse context semantics（2026-08-14）

本批共享二进制只修改显式 caller-owned `SIGNALS_QS` 的末柱 selector、f32 输出和 mode 0/1/2 稀疏
填补；未新增券商取数、Level2/TPBus/SDK callback、消息、网络、凭据或授权路径。targeted evidence 为
`output/ida-tcalc-signals-qs-targeted-20260814.json`（36690 B，SHA-256
`6a5da89e4e86e94bf5ee509fae5d277c3afa475d68ca8e03009a4cf9a8f3e620`）。

formula context focused、link、真实 day5 CLI 与 3 条正式 POST 通过；未跑 full CTest/API，139/139
仍归 TPBus 115。当前 PID `34560`，EXE SHA-256
`b30373b159169616839e4c312afa70c5b271bec21ad0e3f7900af1f2c4af6760`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `4560`
为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-a005f2fb-pre-signals-qs-native-rollback-20260814.exe`，SHA-256
`a005f2fb1e60580087a4f1217e352da296907fd6db100e0a5e20b071a0e120d8`。

## 共享部署更新：L2_AMO final selector consumer（2026-08-14）

本批共享二进制只修改公式解释器对 caller-owned `L2_AMO` context 的末柱双 selector 与 f32 消费；
未新增 type-168 host callback、Level2/TPBus/SDK 数据获取、消息、网络、凭据或授权路径。targeted
evidence 为 `output/ida-tcalc-l2-amo-targeted-20260814.json`（464291 B，SHA-256
`e5f8699a5d65fe0b99e69338684d090e0e678d9f4416716b81ef53870d33286e`）。

formula context focused、link、真实 day5 K 线加明确合成的 L2 context fixture 与 3 条正式 POST 通过；
fixture 只验证 consumer semantics，不是真实 Level2 数据。未跑 full CTest/API，139/139 仍归 TPBus 115。
当前进程实例 PID `4560`，EXE SHA-256
`88c48ea38f0762616d1daf05b8a30637de56363b7447eb982f06a52040a3162e`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `34560`
为上一 formula 阶段；更早的 STRCMP PID `4560` 为历史 PID 复用。rollback 为
`output/tdx-tool-formal-b30373b1-pre-l2-amo-native-rollback-20260814.exe`，SHA-256
`b30373b159169616839e4c312afa70c5b271bec21ad0e3f7900af1f2c4af6760`。

## HQPUSHPB observer/descriptor 定点复核与共享部署（2026-08-14）

新增 `output/ida_probe_tpbus_hqpushpb_consumer_targeted_20260814.py`（SHA-256
`0edab3981706afb50f82bf82884f38e4bf7c3ef52aa12f6cf6761468efa1ce8f`）和
`output/ida-tpbus-hqpushpb-consumer-targeted-20260814.json`（228125 B，SHA-256
`ce226c13e55fbf6d84fc63912472ae43f6b260d6d7a0e46d8260988ffb4e25d0`）。证据包含 publisher
`0x1007BFBE`、topic expansion、observer registrations 与 EventBus vtable。完整主题字符串仅一份、
两个 xref 都在同一 publisher；当前安装 87 个模块没有精确/wildcard observer 或相关 protobuf
descriptor/parser。运行时构造/后加载模块仍是显式 frontier，不能据此发明静态 consumer 地址。

因此 TPBus 113/114/116 仍只提供 opaque protobuf wire inspection，不新增 typed business decoder。
本批共享二进制只修改公式 RGB 的 raw-f32/native channel 转换，Level2/TPBus/SDK/消息/网络/授权边界
不变。formula render focused、link、真实 day5 CLI 与 3 条 POST 通过；未跑 full CTest/API，139/139
仍归 TPBus 115。

当前 PID `33964`，EXE SHA-256
`7194ec950edc472d4f9fd018d0f71106b6dea51f3bc1e7a6bf58ccebb793af68`，web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee`；health 匹配。PID `4560`
为上一 formula 阶段；rollback 为
`output/tdx-tool-formal-88c48ea3-pre-rgb-native-rollback-20260814.exe`，SHA-256
`88c48ea38f0762616d1daf05b8a30637de56363b7447eb982f06a52040a3162e`。

## Level2Lab 有界预览与捕获生命周期（2026-08-14）

请求构造、合法捕获 decode、1807/18071 quote transition、1801/1802/1803/18031/1804 host
projection、4654/4651/4655 snapshot transition 五块结果默认停留在 summary，关闭详情时不会对完整
响应执行 `JSON.stringify`。第一次显式展开生成递归有界预览：遇到的每个数组最多展示前 100 项，
`preview_metadata` 同时记录截断数组的 JSON 路径、总项数、展示项数和省略项数。只有第二次显式选择
才显示完整 JSON；新请求与格式切换会把对应结果和详情模式复位，避免沿用旧响应。

四类已授权输入的生命周期按提交时快照处理：decode、quote、host 和 snapshot 请求只有成功返回且
当前输入仍与提交值相等时才自动清空；请求失败或等待期间用户已编辑时保留。每块仍有显式“清除捕获”
入口。quote 的上一宿主状态和证据 `context`、4651 的 `previous` 不随捕获清空，连续投影与 retained
replay 路径保持可用。本批只改 `Level2Lab.svelte`，没有新增或改变后端、API schema、SDK、消息、
网络或授权行为。

`npm run check` 为 0 errors/0 warnings；`npm run build` 成功，仅保留既有 >500 KiB chunk warning。
本批未运行 CTest 或 API；最近完整 CTest 仍为 TPBus 115 阶段的 139/139。

正式 PID `19348`，EXE SHA-256
`7194ec950edc472d4f9fd018d0f71106b6dea51f3bc1e7a6bf58ccebb793af68`，web index SHA-256
`378da4337ceb42c55444a2ccd4aa3c8f78735a9b8e79517fb962258c892ada00`；health 匹配，
`native_cpp=true`、`python_runtime=false`。PID `33964` 与 web SHA-256
`972558567aab8c7ff99b848a380fc7842d033537a08fa35fd34bd969b7b296ee` 是上一阶段历史身份。

## 共享前端 surface 更新：TQLEX/PBRPC/TPool（2026-08-14）

同一正式 web surface 本批只调整 TQLEX/PBRPC 完整 `ResultSets` 的 200 行客户端分页展示，以及
TPool 512 KiB 内联 XML 的成功条件清空、失败/竞态保留、文件控件全路径复位与显式清除。没有改变
Level2、TPBus、后端、schema、SDK、网络或授权边界。

`npm run check` 0 errors/0 warnings，`npm run build` 成功且仅有既有 chunk warning；
`tqlex/pbrpc/tpool` 路由均为 200 并命中新 asset。未跑 CTest/API，139/139 仍归 TPBus 115。
正式 PID `10812`，EXE SHA-256
`7194EC950EDC472D4F9FD018D0F71106B6DEA51F3BC1E7A6BF58CCEBB793AF68`，web SHA-256
`E9FA36DC240FE4B2EB4CA88872008324A0091F7A0DC4F1F1FE43B99B391CB75C`；health 匹配、native true、
python false。PID `19348` 与 web SHA-256
`378DA4337CEB42C55444A2CCD4AA3C8F78735A9B8E79517FB962258C892ADA00` 为历史身份。

## 共享 TQLEX/PBRPC 预算部署（2026-08-14）

本批只调整 TQLEX HTTP plan 与 PBRPC 组装预算；Level2/TPBus 的 domain、HTTP schema、SDK、网络、
消息和授权 surface 均未改变。未跑 full CTest，最近完整 139/139 仍归 TPBus 115。

正式 PID `36440`，EXE SHA-256
`C120DBFAAC9EF67E98B2B4697B9A42AA0DCD6B3CFB655ED65684C3783F89DD38`，web SHA-256
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`；health 匹配、native true、
python false。PID `10812` 为上一正式阶段，文件锁期间未监听的 PID `14592` 为非正式历史实例。

## 共享公式审计部署身份（2026-08-14）

本批只修复 HK 公式上下文聚合、缺绑定预分类及 coverage recon；Level2/TPBus 的 domain、CLI、HTTP/UI、
SDK、消息、网络和授权边界均未改变。未跑 full CTest，139/139 仍归 TPBus 115。

正式 PID `33136`，EXE SHA-256
`228F5261F52D9D19B2E33B08D1A5203C79DC42441FD899629BA7FACBAFB3ACD6`，web SHA-256
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`；health 匹配、native true、
python false。PID `21544` 为中间历史。rollback 为
`output/tdx-tool-formal-76eb2da6-pre-price-annotation-contract-rollback-20260814.exe`，SHA-256
`76EB2DA6232693C1679F37A01C15FCCC5FFA64E20CDEA33D740D15DF804F869D`。

## 共享市场 recon 部署身份（2026-08-14）

本批只修正滚动市场数据的 recon 不变量；Level2/TPBus domain、CLI、HTTP/UI、SDK、消息、网络和授权边界
均未改变。正式 full API 225/225；未跑 full CTest，139/139 仍归 TPBus 115。

正式 PID `30820`，EXE SHA-256
`1D189E40A11171A098C58D449E5288AEF1537C9E001AB6EF9FDEF87185DBEF9C`，web SHA-256
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`；health 匹配、native true、
python false。PID `33136` 为历史。rollback 为
`output/tdx-tool-formal-228f5261-pre-reconciliation-rollback-20260814.exe`，SHA-256
`228F5261F52D9D19B2E33B08D1A5203C79DC42441FD899629BA7FACBAFB3ACD6`。

## 共享公式宿主上下文部署身份（2026-08-14）

本批仅将公式 ZTPRICE/DTPRICE 的两个 type120/evaluator raw u16 输入显式化并修正 HK audit 缺键分类；
Level2/TPBus domain、CLI、HTTP/UI、SDK、消息、网络和授权边界均未改变。正式 full API 225/225，未跑 full
CTest，139/139 仍归 TPBus115。

正式 PID `31280`，EXE SHA-256
`1D6354054F73F47BDB4A7C17E5F089727EF941C9D7294F3A66E798B7CA02B0A6`，web SHA-256
`789592824F72D670FF65828A8924E1004618DBB0F4BAEA42DDD948E4579D767E`；health 匹配、native true、
python false。PID `30820` 为历史；rollback：
`output/tdx-tool-formal-1d189e40-pre-zd-exact-context-rollback-20260814.exe`，SHA-256
`1D189E40A11171A098C58D449E5288AEF1537C9E001AB6EF9FDEF87185DBEF9C`。

## 共享公式模板部署身份（2026-08-14）

本批只把 ZTPRICE/DTPRICE 的 caller-owned host raw 上下文模板从逐柱 series 改为两个 u16 scalar 占位；
Level2/TPBus domain、CLI、HTTP/UI、SDK、消息、网络与授权边界均未改变。stage full API 226/226、正式
focused 5/5；未跑 full CTest，139/139 仍归 TPBus115。

正式 PID `2536`，EXE SHA-256
`B600B503FC31250019023E1803972149A05F5FE55B737BF226295CF9070755A4`，web SHA-256
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、
python false。PID `31280` 为历史；rollback：
`output/tdx-tool-formal-1d635405-pre-context-template-scalar-rollback-20260814.exe`，SHA-256
`1D6354054F73F47BDB4A7C17E5F089727EF941C9D7294F3A66E798B7CA02B0A6`。

## 共享公式 scalar-only 快路径部署身份（2026-08-14）

本批只让纯标量公式 context-template 跳过无意义 K 线获取；Level2/TPBus domain、CLI、HTTP/UI、SDK、消息、
网络与授权边界不变。stage full API 226/226、正式 5/5；未跑 full CTest。

正式 PID `9092`，EXE SHA-256
`559C785798C06C9812D191C0304F28D5AB9C02670BE7D15C90783A8559F7E8C5`，web SHA-256
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；PID `2536` 为历史。rollback：
`output/tdx-tool-formal-b600b503-pre-context-template-skip-rollback-20260814.exe`，SHA-256
`B600B503FC31250019023E1803972149A05F5FE55B737BF226295CF9070755A4`。

## 共享 ATAN/COS/SIN 部署身份（2026-08-14）

本批只补公式解释器可表达的 trigonometric numeric raw-f32 路径；Level2/TPBus domain、CLI、HTTP/UI、SDK、
消息、网络与授权边界均未改变。只跑 native-scalars/language focused 和真实公式 smoke，未跑 full CTest/API；
139/139 仍归 TPBus115。

正式 PID `22988`，EXE SHA-256
`A04ECEEFA4155FDF5662434A64477A51D076873CB0624BB03A58E3943AECAE7C`，web SHA-256
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、
python false。PID `9092` 为历史。rollback：
`output/tdx-tool-formal-559c7857-pre-atan-cos-sin-rollback-20260814.exe`，SHA-256
`559C785798C06C9812D191C0304F28D5AB9C02670BE7D15C90783A8559F7E8C5`。

## 共享 140/140 阶段基线（2026-08-14）

本阶段未改变 Level2/TPBus domain、CLI、HTTP/UI、SDK、消息、网络或授权边界；工作树全目标 MinGW 串行
构建和当前完整 CTest **140/140**（0 failed、32.87 秒）通过，替代旧 TPBus115 的 139/139 基线。唯一
全构建 warning 位于公式测试字节夹具，已以等价定长 LE 写入消除并重跑相关目标。未跑 full API。

正式二进制与构建产物 hash 一致，无需重启：PID `22988`，EXE
`A04ECEEFA4155FDF5662434A64477A51D076873CB0624BB03A58E3943AECAE7C`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、
python false。Level2 的纯离线/无 SDK 调用/无消息投递/无网络边界保持不变。

## 共享 TPool root-relative 阶段基线（2026-08-14）

本批只收紧 TPool HTTP catalog/evaluate 的服务器路径投影；Level2/TPBus domain、CLI、HTTP/UI、SDK、消息、
网络和授权边界均未改变。最终全目标构建零告警、完整 CTest 140/140（0 failed、45.82 秒），未跑 full API。
正式 PID `12312`，EXE `37A639B5FD0501D7156DF3977928E55C94F643B1F431466F7A4EC99BFB972986`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、Python
false。PID `22988` 为历史；rollback SHA-256
`A04ECEEFA4155FDF5662434A64477A51D076873CB0624BB03A58E3943AECAE7C`。

## TPBus 4650 dispatcher 捕获预检（2026-08-14）

新增 `Level2Tpbus4650DispatchPreflightRequest` 与
`level2_tpbus_4650_dispatch_preflight_document`，并以
`level2 preflight --format tpbus-4650-dispatch-preflight --input BODY --encoding raw|hex` 暴露 CLI-only 入口。
domain 严格复现 `sub_10068065` 的六个 signed-i8 raw term 与 exact-size 表达式，保留 `raw+8` signed-i16
market、header 内 bounded NUL/ASCII code、`raw[5] || raw[86]` trigger 与 `raw[0]==1` data branch。
data candidate 只保留 offset/size/SHA-256，不输出 body；缺 NUL 时也不执行原生无界 `strcmp`。

该 schema 明确是 `offline-raw-shape-and-caller-gate-preflight`，不是 4650 host projection。host code/market、
target object、prior vector、host/server time、quote/subscription mutable state 继续列为 unresolved；
`dispatcher_fully_qualified=false`、`state_projection_closed=false`、handler/host write/SDK/callback/message/network
均为 false/0。因此此前 `sub_1007A75E` 宽宿主状态 do-not-implement 结论保持，只把已闭合的输入 validator
做成安全诊断。

独立 4650 focused 覆盖 minimum/exact±1、signed term、market/code、trigger/data branch、raw/hex CLI、384 KiB、
未知参数、无 body echo 与全零副作用；原 SDK compatibility preflight 亦重跑通过。完整串行构建成功，CTest
**141/141**、0 failed、11.36 秒；本批无 HTTP/UI/schema，未跑 full API。正式 PID `18344`，EXE
`365145AE97F6C78CFE8B6DECB08B53F9C75DEA1D222917C90325C0F060BBA172`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、Python
false。PID `12312` 为历史；rollback 为
`output/tdx-tool-formal-37a639b5-pre-tpbus4650-preflight-rollback-20260814.exe`，SHA-256
`37A639B5FD0501D7156DF3977928E55C94F643B1F431466F7A4EC99BFB972986`。

## 共享 JSN UTF-8 / 公式上下文部署身份（2026-08-14）

本批只修复 JSN discovery profile preview 的 UTF-8 边界并收口公式自动上下文分类；Level2/TPBus domain、
CLI、HTTP/UI、SDK、message、network 与 authorization 边界均未改变。JSN/formula focused、全 build、CTest
141/141（0 failed、46.38 秒）通过；schema 未变，未跑 full API，正式 discovery HTTP 已严格 UTF-8/JSON
验收。正式 PID `6852`，EXE `D1E041DD439256E8A86081DDD6682C42755E5C4554109A70E3FFF0CFB7288708`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、Python
false。PID `18344` 为历史；rollback SHA-256
`365145AE97F6C78CFE8B6DECB08B53F9C75DEA1D222917C90325C0F060BBA172`。

## 共享统一 UTF-8 / 数字时区部署身份（2026-08-14）

本批未改变 Level2/TPBus domain、CLI、HTTP/UI、SDK、消息、网络或授权边界；共享 native JSON 只统一了
code-point 安全前缀和 ASCII 数字时区。native/cloud/JSN/recon focused、全 build、CTest 141/141
（0 failed、59.13 秒）与正式 API focused 4/4 通过，未跑 full API。正式 PID `34252`，EXE
`C1B2B845AB375956CF48C6653E344070C0B2FECF899C2FEB531AA7DC91529A36`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、Python
false。PID `6852` 为历史；rollback SHA-256
`D1E041DD439256E8A86081DDD6682C42755E5C4554109A70E3FFF0CFB7288708`。Level2 全部离线/零副作用声明保持不变。

## Level2 / 用户面完成性审计（2026-08-14）

`level2.hpp` 当前 31 个非 command public function 全部由 build/decode/project/session/preflight 或公式 context 工作流消费；
18 个独立 Level2/server focused target 已包含在 141/141 CTest。现有静态证据不再留下“domain 已成熟但无入口”
的能力；宽 host-state、live time、SDK/authorization 与无 parser/consumer 的 opaque 分支仍明确为 preflight、
hash/size inspection 或 do-not-implement，不把猜测当作取数能力。

当前 Svelte check 0/0、web build 成功，正式 full API 226/226、0 failed；严格 UTF-8/JSON 报告 SHA-256
`068929D55743A628786011C6C6A74DEDB5C4034E094E71C7D8870DFA474D8F42`。本审计无生产代码变化；完整
build/CTest 保持 141/141、0 failed、59.13 秒。正式 PID `34252`，EXE
`C1B2B845AB375956CF48C6653E344070C0B2FECF899C2FEB531AA7DC91529A36`，web
`A8D0187856CDBA1C47BCE90272A2C4EFEB46796B55DE4F22A81033F22ED0D08D`；health 匹配、native true、Python
false。Level2 的离线、零 SDK/消息/网络/授权绕过边界不变。

## 下一阶段证据边界复核（2026-08-14）

本阶段的生产变化位于公式上下文/未来重放、AUTOFILTER 解释与 Web 拆包；现有 Level2 typed domain、CLI、
HTTP/UI、SDK/message/network/authorization 边界均未改变。

TPBus 113/114/116 再次对照
`output/ida-tpbus-hqpushpb-consumer-targeted-20260814.json`（228,125 B，SHA-256
`CE226C13E55FBF6D84FC63912472AE43F6B260D6D7A0E46D8260988FFB4E25D0`）：当前安装只有
`MaintainData.HQPUSHPB` publisher，没有精确/通配 observer，也没有绑定的 protobuf descriptor/parser。
因此只能证明 opaque payload 发布，不能命名业务字段。

4650 previous-state evidence
`output/ida-tpbus-sdk-4650-previous-state-targeted-20260813.json`（832,450 B，SHA-256
`996FFA85655B87AE5EC3F34545789AD477B2540194472D0A1F631DD240BC3B60`）仍显示完整输出依赖 `_time32`、
server offset、证券身份、多个 vectors 与宽 mutable object graph；现有 raw-shape preflight 不等于 state
projection。两项继续保持 evidence-blocked/do-not-implement，不新增猜测 decoder。

本阶段未跑 Level2 focused 或 CTest，因为 Level2 代码/schema 未变。
公式相关最终代表合约 4/4（报告 SHA-256
`35E135A9B8BEF79B7A2D424FBD28C69DB091C493A6B2D61DB77C3E294AFD34CD`），未触发任何 Level2 SDK/message/network
路径。工作树 `tdx-tool` 因公式/recon 改动重链，
SHA-256 `9055F1ED3EC2C1155B72ECA00FF4ACC74EF5B58BC0F65B388747330A03E01951`；web index
`E29140C04EEFA00067A40A1AA27754CE5D1B367912875A41D855FDF9781B3964`。这些是未部署工作树产物，
不代表 SDK 加载、消息投递、网络请求或授权绕过。

## HQPUSHPB consumer 复核与 TPBus 4650 价格原语（2026-08-14）

### 113/114/116 静态边界

新增 `output/ida_probe_ttplugin_hqpush_consumer_targeted_20260814.py` 与
`output/ida-ttplugin-hqpush-consumer-targeted-20260814.json`。安装版 `QHPlugins/TTPlugin.dll` 与分析副本完全
同哈希；报告从 `PushType/PushBody/PushTQL`、`CPushParse` getter/setter 和调用者反向收敛，未发现
`MaintainData.HQPUSHPB` 字符串，也没有同时属于 push-facing owner 且比较 113/114/116 的函数。安装目录全部
PE 的精确 topic 搜索仍只命中 `tpbus.dll`。这排除了此前新发现的 TTPlugin 线索，但不能否定运行时构造或稍后
加载模块；因此 113/114/116 继续只作为 opaque payload，不字段化。

### 4650 可分离纯子投影

`output/ida_probe_tpbus_4650_price_primitives_targeted_20260814.py` 生成 69,714 B 的紧凑 JSON，完整包含：

- `sub_10066A34`：104 条指令，120-byte quote snapshot 的 f32 价格投影；
- `sub_10066B6B`：46 条指令，整数价格投影；
- `__ftol2`：37 条指令；
- `sub_1007950B/sub_1007A75E` 的 target call 与 `push 78h` copy 切片。

production 新增独立 `level2_tpbus_4650_price_primitives.cpp`，公开 request 只含已授权 raw body 和 opaque
`target_market_or_mode_raw`。它先复用既有 exact signed-shape preflight，再要求 `raw[0]==1` 且 raw+96 至少
有 120 字节；snapshot 仅输出 SHA-256，不输出 body。普通证券在 mode=0 时使用 1..99 fraction；非零 mode
忽略该 fraction；SH 688/689 与 SZ 30 分支再加入 `auxiliary_f32/100`，整数路径按 native `__ftol2` 低 32 位
wrap。非默认 x87 precision-control 的极端边界不做超出证据的 bit-exact 宣称。

CLI：

```text
tdx-tool level2 preflight --format tpbus-4650-price-primitives \
  --input BODY --encoding raw|hex --target-market-or-mode-raw U32
```

新增 `tdx-level2-tpbus-4650-price-primitives-tests` 与既有
`tdx-level2-tpbus-4650-dispatch-preflight-tests` 均通过。只做 affected-target 构建与直接运行；未跑 full CTest、
full API、live SDK/network sample 或部署。验证报告为
`output/level2-tpbus-4650-price-primitives-verification-20260814.json`，SHA-256
`BB78797BEB4C46EAD348AFCDF7F9403B44899695DA06BCF920F44749B860C3D5`。完整 4650 handler 的 live time、
identity、subscription 与宽 host graph 边界保持未实现。
