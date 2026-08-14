# 2026-08-07 美股评级主从链纯 C++ 固定化

## 结果

通达信 `MGPJ` 已并入现有纯 C++ `market ratings` 与
`/api/v1/market/ratings`：

```powershell
tdx-tool market ratings --root C:\new_tdx --view us
tdx-tool market ratings --root C:\new_tdx --view us `
  --market us --code ABNB --details
```

主表来自 `list/func_mgpj101_1.jsn`；单票详情使用客户端 `refunit=13601`
验证出的动态键 `mgpj/74<代码>.jsn`。`mgpj/ABNB.jsn` 为缺失，
`mgpj/74ABNB.jsn` 非空，因而市场号不是可省略前缀。

## 当前真实样本

2026-08-07 下载的主表为 3,238 字节、MD5
`7a85aa371bcaf31ecc1c243d0d87bddf`：

- 40 只美股；
- 13 个行业；
- 机构覆盖数 `JGS` 合计 926；
- 30 只带最新目标价；
- 最新评级日期为 2026-08-07。

`ABNB` 详情为 12,384 字节、MD5
`5389cf7b62d535012478b79511162ea6`，归一后 159 条。最新一条为：

- 评级日 2026-08-07；
- 韦德布什；
- 中性上调为跑赢大市；
- 目标价 200 美元；
- 客户端字段 `QCJ`（期初价）为 152 美元。

`QCJ` 没有被猜成“上次目标价”，输出保守命名为 `initial_price_usd`。

## 实现边界

- `--view us` 使用市场 `us/74`；
- 美股代码允许字母、数字、点、横线、脱字符和下划线，拒绝路径字符；
- 主表保留证券名、最新机构/评级/目标价、机构数和行业；
- 详情保留当前/上次评级、调整类型、目标价与期初价；
- 中英文评级均归一为 `positive/neutral/negative/unknown`；信用评级等无法可靠
  映射的文本继续归为 `unknown`。

## 验证

- `tdx-ratings-tests` 通过；
- 主表端到端：`availability=live`、40/40；
- ABNB 详情：`availability=live`、159 条、0 个详情错误；
- MGPJ 主表和动态详情已登记到 JSN 类型化覆盖；发现器可从
  `mgpj/74ABNB.jsn` 恢复 `SC=74`、`ZQDM=ABNB`，当前 503 个已下载资源
  全部为 `typed-command`，未识别数为 0；
- 全量 C++ 回归 92/92，通过；正式服务全量 API 契约 129/129，通过；
- 正式服务由 `dist/tdx-tool/bin/tdx-tool.exe` 提供，PID 5012，SHA-256
  `730CC6CCF5A3872C595338D3280BB3F9B8C59FADCEF777F2A8D2C21DD317BDDD`；
- 产物：
  - `output/tdx-market-ratings-us.json`
  - `output/tdx-market-ratings-us-abnb.json`
  - `output/recon-api-contract-jsn-discovery-mgpj.json`
  - `output/recon-api-contracts-us-ratings-final.json`
