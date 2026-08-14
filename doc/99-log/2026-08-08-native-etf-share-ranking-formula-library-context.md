# ETF 份额榜与内置公式显式上下文执行闭环

> 日期：2026-08-08。范围：客户端 `ETFJHB` 配置、公开 7709 JSN/L1、纯 C++
> CLI/API、Svelte 数据中心与公式工作台；不使用 Python、登录账号或 Level2。

## ETF 份额榜资源

客户端 `func_tlfeyxetf101.cfg` 的页面标题为“ETF份额榜”，对应静态资源：

```text
list/func_tlfeyxetf101_1.jsn
```

通过统一工具的纯 C++ `709→1721` 下载链取得 64,707 字节，远端 MD5 为
`713e32ccd5a33f06b3b3a8dea422db1c`，共 458 行。字段按 CFG 语义类型化：

- ETF 与跟踪指数分别使用 `$SC/$ZQDM`、`$SC1/$ZQDM1`；
- `ZXFE/FEBH/ZFEBH/YFEBH` 分别为最新、日、周、月份额变化，单位为份；
- `YZGM/YYGM` 是一周前和一月前的规模基准，单位为元；
- `ZXSSDW` 的原始单位是万份，接口同时保留
  `subscription_unit_10k_shares`，并精确乘 10,000 输出
  `subscription_unit_shares`；
- `JLR` 作为 `net_inflow_yuan` 保留上游原值。本期 458 行全部为 0，摘要明确
  输出非零行数 0，不以份额变化推导或伪造资金流。

`market exchange-funds` 新增 `etf-share-ranking` 视图。公开 L1 行情核心已能
解析 `fund_iopv`，因此在 `include_quotes=1` 时按客户端公式本地复算：

```text
当前规模 = 最新价 × 最新份额
日规模变化 = 最新价 × 日份额变化
周规模变化 = 最新价 × 最新份额 - 一周前规模
月规模变化 = 最新价 × 最新份额 - 一月前规模
折溢价率 = (最新价 - IOPV) / IOPV × 100
```

正式数据实测 458/458 条取得公开行情，458 条均得到当前规模和 IOPV，请求约
521 ms，无行情错误。Svelte 场内基金页新增 ETF 份额榜、跟踪指数、份额变化、
规模变化、最小申赎单位和折溢价展示。

## 内置公式与显式上下文 POST

原网页只能生成 L2/券商私有序列的空模板，用户仍需复制公式源码到自定义模式。
本轮扩展确认 POST：

```http
POST /api/v1/formulas/evaluate
X-TDX-Action: formula-evaluate
Content-Type: application/json

{
  "market": "sz",
  "code": "000001",
  "formula": "ZJLX",
  "formula_kind": "technical",
  "context": { "series": { "L2_AMO#0#2": { "2026-08-07|15:00": 1 } } }
}
```

当请求省略 `source` 时，服务端只从已恢复的内置公式库按公式代码和类型选择正文，
再使用调用方提交的显式序列执行。响应标记
`formula_source_mode=library-post`、内置公式代码和类型；请求体与公式正文不留存。
原有带 `source` 的 `inline-post` 行为保持不变。

公式详情页现在把模板放入可编辑 JSON 文本框；只有用户把 `null` 替换为合法数值
后才提交。空模板、缺失绑定或非法 JSON 继续明确拒绝，不下载、推导、填零或绕过
L2/券商授权。本轮使用 8 个 `L2_AMO#i#j` 键验证 `ZJLX`，成功计算 800 个日线
点并在 2026-08-07 取得数值输出。

## 覆盖、验证与部署

- JSN：510/618 个模板已类型化，529 个本地文件全部匹配，解析错误 0，已下载
  通用独占 0；
- CTest：96/96；
- Svelte：`check` 0 错误/0 警告，生产构建通过；
- 新增/关联正式契约 4/4，full 145/145；
- 正式服务：`127.0.0.1:8765`，PID `37600`，529 份 JSN，
  `native_cpp=true`、`python_runtime=false`；
- 发行版 SHA-256：
  `5308C4E87FAB738BF033F350F476019367AABC5DB84E8FF02D8730AFCB648E3B`。

证据文件：

- `output/etf-share-ranking-check.json`；
- `output/tdx-jsn-variants.json`；
- `output/api-contracts-etf-formula-selected.json`；
- `output/api-contracts-etf-formula-formal-selected.json`；
- `output/api-contracts-etf-formula-formal-full.json`。

下一轮继续只选择配置语义、字段单位和非空数据都能闭合的公开资源；ETF 净流入
字段后续若出现非零样本，再以增量契约验证其币种和刷新频率，不提前猜测。
