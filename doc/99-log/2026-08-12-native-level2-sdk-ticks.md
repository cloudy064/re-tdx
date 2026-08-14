# Level2 SDK 请求与逐笔回调纯 C++ 闭环

日期：2026-08-12

## 本轮收益

先前静态证据已经恢复 SDK 请求与 `1801/1802` 固定记录，但统一工具只支持
`1803/18031` 回调解码。本轮补齐两侧：

- `level2 build --format sdk-4653`：40 字节分时请求，含附加行情和回购时间标志；
- `level2 build --format sdk-4655`：46 字节逐笔成交请求，保留客户端
  `want-number > 500` 回退为 80 的规则；
- `level2 build --format sdk-4680`：37 字节五档/十档请求；
- `level2 decode --format sdk-1801`：52 字节逐笔成交记录；
- `level2 decode --format sdk-1802`：40 字节逐笔委托、买撤、卖撤记录。
- `level2 decode --format sdk-1804`：432 字节双价格、两侧各最多 50 个数量槽。
- `level2 decode --format sdk-1807|sdk-18071`：380 字节行情更新、OHLC、量额、
  双边十档与买卖均价/总量。
- `level2 decode --format sdk-json-4655`：把调用方合法取得的 tpbus UTF-8 JSON
  归一为逐笔成交序列，复现末项到首项的原生遍历、时间、价格、数量和 B/S 状态。
- `level2 decode --format sdk-json-4671`：从 `buyList[0]` 与 `sellList[last]`
  提取委托数量，按原生 242 字节体的 101 项总容量和除 100 手数口径输出。
- `level2 decode --format sdk-json-4680`：校验买卖两侧成对价量数组，按请求的
  5/10 档归一方向、行情摘要与数量乘 1000 的原生投影。
- `level2 build --format sdk-fnreqdata-plan`：覆盖主程序调用的
  `1801/1802/1803/1804/18071`，恢复 12 个 32 位 ABI 槽及 25 字节本地 callback
  correlation 布局；这是逻辑调用计划，不是 wire 请求体。

工具只处理离线请求体和调用方合法捕获的授权回调，不创建登录、连接或订阅，也不
读取或推导账号权限。三类 SDK JSON 入口只处理内存文档，不读取服务器路径；未被
消费的输入字段不会回显或持久化。

## 精确字段

`1801`：`u64 time_raw + f64 price + u64 volume_raw`，以及偏移 36/40 的两个
辅助 `u32` 和偏移 48 的方向：1 买、2 卖，其余保持中性/未知。

`1802`：`u64 time_raw + f64 price + u64 volume_raw + u32 order_id`，偏移 36
类型为 1 买、2 卖、3 买撤、4 卖撤。未得到业务证据的保留区不命名。

`1804`：`+8/+216` 两个 `f64` 价格，`+16/+224` 两组 50 个 `f32` 数量槽，
`+424/+428` 两个 `u32` 计数。主程序把它们投影为两个最多 50 项的队列，但该
消费层没有可靠买卖枚举，所以 schema 使用 `first/second`，不猜方向。

`1807/18071` 共用 380 字节源体：`+0` 为毫秒时间，`+8/+16/+24/+32`
为现价/开/高/低，`+40/+48` 为成交额/累计量，`+76` 为昨收；`+108/+188`
是卖十档价格/数量，`+228/+308` 是买十档价格/数量，尾部
`+348/+356/+364/+372` 依次投影为买均/总买/卖均/总卖。证券身份由 SDK
请求/回调注册表关联，并不在 380 字节体内。`+68/+72` 仍按原始值输出：当前
消费函数只搬运或累加，没有给出可引用的业务名称。

## 验证

- 当前 `tdx-level2-tests` 聚焦测试通过，覆盖三类 SDK 请求、`want-number` 回退、
  两种逐笔记录、1804 数量队列、1807/18071 行情更新、三类 SDK JSON 归一化、
  截断限制以及买卖/撤单枚举；
- 当前 `tdx-server-level2-tests` 聚焦测试通过，覆盖三类 SDK JSON 的 HTTP body
  白名单、大小/深度/数值拒绝边界，以及输入哨兵不回显；
- CLI 合约验证 `sdk-1801` 为 52 字节、价格 10.25、买方向；`sdk-1802` 为
  40 字节、价格 10.26、买撤、委托号 70001；
- `sdk-1804` 合约验证 432 字节、价格 10.23/10.24、计数 2+3 和有界截断；
- `sdk-1807` 合约验证 380 字节、OHLC、买卖一、买卖均价与总量；
- SDK `4655` 输出 46 字节且 501 回退 80；`4680` 输出 37 字节且深度为 5；
- 原有 `1364` 26 字节请求 hex 保持不变；
- 最初完成固定 SDK 请求/回调时，正式服务 PID 24096 未重启、未替换；该 PID 只
  是历史记录；后续 42900 也只是中间阶段。
- fnReqData 聚焦测试与两项 CLI smoke 通过：五种 data type 均生成 12 槽计划，
  25 字节 correlation 因 opaque key 未就绪，`ready/invoked/request_sent=false`，
  `abi_stack_bytes_built/wire_bytes_built/network_request_bytes_built=false`。
- 当前包含 SDK JSON 与 fnReqData plan 的产物已 build/install/restart 到正式 PID
  `11468`。EXE SHA-256 为
  `3881917c02000fad1dcc8d6e9d956e92022f910932a7c1aad2f0fc3176c0fb54`，网页
  `index.html` SHA-256 为
  `f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`；运行时
  focused 已确认哈希一致及上述未调用/未发送状态。本批未运行完整 CTest 或 full API。

机器可读证据：

- `output/verify-level2-sdk-1801-20260812.json`
- `output/verify-level2-sdk-1802-20260812.json`
- `output/verify-level2-sdk-1804-20260812.json`
- `output/verify-level2-sdk-1807-20260812.json`
- `output/verify-level2-sdk-4655-request-20260812.json`
- `output/verify-level2-sdk-4680-request-20260812.json`
- `output/verify-level2-request-regression-20260812.json`
- `output/ida-tdxw-level2-remaining-sdk-20260812.log`

## 剩余边界

`tpbus 113/114/116` 仍只有解帧、protobuf wire 和发布主题证据，现有已处理 DLL
没有注册消费回调；在没有授权真实样本或消费方 schema 前，不为字段编造成交、委托
或盘口名称。`1804/1807/18071` 已恢复；其中 1807/18071 的 `+68/+72`
仍保留原始字段名，只有取得授权样本或发现带名称的 SDK 结构定义后才继续标定。
