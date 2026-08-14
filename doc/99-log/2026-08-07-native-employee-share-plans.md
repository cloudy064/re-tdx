# 员工持股计划类型化与本地冷启动加速

日期：2026-08-07

## 结论

`QXFA.sp` 的 `func_qxfa501_1.jsn` 是独立的员工持股计划表，不属于定向增发。
本轮已把它并入纯 C++ `market employees`、固定 API、Svelte 数据中心和个股
工作台，并让本地服务优先读取已下载 JSN 镜像，避免服务重启后重复下载大表。

当前数据为 1,177 期、840 家公司；其中“股票购买中”37 期、“股票购买完成”
1,140 期。累计购买股数为 14,108,054,632 股，可由公开均价复算的参考金额为
84,350,689,304.63 元。

## 字段与单位

- `gmgs` 是万股，接口同时保留 `purchase_shares_10k`，并输出乘 10,000 后的
  `purchase_shares` 单股口径。
- `price1` 是调整后的购买均价；只有均价和股数同时存在时才输出
  `purchase_amount_yuan`，缺值不推测。
- `zb` 是占总股本百分比点；`jhzqs/ssqs` 是计划/已实施批次，并据此计算批次
  完成百分比。
- `date1..6` 分别拆成实施起止、存续起止和锁定起止；记录按可用的最新业务日期
  倒序，不按股票代码粗暴去重。
- 每条记录保留 `event_id`、`source_resource`、`raw` 和公开详情文本。

## 接口与界面

```text
tdx-tool market employees --root C:\new_tdx --view share-plans --limit 5000

GET /api/v1/market/employees?view=share-plans&limit=5
GET /api/v1/market/employees?view=all&market=sz&code=000034
```

数据中心“员工与持股”现在可在“员工与高管 / 员工持股计划”间切换，支持按
公司、代码、行业、状态和详情检索。个股工作台同一页同时展示员工研发画像、
高管名单和该股票历期员工持股计划；神州数码样本返回 3 期计划、15 名高管。

## 冷启动边界

直接从公开 7709 拉取 `YGXC + QXFA501` 在本次网络状态下曾超过 60 秒。服务在
配置 `--jsn-root` 后现优先解析本地镜像；正式进程重启后的首次持股计划请求实测
为 1.346 秒，来源显式为 `local-jsn:`。网页“刷新”仍使用远端源，CLI 未指定
本地镜像时也保持原来的实时下载行为。

## 验证

- C++：68/68 测试通过；新增万股换算、金额复算、状态、批次和日期测试。
- Svelte：`svelte-check` 0 错误、0 警告，生产构建通过。
- 新增 `employee-share-plans-live` 契约，要求至少 1,000 期/800 家公司、状态
  对账、日期倒序、单股/元单位、原始行和精确来源；所选契约 1/1 通过。
- 正式 full 契约 103/103 全部通过。巡检期间还纠正了连板天梯把日度活跃集合
  硬编码为至少 200 条的漂移断言；现要求至少 100 条且来源数与分类数严格对账。
- JSN 覆盖为 342/616、已下载 361 个、解析错误 0；同时补登记已实现的
  `func_yysg101_1` 要约收购，已下载非空资源的 generic-only 数量重新归零。

## 正式部署

- 服务：`http://127.0.0.1:8765`，PID `24460`；115 个功能、361 个 JSN 资源、
  10,431 个证券索引键。
- `tdx-tool.exe`：35,051,993 字节，SHA256
  `DFEA3AD1B7F2E803BD9D7B9D90F41554D27F60BC462391A99D4FC4FE5EFC09EA`。
- Svelte：`index-P_5lNB7u.js` / `index-DKrMEkhP.css`。

证据文件：

- `output/probes/employee-share-plans-live.json`
- `output/probes/api-contract-employee-share-plans-20260807.json`
- `output/probes/api-contract-employee-plans-and-ladder-20260807.json`
- `output/probes/api-contract-full-after-employee-share-plans-20260807.json`
- `output/probes/jsn-variants-after-employee-share-plans-20260807.json`
