# TPool 每日历史文件解析

## 结论

TPool 的两类每日文件已从“动作计划中的路径和字段说明”推进为可直接使用的纯 C++
只读能力。新子命令 `pool history` 支持重复 `--input`，也能从安装根目录扫描
`tpool/<池>/<节点>/<YYYYMMDD>.dat/.log`，按池、节点、日期和类型过滤。显式
`--input` 还支持原版 `_in_pool_his.txt` 与 `_status_his.txt`，从而形成 XML 与原版
文本之间的双向只读转换。

## 恢复的格式

IDA 静态证据把两条写入路径固定为：

- `sub_10018380`：`.log` 的 `root/data/stk`，字段为
  `market/code/indate/intime/inprice`；同一 `market+code` 先替换后追加；
- `sub_10017CC0`：`.dat` 的 `root/data/stk`，字段为
  `market/code/indate/intime/inprice/income/now/rise/volume/maxrate/`
  `maxperiod/maxtime/maxprice/idaynum`。

继续追踪两个界面导出函数后，`maxtime` 的语义得到纠正：`sub_1003AC40` 将它按
`MM/DD` 输出，中文表头是“最高日期”，因此它不是最高时刻。两个原生文本契约为：

- `sub_1003A720`：
  `市场|代码|名称|进入日期|进入时间|进入价`；
- `sub_1003AC40`：在前六列后追加
  `最高收益率|最高周期|最高日期|最高价格`。

两者都以 GBK 表头开头，每条数据前置 CRLF，价格保留两位；进入时间只显示时分。
完整反编译、字符串原值和 xref 在
`output/ida-tpool-text-history-exports-20260812.json`，精简偏移映射在
`output/ida-tpool-text-history-semantics-20260812.json`。

证据摘要保存在 `output/ida-tpool-history-file-semantics-20260812.json`。当前安装目录
没有可用的真实 tpool 日文件，因此验证采用按上述精确字段构造的只读夹具；发现和
解析器不依赖夹具名称或内容。

## 原生实现

- `cloud/tpool_history.cpp`：扩展名分派、XML 记录解析、类型化字段、市场与证券归一化、
  缺失/非法字段、重复项、文件身份及跨文件汇总；
- `cloud/tpool_history_export.cpp`：复现两种原版文本布局，并拒绝不完整记录和
  `.dat/.log` 混合导出；
- `cloud/tpool_history_text.cpp`：按精确中文表头识别原版入池/状态文本，自动识别
  GBK 或 UTF-8，解析列值、完整性和精度信息；
- `cloud/tpool_history_command.cpp`：CLI 参数、根目录定位和 JSON 输出；
- `pool history --root ...`：扫描已有三个 tpool 候选目录，最多返回 10,000 个文件；
  超限时按日期倒序保留最新文件；
- 所有结果标记 `read_only=true`，不加载 TPool.dll、不启动 worker、不调用宿主回调，
  也不修改通达信目录。

`--native-text-output` 默认写 GBK，也可显式选择 UTF-8。名称通过 `--root` 或
`--name-root` 从本地证券目录解析；未命中保持空字符串并计入
`unresolved_name_count`。JSON、文本和任何输入历史文件不能指向同一路径。
反向读取文本时，进入时间明确标记为分钟精度；状态文本中的最高日期仅有 `MM/DD`，
因此输出 `maximum_date_month_day` 和 `month-day` 精度，不凭文件名或当前日期推断年份。
没有外部名称目录时，重新导出会沿用文本内嵌名称。

代表输出为 `output/tpool-history-inspection-20260812.json`，聚合两个夹具文件得到
3 条完整记录和 3 个证券。后续已把同一纯 C++ parser 接入固定只读 GET
`/api/v1/pools/history`：服务只能扫描 ApiState 配置的 root，查询白名单为
`pool/cell/from/to/limit`，且 `kind` 只能是 `all|snapshot|entry`；端点明确拒绝
`input/path/url/file`。pool/cell 只能是单层目录名，不接受路径；响应删除 root，并把
目录 `path`、文件 `source` 稳定投影成 root-relative 路径，浏览器不会看到服务器
绝对目录。TPoolLab 的“每日历史文件”页提供同一组筛选、文件表和成员表；局部或整体
无数据都显示真实空态，不生成示例记录。

## 聚焦验证

- 增量构建：`tdx-tpool-tests`、`tdx-tool`；
- TPool 专项测试通过，覆盖 `.dat` 14 字段、`.log` 5 字段、市场映射、日期/时间、
  重复诊断、缺失/非法字段、目录发现和筛选；
- 代表 CLI：2 文件、3 记录、3 完整记录、3 个证券；
- 临时 18877 的 `health/features/openapi` 为 3/3，报告为
  `output/api-contract-tpool-history-focused-20260812.json`；临时服务自动退出，正式
  8765 PID 24096 未重启；
- 原版文本增量：状态样例 2 行、名称 2/2 命中，GBK SHA-256 为
  `308e82fb6e520f092a68f8c9dd5213da7500f4295ca53562d163fdfbe160caca`；入池样例
  1 行，旧北交所代码未命名并保持空列。输出碰撞在写入前以退出码 2 拒绝；临时
  18878 上 `health/features/openapi` 仍为 3/3，报告为
  `output/api-contract-tpool-native-text-focused-20260812.json`；
- 原版文本反向解析增量：状态与入池两个 GBK 样例均可解析并逐字节往返，UTF-8
  状态样例也能自动识别；专项测试覆盖表头识别、分钟/月份日期精度、非法列和不完整
  记录拒绝。最终二进制 `serve --self-test` 返回 `ok=true`、`feature_count=155`；
- 未运行完整 CTest 或完整 API 契约集。

第二批 HTTP/UI 聚焦验证使用临时 root，`tdx-server-tpool-history-tests` 通过，覆盖
`.dat/.log`、日期、limit、非法 kind、路径参数拒绝、root-relative 投影、绝对根目录
不泄漏以及源文件 SHA-256 不变；`tdx-server-catalog-tests` 通过，确认 OpenAPI 对
history 只列 GET。`npm run check` 为 0 errors / 0 warnings，`npm run build` 通过且
只有既有 chunk warning。没有运行完整 CTest 或 full API 契约。

当前产物已 build/install/restart 到正式 `127.0.0.1:8765`，PID `11468`；EXE
SHA-256 为
`3881917c02000fad1dcc8d6e9d956e92022f910932a7c1aad2f0fc3176c0fb54`，网页
`index.html` SHA-256 为
`f159a6ec60e0074bf213ac86582f27482f41666d09b60262e7c191f42bf408b6`。
运行时 focused 确认构建、安装与 health 哈希一致；history 返回 HTTP 200 与正确
schema，当前真实安装为 0 文件/0 记录空态且 root 隐藏，传入 `path` 返回 400，
OpenAPI 为 GET-only，TPoolLab 文案已进入部署网页。此前 PID 24096 的“未重启”仅是
本日志早期阶段记录，不代表当前部署。

## 保留边界

真实用户目录样本尚缺，因此尚未验证不同客户端版本是否增加可选属性。原 worker 的
周期写入、过期删除和宿主 UI/文件副作用仍不执行；工具只读消费现有文件。
