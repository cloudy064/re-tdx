# TBigData 榜单批量复算、共享行情与浏览器模板

## 目标

把已上线的单行 TBigData `calc/calcref` 解释器推进到真实榜单批处理，并补齐
统一 CLI、固定 HTTP、Svelte 页面和浏览器本地模板；继续保持纯 C++、只读、
有界请求和不回显输入。

## 纯 C++ 批量内核

新增 `evaluate_cloud_calc_batch_request`：

- 接受 1—128 个内联扁平对象；单行最多 4096 个字段，全批最多 65,536 个；
- 先逐行预检 CFG 的主证券、`refzqdm/refunit` 关联证券、财务、涨速、深度、
  行业和封单需求；
- 对全批证券去重，根据最强实际需求统一选择 `0x054C/0x0547/0x053E`，只建立
  一份共享行情文档；财务和特殊涨跌停文档同样跨行共享；
- 计算阶段仍按行、按 unit 隔离。结果以零基 `row_index` 关联，某行失败不会
  打乱其他行；响应不包含原始 `row/rows` 或请求体；
- API 单行响应同时移除了 CFG 绝对路径，只保留安全的 `cfg_name`。

固定接口为 `POST /api/v1/formulas/cloud-calc/batch`，必须携带
`X-TDX-Action: formula-cloud-calc-batch`。OpenAPI 只为该路径声明 POST，不误报
GET。

## 统一 CLI

`formulas cloud-calc` 新增：

- `--all-rows`：从对象、对象数组或 `colheader/data` JSN 读取批次；
- `--max-rows 1..128`：显式限制批量大小；
- 报告保留 `source_rows_available/source_rows_truncated`，便于确认输入是否截断；
- `--all-rows` 与 `--row-index` 互斥，网络仍只在显式 `--quotes` 时启用。

真实命令读取 `gxjty_zq_kzzsy101_1.jsn` 的 309 行转债榜，取前两行得到：

- 2/2 行成功，30/30 个计算列完成，0 不可用、0 错误；
- 两行涉及 4 个不同证券，但 `quote_document_fetches=1`；
- 每行各 9 个宿主绑定；
- 证据：`output/tdx-tbigdata-cloud-calc-batch-cli.json`。

## Svelte 工作台

网页“榜单计算列解释器”现在：

- 自动区分单个 JSON 对象与 1—128 行对象数组，并选择单行或批量 API；
- 结果表增加输入行号，批量摘要展示成功/失败行、共享行情模式、去重证券数和
  行情/财务文档请求数；
- 可一键把单行样本变成两行示例；
- 用户显式点击后可保存最多 20 个模板到当前浏览器 `localStorage`，模板包含
  CFG、评价日、行情开关和输入 JSON，不会自动写盘；
- 可把当前单行或批量响应下载为 JSON。

生产构建已确认包含 `/api/v1/formulas/cloud-calc/batch`、模板存储键、批量示例
和导出入口。

## 验证

真实 HTTP 两行南航转债样本：

- 2/2 行成功、30/30 列完成；
- 相同转债/正股跨行去重为 2 个证券，只执行 1 次共享 `0x054C` 文档获取；
- 每行各 9 个宿主绑定、0 未解析；
- `request_body_retained=false`，不存在请求体、原始行或 CFG 绝对路径。

新增 `formula-cloud-calc-batch-post` 固定契约，验证批量计数、共享去重计划、
逐行索引和输入清理。结果：

- 临时选定契约：3/3；临时全量：166/166；
- 正式选定契约：3/3；正式全量：166/166；
- 原生 CTest：98/98；
- `svelte-check`：0 错误、0 警告；Vite 生产构建成功。

证据文件：

- `output/probes/api-contracts-cloud-calc-batch-temp-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-cloud-calc-batch.json`
- `output/probes/api-contracts-cloud-calc-batch-formal-20260808.json`
- `output/probes/api-contracts-full-formal-20260808-cloud-calc-batch.json`

## 正式部署

正式服务已更新到 `http://127.0.0.1:8765`：

- PID：`40088`
- EXE SHA-256：`D086F76D0D438DB86AFE94829CB172735A2AB59E17DE85D270E431922BC52ECF`
- 页面资产：`index-BV8XOU_g.js`、`index-CKHvx5XI.css`
- 主页 HTTP 200；健康页识别 51,935 个证券、1,159 个板块、379 个公式、
  580 个 JSN 资源和 47,669 个证券键；
- 正式 OpenAPI 中批量路径存在 POST，不存在 GET。

## 后续方向

下一步不应继续扩大单次请求上限。更高收益的是增加批次结果的列选择/排序与
CSV 导出，以及基于 CFG 审计结果自动生成最小输入模板；这些都可以建立在当前
有界批量内核上，不再复制宿主字段或行情请求逻辑。
