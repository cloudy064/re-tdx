# 基金独门与汇金证金十九视图固定化

## 客户端语义与资源

本轮继续处理非 L2 的机构持仓功能，不新增重复命令，而是扩展已有纯 C++
`market institution-analysis`。客户端页面和 CFG 对应两张主表：

- `list/func_tbgz108_1.jsn`：基金独门；
- `list/func_tzcg104_1.jsn`：汇金证金。

当前远端资源分别为 12,442 和 12,663 字节，MD5 为
`3d4c16fa58ea5cb14e5b4b54769f14ba`、
`c55a83b89729b4e5ca29213f3ecaf82b`。两张表已由原生 `709/1721` 下载器更新到
`output/tdx-jsn/list` 并通过远端 MD5 校验。

`func_tbgz108.cfg` 确认 `glgs/bgq/cgsl/zb` 分别是基金管理公司、报告期、持股
数量（万股）和占流通股比例（%）。同一股票出现多家管理公司是业务明细，不能
按股票去重。客户端还会从宿主补现价、流通市值、总市值和行业，这些列不属于
JSN，因此接口不伪造。

`func_tzcg104.cfg` 确认 `zjcg/hjcg/hj` 是证金、汇金和合计持股占比（%），
`gdmc/hy/date` 是股东名次、行业和截止日期。客户端显示的三列持股市值依赖宿主
总市值，纯静态表中没有可靠输入，同样不生成虚假值。

## 当前数据校验

基金独门当前 188 行、100 只股票、71 家基金管理公司，报告期分布为
`20260331` 37 行、`20260630` 151 行；其中 88 行是同票额外管理公司记录。
`cgsl` 合计 41,844.62 万股，规范字段同时输出 418,446,200 股。记录按占流通
比例降序，名称解析 188/188。

汇金证金当前 191 行、191 只股票、43 个行业，截止日均为 `20260331`：

- 证金持股 76 只；
- 汇金持股 143 只；
- 两者同时持股 28 只；
- 191 行均满足“空值按零处理后，证金比例 + 汇金比例 = 合计比例”，不一致 0。

平安银行现可在 `national-team` 视图命中：证金持股占比 1.75%，第五大股东，
行业为银行。加入两张表后，单票聚合从 17 扩展为 19 个视图，平安银行命中
8 类；`SZ000061` 样本命中 11 条，并保留基金独门的两家管理公司记录。

## 纯 C++、API 与网页

新增的两个视图为：

```powershell
tdx-tool market institution-analysis --view exclusive-funds
tdx-tool market institution-analysis --view national-team
tdx-tool market institution-analysis --view security --market sz --code 000001
```

固定 API 继续使用 `/api/v1/market/institution-analysis`。响应保留标准化字段、
原始行、来源端点、尝试次数、陈旧状态和上游错误；基金独门额外汇总唯一股票、
基金公司、重复明细及万股到股的合计，汇金证金额外汇总两类机构覆盖和公式检查。

Svelte 数据中心新增“机构持仓全景”入口，可切换 19 个视图、检索并跳转个股；
个股工作台的“更多”区域新增机构全景页签，单次聚合 19 张表，基金独门和汇金
证金使用专用字段展示。运行时仍只有 C++ 服务，不调用或转发 Python。

## 覆盖、契约与部署

两张模板类型化后，JSN 覆盖由 243/616 提升到 245/616，通用独占由 373 降到
371；本地仍为 338 个下载文件、295 个下载模板，已下载但通用独占降到 83，
解析错误为 0。审计证据为 `output/probes/jsn-variants-current.json`。

新增两个正式 API 契约，分别检查真实行数、名称解析、原始行、降序排序、基金
公司重复明细、汇金证金合计公式、精确资源路径和上游健康。全量 CTest 54/54，
正式服务 API 契约 49/49，报告位于
`output/probes/api-contracts-official-current.json`。

正式服务地址为 `http://127.0.0.1:8765`，PID `41260`，功能目录 99 项；
`native_cpp=true`、`python_runtime=false`。部署哈希：

- `tdx-tool.exe`：`E532651550C91C91AC8CD5D118CDAF700CCCCE0DEF89764BC121B117959849EC`；
- `index.html`：`5B4821367982D66F17A87616016961724677D496201263674ADD0F88FEDA864E`；
- `index-faac46nk.js`：`456A3CCB639BFE0D87FE3F9E4D6EE5E9B1279A09BB472D83EB37E5FE252DE693`；
- `index-Uuc5HlG9.css`：`AB4074662147A30F2643B8D68A57A6009A762C18B383730B1C59B0C7C9F2B4D8`。
