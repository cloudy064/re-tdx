# 公式连续标记、条件色线与动态带状填充

## 结论

本轮没有把 `pixel_renderer_equivalent=false` 改成“已等价”，而是从 379 条公式的
56 条展示 surrogate 中找出三处会导致实际误绘的语义并补齐：

- `DRAWNUMBER_DIF(COND,STYLE,START,NUM)` 不再当作单点 `DRAWNUMBER`。C++ IR
  从命中 bar 开始连续生成 `NUM` 个事件，值逐根加 1，`11..36` 映射为 `A..Z`；
  `STYLE=0/1/2` 分别保留普通、引线、引线加框，`DRAWABOVE` 决定锚定 K 线高点，
  否则锚定低点，最多展开 250 项。
- `PARTLINE(VAL,COLOR,DIRECT)` 不再忽略第三参数。IR 明确记录
  `DIRECT=0` 的“当前到下一根”和 `DIRECT=1` 的“上一根到当前”；网页按
  TradingView Lightweight Charts 的起点着色规则移动颜色，`DRAWNULL` 颜色使用
  透明线段，不再回退为默认色。
- `DRAWBAND(VAL1,COLOR1,VAL2,COLOR2)` 的 IR 逐根选择实际填充色：
  `VAL1>VAL2` 取 `COLOR1`，否则取 `COLOR2`。Svelte 在两条动态边界之间生成
  随缩放、平移和尺寸变化重算的 SVG 四边形，边界交叉时在交点拆成两块。

全部执行和 IR 生成仍是纯 C++；浏览器只消费已求值结构，不运行 Python。

## 证据

`TCalc.dll` 内置帮助字符串位于 `0x100F7EB7`，恢复出的签名是
`DRAWNUMBER_DIF(COND,STYLE,START,NUM)`。官方函数表进一步确认连续递增、
`11..36 -> A..Z`、三种 STYLE 和 250 个字符上限。IDA 字符串与伪代码报告为：

- `output/probes/ida-tcalc-drawnumber-dif-xrefs.json`
- `output/probes/ida-tcalc-drawnumber-dif.log`

真实平安银行日线验收：

| 公式 | 240 根日线结果 | 本轮确认的语义 |
|---|---:|---|
| `SQJZ` | 8 图元、54 事件 | 四组 `DRAWNUMBER_DIF` 分别展开 32/4/16/2 个事件，锚定高/低点 |
| `WAVEKX` | 2 图元、41 事件 | 9 个 `PARTLINE` 事件使用 `previous-to-current` |
| `RGBAND` | 1 图元、240 事件 | 每根保留上下关系、选中色参数及 Windows `COLORREF` |

800 根 `SQJZ` 额外验收得到 243 个连续标记事件；旧实现只会在每个触发 bar
产生一个事件，并把第二参数 `STYLE=1` 误当成价格 1 元。

## 协议增量

保持 `tdx-formula-render-ir-v1` 向后兼容，新增字段均为可选增量：

- 连续标记：`source_index/sequence_offset/sequence_style/sequence_value/`
  `sequence_label/anchor`；
- 条件色线：`segment_direction/segment_from_index/segment_to_index/`
  `segment_color_available`；
- 带状填充：`band_side/fill_color_argument/fill_color_available/fill_color_ref`。

这仍不是通达信字体、图标资源、像素宽度和覆盖顺序的完整复刻，
`pixel_renderer_equivalent=false` 保持不变。扫描和回测仍只消费数值输出，
展示事件不会变成交易信号；L2 与券商私有 `SIGNALS_QS` 也没有用假数据补齐。

## 验证

- 原生 CTest：101/101；
- `svelte-check`：0 错误、0 警告；
- Svelte 生产构建成功；
- 新增真实公式契约临时/正式均为 3/3；
- 临时/正式服务全量 API 契约均为 182/182。

契约证据：

- `output/probes/api-contract-formula-render-semantics-temp-20260809.json`
- `output/probes/api-contract-full-temp-20260809-formula-render-semantics.json`
- `output/probes/api-contract-formula-render-semantics-formal-20260809.json`
- `output/probes/api-contract-full-formal-20260809-formula-render-semantics.json`

固定契约分别为 `formula-sqjz-sequence-live`、
`formula-wavekx-partline-live`、`formula-rgband-fill-live`；其中 SQJZ 契约会拒绝
旧的单点 `number` surrogate，防止后续回归。

正式服务已安装到 `dist/tdx-tool`，PID `13592`，仅监听 `127.0.0.1:8765`；
网页入口加载 `assets/index-5gq2UhNR.js`。EXE SHA-256：
`400561D6A6778D5397CC3BDF7AD441D2CD355ECE56F5DCDDAFE6EC620FAAFF71`。
