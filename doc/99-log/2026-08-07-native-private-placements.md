# 定向增发六阶段生命周期固定化

日期：2026-08-07

## 结论

`QXFA.sp` 的定向增发不是现有 IPO/债券发行人、`0x000F` 单票股本事件或
`DBLJJ` 限售解禁的重复入口。客户端用六张非空 JSN 主表维护不同生命周期；
本轮已把它们并入纯 C++ `market futures-issuance --section placements`、只读
API、Svelte 数据中心和个股工作台。

## 资源与语义

| 资源 | 语义 | 行数 | 字节 | MD5 |
|---|---|---:|---:|---|
| `func_qxfa201_1` | 已实施、锁定中 | 267 | 132,244 | `9b153ee08a7fb05efea48bbbf9e3febc` |
| `func_qxfa202_1` | 已实施、已解锁 | 215 | 106,139 | `ab7be405560ff4429209626237d0870d` |
| `func_qxfa301_1` | 方案推进 | 411 | 194,125 | `7f298cd5975f450e0b6bc4ddaf71ba78` |
| `func_qxfa302_1` | 终止/中止审查/延期 | 80 | 45,868 | `6dc50548fa7eaf6dd2b9642dcb96ec63` |
| `func_qxfa402_1` | 已实施 | 288 | 65,946 | `b787c0a6fc1120a00572bf2afb7337d0` |
| `func_qxfa601_1` | 注册生效 | 78 | 50,017 | `5f29118d3dff6c2cf81219f0affe9bfe` |

合计 1,339 条、1,008 只去重证券。阶段原值进一步对账为：董事会通过 130、
股东大会通过 281、注册生效 78、已实施 288、锁定中 267、已解锁 215、
已终止 58、中止审查 13、已延期 9。

`func_qxfa401_1` 当前远端为空，不作为必要源；`func_qxfa501_1` 当前 1,177 条，
其语义是员工持股计划，未混入定向增发，现已由 `market employees` 独立类型化。

## 归一化边界

- 每条记录保留 `source_resource`、`lifecycle` 和 `raw`；同一股票跨阶段或多期
  方案不会按代码粗暴合并。
- `mzze/mzje` 保持客户端万元口径，`fxzl/fxgm/fxhgb` 保持万股口径；网页显示
  时才乘 10,000。
- 日期按业务拆成董事会、股东大会、监管获准、注册、实施、上市和解锁。
- 只在原字段足够时复算获准至实施收益、锁定期收益和发行股数占发行后股本比例；
  没有公开 L1 时不会伪造“发行至今”收益。
- `market+code` 必须成对出现，市场仅接受 `sz/sh/bj` 或 `0/1/2`。

## 接口与界面

```text
tdx-tool market futures-issuance --root C:\new_tdx \
  --section placements --placement-status registered --q 工业机械

GET /api/v1/market/futures-issuance
    ?section=placements&placement_status=all&limit=5
```

数据中心“期货与发行”新增“定向增发”分区，可按生命周期和文本检索，右侧展开
日期链、变更说明、发行详情和注册公告 PDF。个股工作台“筹码股东”组新增
“定向增发”页签，通过同一接口的 `market+code` 严格过滤。

## 验证

- C++：67/67 单元测试通过；新增 normalizer 与生命周期、万元/万股口径测试。
- Svelte：`svelte-check` 0 错误、0 警告，生产构建通过。
- 在线样本：注册生效 78/78 返回；`SH603076` 乐惠国际正确给出董事会
  `20260311`、股东大会 `20260327`、注册 `20260801`、预计募资 35,000 万元
  和注册公告 PDF。
- 固定契约：新增 `private-placements-live`，要求六种生命周期、至少 1,000 条/
  800 只证券、日期倒序、单位总量和六个精确来源同时成立。
- 正式服务所选契约 1/1 通过；full 为 101/102，唯一失败仍是既有
  `margin-classifications-live` 的远端零长度 JSN，和本次定增链无关。

## 正式部署

- 服务：`http://127.0.0.1:8765`，PID `1172`；健康页报告 361 个 JSN 资源、
  10,431 个证券索引键和 115 个功能。
- `tdx-tool.exe`：34,999,259 字节，SHA256
  `A7DD6717AE48D938540C0D53047B0BFDCE527502E65253FCCDB0527D4E612613`。
- Svelte：`index-CBX5lvBX.js` / `index-C9lGCXH8.css`；HTML 已引用该组资源。

证据文件：

- `output/probes/private-placement-resource-audit-20260807.json`
- `output/probes/private-placements-all-live.json`
- `output/probes/private-placements-registered-live.json`
- `output/probes/jsn-catalog-after-qxfa-20260807.json`
- `output/probes/api-contract-private-placements-20260807.json`
- `output/probes/api-contract-full-after-private-placements-20260807.json`
