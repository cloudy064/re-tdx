# TBigData 最小输入模板与网页补齐

> 日期：2026-08-08。范围：非 L2、纯 C++ TBigData `calc/calcref` 解释器。

## 目标

解决调用方必须先理解 CFG 全部列定义才能构造 `cloud-calc` 输入的问题。模板必须
来自配置依赖图，而不是保存一份容易过期的手写样例；同时不得把可由宿主补值、
同行派生或解释器计算的字段误报成必填输入。

## 实现

- 新增 `formulas cloud-calc --cfg NAME --template`。它只接受安全 CFG 名称，
  输出 `tdx-tbigdata-cloud-calc-template-v1`，不加载 DLL、不访问行情、不暴露
  通达信安装路径；
- 遍历每个 unit 的 `calc/calcref/refzqdm/refunit`，将依赖分为
  `input_fields`、`host_fields`、`derived_fields` 和 `calculated_fields`；
- 关联证券自动加入带后缀的 `$SC/$ZQDM` 身份字段；`$BONDAI` 加入面值、发行
  日期和利率序列等同行前置字段；`DQLL2` 明确标记为从 `SYFXLLXL` 取得首期
  利率；`$S_ZQDM` 保留板块成员宿主语义；
- 新增只读 `GET /api/v1/formulas/cloud-calc/template?cfg=NAME` 和 OpenAPI 声明；
- Svelte“榜单计算列解释器”新增“补齐最小模板”：单对象和对象数组都会补齐
  缺失键，当前已有值保持优先。浏览器模板仍只在用户显式保存后写入
  `localStorage`。

真实 `func_kzz_kzzsy101.cfg` 的结果为 19 个调用方输入、3 个宿主字段、1 个同行
派生字段、15 个计算字段和 1 个 unit。`row_template` 含主证券/正股身份、面值、
票息序列等 19 个空值键，不含 `$NOW3`、`ZGXJ`、`DQLL2` 或 `QJ` 等非调用方输入。

## 安全与边界

- `--template` 必须指定 `--cfg`，并与行、JSN、行情、快照、覆盖和批量选项互斥；
- HTTP 仍不接受文件路径，响应没有绝对路径或 CFG 路径；
- 模板描述“最小结构”，不是业务有效值生成器。`null` 必须由调用方填写，或由
  后续实际数据行覆盖；
- 宿主字段的分类只表示现有纯 C++ 宿主上下文有解析器，不表示离线且没有快照时
  一定有值。

## 验收与证据

- 原生全量测试：98/98；Svelte 检查 0 error / 0 warning；生产构建通过；
- 临时服务专项合约 4/4、全量合约 167/167；
- 正式服务专项合约 4/4、全量合约 167/167；
- 正式服务 PID `40112`，可执行文件 SHA-256
  `A25352293D8986C11FEEB65EE75E059D5B3DB745B029E95B468625C89B22D203`；
- 正式前端入口引用 `assets/index-BRt3Fwd-.js` 和
  `assets/index-CKHvx5XI.css`。

产物：

- `output/tdx-tbigdata-cloud-calc-template-kzz.json`
- `output/probes/api-contracts-cloud-calc-template-temp-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-cloud-calc-template.json`
- `output/probes/api-contracts-cloud-calc-template-formal-20260808.json`
- `output/probes/api-contracts-full-formal-20260808-cloud-calc-template.json`

