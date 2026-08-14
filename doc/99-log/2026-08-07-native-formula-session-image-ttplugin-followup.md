# 公式外部上下文、会话定时器、盘口字段与 TTPlugin 翻译契约

## 目标

继续收口不依赖 L2 主动联网的高收益缺口：让公式解释器能够严格接收调用方
自有外部序列，恢复 tpbus/TaApi 会话默认时序，命名 TDataParse 剩余高价值
盘口字段，并澄清 TTPlugin `RedirectData` 的真实职责。所有新增运行能力保持
纯 C++，不加载原 DLL，不读取或输出私密会话值，不绕过授权。

## 公式显式上下文

- `formulas analyze` 升级为 `analysis_schema_version=6`；
- 379 条公式中 20 条被标记为 `explicit_context_bindable`：14 条依赖调用方
  合法取得的 L2 序列，6 条依赖券商私有 `SIGNALS_QS`；
- `L2_AMO#i#j`、七类 L2 统计键和 `SIGNALS_QS#id#mode` 共形成 36 个精确键；
- 新增 `formulas context-template`，按选中公式和 `DATE|TIME` 生成 null 模板；
- `evaluate/audit` 只在全部声明键均有真实数值时执行，不缺省为零；模板本身
  不下载、推导、伪造或取得任何受限数据；
- 固定 API 增加 `formula-explicit-context-post`，当前 full 契约共 130 项。

生成证据：

- `output/formula-analysis-explicit-authorized-v6.json`
- `output/formula-explicit-context-template-all.json`

## tpbus/TaApi 会话静态时序

`TaApi!CTAPeer::OnHeartBeatTimer` 与 tpbus 内嵌 TAEngine XML 共同确认：

- tpbus 默认 `JobTimeout=150000 ms`；创建/事务超时均为 3000 ms；
- 心跳 `TimeSpan=30, OnIdle=YES, JustNoQueue=NO`；
- 空闲达到 30 秒建立心跳任务，约 `2*TimeSpan+1` 秒无活动后发事件并关闭/
  失败 peer；引擎定时器同时清理过期任务和 peer；
- 已确认重连/超时选项包括 `DisConnect/LazyTimeOut/MaxReConTimes/JobTimeOut/`
  `MaxTimeOutTimes/LoginStateNoChangeHost/ReConnectTimeOut/EnableTimeoutProc`。

新增纯 C++ `recon session-config`：解析 `connect.cfg` 公开服务器组和上述静态
默认值；`user.ini` 中会话/令牌类字段只输出存在性、编码长度和加密封套标志，
不输出值或哈希。当前只读审计发现 6 个服务器组、69 个有效端点，其中 43 个
为行情端点；该统计不是登录成功声明。

证据：

- `output/ida-taapi-session-chain.json`
- `output/ida-tpbus-session-chain.json`
- `output/tdx-session-config-audit.json`

## TDataParse `1G..1J`

`TdxW.exe:0xA26C90` 将 1032 字节记录 `+424/+432/+440/+448` 映射到分钟 UI
结构；`0xA207C0` 的 GBK 标签分别为“买均/卖均/总买/总卖”。结合精确偏移，
字段闭合为：

- `1G`：买均，平均买价；
- `1H`：总买，买方总量；
- `1I`：卖均，平均卖价；
- `1J`：总卖，卖方总量。

纯 C++ JSON 升级为 `tdx-image-data-snapshots-v2`，四项放入
`aggregate_order_book`；只有 `1C..1F` 继续保留中性标签。原 1032 字节编码
和原 DLL 逐字节差分基线不变。

证据：

- `output/ida-tdxw-image-aux-consumers.json`
- `output/ida-tdxw-image-aux-labels.json`

## TTPlugin `RedirectData`

新的精确反编译纠正了“RedirectData 是地址 JSON”的旧判断：

1. `HQDataService.SetOption("RedirectData", req, json)` 解析结构化请求；
2. `sub_102979D0` 只支持 4611/4612/4618/4630/4631/4632，编码 F10、资讯标题、
   文件块和 XML 块的固定旧协议体；
3. `CTAJob_Redirect` 承载 `Target/ReqNo/Body`；
4. `sub_102983A0` 将响应翻译为对象，再以
   `CTAJob_InetTQL(Name=Local:HQDataService)` 返回。

新增纯 C++ `recon ttplugin-redirect`，输出六类请求的字段偏移、定长、响应对象
字段和完整任务流；传入 `--request` 时可严格编码 GBK/NUL 定长请求体。命令
明确拒绝未由该翻译器支持的 9221，不加载 DLL、不联网，也不读取登录票据。

证据：

- `output/ida-ttplugin-redirect-chain.json`
- `output/ida-ttplugin-redirect-targets.json`
- `output/ida-ttplugin-redirect-strings.json`
- `output/tdx-ttplugin-redirect-contract.json`

## 验收与发行

- 完整 C++ 测试：94/94 通过；
- 正式 HTTP 契约：130/130 通过，131 次网络请求、0 失败；
- 正式功能目录：140 项；
- 发行文件 SHA-256：
  `97A4E93DEACD0836F1354CD94BBD7EABB2EFC8A7D3FF0F7ABA3EE4DF5A884E2D`；
- 正式服务已从该发行文件重新启动并监听 `127.0.0.1:8765`；
- 契约报告：`output/api-contracts-formula-session-image-ttplugin-full.json`。

## 剩余边界

- 后续全量消费者审计已确认当前 TdxW 六个入口均不读取 `1C..1F`；它们已
  定性为当前构建保留字段，详见
  [后续记录](2026-08-07-native-ttplugin-servers-image-consumer-audit.md)。同一审计
  还确认返回后的 `0D` 文本区未被当前六个入口读取，但它在解析器内部仍参与
  状态继承和相邻记录去重；
- 后续已确认 TTPlugin `Target` 是调用方提供的 `uint8` 路由号，并闭合四个
  服务器族和六类 URL 语法。仍需合法运行时样本标定 9221 剩余状态字段；
- tpbus/TaApi 主动登录和 L2 订阅没有实现。本轮静态时序与私密字段存在性审计
  不构成登录、授权或绕过能力。
