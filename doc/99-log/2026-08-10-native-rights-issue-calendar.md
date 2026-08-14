# GSRL207 配股四阶段日历与 JSN 缺口复探

日期：2026-08-10

## 缺口筛选

重新对 600 个本地 JSN 文件和 622 个资源模板执行纯 C++ 审计，已下载资源仍为
600/600 类型化、解析错误为 0。对剩余 35 个无专用语义模板使用公开 7709 元数据
链做有界探测后，34 个返回零长度，只有 `list/func_gsrl207_1.jsn` 可用：841 字节、
MD5 `6c15a586e4ce56df44bb3ee0356e9203`。

## 业务语义

`func_gsrl207.cfg` 把该表定义为证券事件、日期和事件正文。当前真实响应属于
`SZ300176`，包含四个配股节点：

- `record-date`：配股股权登记日；
- `payment-start`：配股缴款开始日；
- `payment-end`：配股缴款结束日；
- `ex-rights`：配股除权基准日。

纯 C++ `market calendar` 已加入第 28 个本地优先数据源和独立
`--view rights-issues`，保留原始事件正文、证券关联及阶段枚举。网页财经日历增加
“配股日历”入口，没有把配股方案表伪装成事件表，也没有从正文推测缺失日期。

## 验证

- `tdx-calendar-tests`：通过；
- `tdx-jsn-variants-tests`：通过；
- Svelte 类型检查：0 错误、0 警告；
- 真实 `rights-issues` 查询：4/4 返回，四个阶段各 1 条，28 个来源均为本地读取；
- JSN 覆盖：588/622 类型化，601 个下载文件全部类型化，剩余 34 个缺口均没有
  已下载数据。

证据文件：

- `output/tdx-jsn-gap-probe-20260810.json`；
- `output/tdx-jsn-gsrl207-download-20260810.json`；
- `output/native-market-calendar-rights-issues-v23.json`；
- `output/tdx-jsn-variants-rights-calendar-v23.json`。

候选 `tdx-tool.exe` SHA-256 为
`F6F0891253193E6F657A0B64E1AB3CF7F3C2E650DDB67FD307FC7150A2EF3F66`；
本批未替换 8765 正式服务。
