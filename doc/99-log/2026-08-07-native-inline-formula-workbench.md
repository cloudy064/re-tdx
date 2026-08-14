# 自定义公式源码纯 C++ 工作台

## 目标

补齐公式解释器最后一个明显的产品入口缺口：CLI 已能通过 `--source-file` 执行
用户公式，但 HTTP 和网页只能选择 TCalc 内置公式。本轮把任意合法通达信公式
源码贯通到本地 API 与 Svelte，不引入 Python，不涉及 L2。

## 原生接口

`POST /api/v1/formulas/evaluate` 接受 JSON body 中的证券、周期、源码、参数、
严格财务时点和未来函数只读开关。服务仍调用
`evaluate_formula_source_document`，返回 `tdx-source-interpreter-v1` 的数值点和
`tdx-formula-render-ir-v1`，不存在浏览器侧公式替代实现。

安全边界：

- 必须携带 `X-TDX-Action: formula-evaluate`；
- HTTP body 最多 64 KiB，公式源码最多 16 KiB；
- 参数最多 64 个，名称和值均有界；
- 请求正文与公式源码不写盘，响应标记 `request_body_retained=false`；
- 未来函数只允许显式只读绘图，继续禁止扫描和回测；
- 未确认 POST 返回 405，跨源预检不放行确认头。

服务端请求读取也从“只读到 HTTP 头”修正为严格按 `Content-Length` 有界读取，
解决浏览器请求体可能跨 TCP 分片时的完整性问题。

## 网页

公式库的计算面板新增“内置公式/自定义源码”切换。自定义模式提供源码编辑器、
参数覆盖、证券/周期/历史页数、严格财务时点和未来函数只读开关；Ctrl+Enter 可
直接执行。结果继续进入现有公式图表，并展示源码模式和本次源码字节数。

默认示例为三输出 MACD：`FAST/SIGNAL/HIST`。选择内置公式会自动回到内置模式，
专家回测和条件扫描按钮不会在自定义模式下误开放。

## 验证

真实部署验证以平安银行日线运行自定义 MACD：

- 返回 120 根 K 线；
- 输出严格为 `FAST/SIGNAL/HIST`；
- `formula_source_mode=inline-post`；
- 语法错误 `X:EMA(CLOSE,);` 返回 HTTP 400，并定位
  `expected formula expression at byte 12`；
- 缺少确认头返回 HTTP 405。

回归结果：

- C++：65/65；
- Svelte：0 错误、0 警告，生产构建成功；
- 新增 OpenAPI POST operation；
- 选定接口契约 2/2；
- 完整部署态契约 93/93。

证据：

- `output/probes/api-contracts-formula-inline-selected.json`；
- `output/probes/api-contracts-after-inline-formula-current.json`。

部署服务为 `http://127.0.0.1:8765`，PID `25036`。构建与发行包可执行文件
SHA-256 均为
`2BB6E06A4A9558FBB36EE477274D61CE1444521AFEA408C8BFCE856FA2235612`。

