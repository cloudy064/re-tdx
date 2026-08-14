# TBigData 计算列解释器 API 与网页工作台

## 目标

把已经恢复的纯 C++ `formulas cloud-calc` 从 CLI 能力提升为固定服务能力，
让网页可以直接审计 `cloud_cfg` 并复算一条榜单输入行，同时保持通达信目录只读、
不引入 Python、不开放任意服务器文件读取。

## 实现

- `cloud_calc` 新增请求级审计和执行入口，HTTP 与 CLI 复用同一 CFG 解析、
  unit 依赖图、36 项内建函数、宿主字段解析和计算核心；
- CFG 请求名只允许 1—128 个字母、数字、点、下划线和连字符，并拒绝
  `.`、`..`，无法通过接口传入绝对或相对路径；
- 执行请求接受扁平 `row`、评价日、公开行情开关、三类调用方离线快照与最多
  256 个标量覆盖；公开请求与对应离线快照不能混用；
- 新增固定 `GET/POST /api/v1/formulas/cloud-calc`。POST 必须携带
  `X-TDX-Action: formula-cloud-calc`，输入行不写盘且不在响应中回显；
- Svelte 公式库新增“榜单计算列解释器”，展示配置审计、宿主绑定、未解析列和
  按 unit 展平的计算结果；默认样本是南航转债与关联正股；
- 功能注册表不再把 `formulas cloud-calc` 标为 CLI-only；OpenAPI 同时声明 GET
  与 POST。

## 真实验证

在临时 `127.0.0.1:8766` 服务上以 `func_kzz_kzzsy101`、证券 `sh:110075`
和关联正股 `sh:600029` 执行：

- 15/15 个计算列全部完成，0 个不可用、0 个错误；
- 自动建立 9 个宿主绑定，包含公开 L1 最新价/涨跌幅/成交额、本地证券名称与
  通达信行业、债券应计利息和同行当前票息；
- `host_context.unresolved` 为 0，快照来源为 `public-l1-0x054c`；
- 响应 `row_source=inline-request`，不存在原始 `row` 字段；
- 单配置审计为 15/15 可执行、36/36 注册内建已实现、8/8 有效宿主列可解析。

固定契约新增：

- `formula-cloud-calc-audit`：验证 660 个 CFG、335 个含计算列 CFG、1370/1370
  可执行、36/36 内建、2570/2570 宿主列、0 解析错误/依赖环/未解析；
- `formula-cloud-calc-live-post`：验证上述真实可转债行 15/15、公开行情来源、
  9 个宿主绑定、0 未解析，并检查不回显请求行。

临时选定契约 2/2、临时全量 165/165；正式服务选定契约 2/2、全量
165/165。证据文件：

- `output/probes/api-contracts-cloud-calc-temp-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-cloud-calc.json`
- `output/probes/api-contracts-cloud-calc-formal-20260808.json`
- `output/probes/api-contracts-full-formal-20260808-cloud-calc.json`

原生 CTest 为 98/98；`svelte-check` 为 0 错误、0 警告；Vite 生产构建成功。

## 部署

正式发行包已重新安装并运行在 `http://127.0.0.1:8765`：

- PID：`26576`
- EXE SHA-256：`3B3A52EB3515F058D21D7359024DE217D1B4AB4A34EE54E60B78720BB1D0CC69`
- 当前页面资产：`index-CbMf8UB-.js`、`index-CeGlLkfZ.css`
- 主页 HTTP 200；健康页识别 51,935 证券、1,159 板块、379 公式、580 个
  JSN 资源和 47,669 个证券键。

## 边界与后续

- 这是 TBigData 表格 `calc/calcref` 解释器，不是 TCalc 时序 K 线公式解释器；
- 默认离线，只有调用方明确设置 `quotes=true` 才请求公开 L1/财务；不登录、
  不使用 L2、不会伪造授权字段；
- 29 个当前安装 CFG 尚未实际调用的内建分支仍只有处理体级固定向量验证；出现
  新 CFG 或原版 UI 真实行时继续做差分即可，无需再建立另一套解释器；
- 后续高收益方向是把用户实际导出的榜单行做成可保存于浏览器本地的模板，并对
  多行批量复算设计有界接口；在确认负载与隐私边界前不直接扩大当前单行 POST。
