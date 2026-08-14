# STICK、LINESTICK 与点线样式原生语义闭合

## 结论

上一轮闭合 `COLORSTICK/VOLSTICK` 后，普通 series 仍有两类网页近似：
`LINESTICK` 被画成 72% 柱宽的 histogram，`DOTLINE` 只套用了浏览器通用虚线；
普通线还会跨过 `DRAWNULL` 连接。TCalc 样式表和 TdxW 分派表现已把绘图类型
4—9 全部闭合：

- `STICK`（type 4，`sub_957D70`）逐有限值从零轴画竖向 stem；
- `LINESTICK`（type 5，`sub_957F70`）先画同样的零轴 stem，再只连接连续有限值；
- `CIRCLEDOT`（type 6，`sub_95ABA0`）按柱间距选择四个方向像素或半径 3 空心圆；
- `CROSSDOT`（type 7，`sub_95AE00`）按柱间距选择半径 1、2、3 的对角叉；
- `POINTDOT`（type 8，`sub_95B0A0`）在线宽小于 2 时画单像素，否则画线宽直径的
  实心椭圆；
- `DOTLINE`（type 9）与普通线共用 `sub_957620`，但创建 `PS_DOT` 画笔；缺失值
  中断连续 run，单点 run 画 `x-3` 到 `x` 的短横线；
- 普通线同样按缺失值断段并保留单点 run。`Other/BoldZBLine` 在默认线宽为 1
  时把有效宽度改为 2；当前安装值为 0。

## 原生证据

本轮 Python 仍只用于离线驱动 IDA 导出证据，正式工具和服务运行时保持纯 C++：

- `output/probes/ida-tcalc-style-tables.json`：样式名到 type 4—9 的映射；
- `output/probes/ida-tdxw-formula-dispatch-current.json`：TdxW 的 renderer 分派；
- `output/probes/ida-tdxw-line-stick-renderers.json`：STICK/LINESTICK 的零轴 stem、
  连续 run 和绘制顺序；
- `output/probes/ida-tdxw-series-line-dotline-renderer.json`：普通线/DOTLINE 的
  `PS_SOLID/PS_DOT`、缺失断段及单点短线；
- `output/probes/ida-tdxw-series-dot-renderers.json`：三种点样式随 bar spacing、
  LINETHICK 变化的像素几何；
- `output/probes/ida-tdxw-series-line-config-xrefs.json`：
  `Other/BoldZBLine` 的读取点、默认值及当前配置。

## 实现

C++ IR 对每条非 COLOR/VOL series 发布 `series_native_mode`、原生 type、renderer、
画笔、线宽来源和 bar-price 坐标契约；对应样式另发布断段、单点、零轴 stem 或
点几何规则。`tdx-formula-render-environment-v1` 新增 `series_lines`，只读解析
`T0002/user.ini` 的 `BoldZBLine`。

Svelte 继续用透明 Lightweight Charts series 提供时间轴、价格轴、autoscale 与
缩放坐标，实际图形由有序 SVG 覆盖层绘制。普通线输入会为缺失 bar 写入
WhitespaceData；SVG 按连续 run 生成线段并处理孤立单点。STICK/LINESTICK 使用
零轴细 stem，不再生成宽矩形。三种点样式每次缩放/平移后重新读取当前 bar
spacing，因此会在原生阈值处切换几何。

新增固定契约 `formula-native-series-styles-inline-post`，同时锁定 type
0/4/5/6/7/8/9、renderer、断段/单点、stem、点几何和 `BoldZBLine` 环境；把
LINESTICK 改回 `wide-histogram` 的反向 fixture 必须失败。原有 SLZT 契约也加强为
必须命中 type 5、`sub_957F70` 和断段规则。

## 验证与部署

- 原生 CTest：101/101；
- Svelte 检查：0 错误、0 警告，生产构建成功；
- 临时及正式服务新增契约均通过，quick 为 8/8；
- 正式 full：201/201，202 次网络请求，报告为
  `output/probes/api-contracts-full-current.json`；首次 full 的唯一失败是
  `jsn-discovery-live` 的 WinHTTP 12002 瞬时超时，单项复核 607ms 通过，随后
  60 秒超时预算的完整重跑 201/201；
- 正式 EXE SHA-256：
  `A77ECED1757D6FFE3F717D55DC46CDB38F09EC147B1C8630CBADC05F702B06BE`；
- 网页资源：`assets/index-By9M7rtx.js`、`assets/index-BBbdHCcI.css`；
- 正式服务 PID 44916，仅监听 `127.0.0.1:8765`；健康检查为 379 条公式、100 个
  图标单元、`native_cpp=true`、`python_runtime=false`。

SVG 的抗锯齿、亚像素取整以及浏览器对 Win32 `PS_DOT` 的栅格化仍不声明与 GDI
逐像素相同；renderer 选择、缺失值拓扑、单点规则、缩放阈值与几何分支已经不再
由网页猜测。
