# 本地投资组合、明细完整性与交易费率

日期：2026-08-12

## 结论

`invest.dll` 的最高收益本地能力已迁入纯 C++ 统一工具：

```powershell
tdx-tool market investment --root C:\new_tdx
```

命令读取组合目录、检查每个组合明细文件，并解析交易费率。它是只读、CLI-only：
不加载原 DLL、不使用 Python，也不把组合私有数据暴露到 HTTP 服务；只有显式
`valuation` 视图且未提供离线行情快照时才请求公开 L1。密码值、`pinfo.dat`
未定标尾部和 `.da0` 保留字节永远不会写入 JSON；交易
明细必须显式选择组合，备注文本还需单独 opt-in：

```powershell
tdx-tool market investment --root C:\new_tdx --view transactions `
  --portfolio "组合名称" --offset 0 --limit 1000

tdx-tool market investment --root C:\new_tdx --view transactions `
  --portfolio "组合名称" --include-notes

tdx-tool market investment --root C:\new_tdx --view holdings `
  --portfolio "组合名称"

tdx-tool market investment --root C:\new_tdx --view valuation `
  --portfolio "组合名称"

tdx-tool market investment --root C:\new_tdx --view valuation `
  --portfolio "组合名称" --quotes output\market-snapshot.json
```

当前真实安装 `C:\new_tdx` 的 `T0002\invest` 为空，`trdpara.dat` 也不存在；
实测返回稳定空状态而不是错误。完整非空行为由固定二进制夹具覆盖，不能把夹具
冒充用户真实持仓。

## 原生文件契约

### `pinfo.dat`

`invest.dll!sub_1001A890` 初始化固定文字密钥，`sub_10004970` 完成 Blowfish
key schedule，`sub_10004100/sub_10004530` 分别处理加密/解密块，
`sub_10004A70/sub_10004AD0` 以 8 字节为单位遍历整个缓冲区。字块按小端
`uint32` 装载，工作模式是 ECB。

文件由连续 192 字节密文记录组成。解密后已证实的稳定布局是：

| 偏移 | 宽度 | 含义 | 输出策略 |
| ---: | ---: | --- | --- |
| 0 | 21 | NUL 结尾 GBK 组合名称 | 输出 UTF-8 名称 |
| 21 | 21 | NUL 结尾组合密码 | 只输出是否非空 |
| 42 | 150 | 私有元数据 | 保持不透明，不输出 |

这里的偏移按 MFC 链表节点的 8 字节链接头与实际记录载荷分离后确定；此前把
节点基址误当成记录基址所得的 `29/29` 已纠正为 `21/21`。解析器要求现有文件
长度是 192 的整数倍，名称和密码都必须在固定字段内出现 NUL。文件不存在则是
正常空状态。

### `<组合名称>.da0`

`sub_10016EC0/sub_10017270` 证明组合明细与 `pinfo.dat` 使用相同密码算法，记录宽度
固定为 200 字节。第 0 条是密码头，宿主把解密后的前 21 字节与组合密码核验；
后续记录才是交易数据。

处理函数、12 个写入 handler、流水帐投影和 MFC 命令资源共同确认了交易记录布局：

| 偏移 | 类型/宽度 | 含义 |
|---:|---:|---|
| 0 | `int32` | 证券市场号；纯现金类型不使用 |
| 4 | `int32` | `YYYYMMDD` 交易日期，也是原版排序键 |
| 8 | `char[101]` | NUL 结尾 GBK 备注，写入最多 100 字节 |
| 109 | `int32` | 现金方向：-1 支出、0 不动现金、1 收入 |
| 113 | `char[10]` | NUL 结尾证券代码；纯现金类型为空 |
| 123 | `int32` | 业务类型 |
| 127 | 20 字节 | 未定标保留区，不输出 |
| 147 | `int32` | 数量 |
| 151 | `double` | 单价/每股值 |
| 159 | `double` | 费用 |
| 167 | `double` | 按业务类型记录的合计值 |
| 175 | 25 字节 | 未定标保留区，不输出 |

命令表中的 ID、原 handler 和业务类型形成直接证据链：

| 类型 | 原菜单 | 现金方向 |
|---:|---|---:|
| 0 | 买入股票 | -1 |
| 1 | 卖出股票 | 1 |
| 2 | 股票分红 | 1 |
| 3 | 送股 | 0 |
| 4 | 配股 | -1 |
| 6 | 追加资金 | 1 |
| 7 | 划出资金 | -1 |
| 9 | 现金红冲 | -1 |
| 10 | 现金蓝补 | 1 |
| 11 | 划入股票 | 0 |
| 12 | 划出股票 | 0 |
| 13 | 送衍生品种 | 0 |

类型 5/8 在当前命令表没有写入入口，解析器保留为 `unknown`，不擅自命名。输出字段
使用中性的 `unit_price/fee/recorded_total`，避免把不同业务类型的合计值一概误称为
成交额；`cash_direction` 才决定现金影响。

`--view transactions` 按 `--offset/--limit` 分页，默认 1000、上限 10000。由于 ECB
记录边界与 8 字节块对齐，工具只读取并解密第 0 条密码头和当前页，不再为一次头校验
或分页查询全量解密整个 `.da0`。包含路径分隔符、Windows 保留字符、尾随点/空格的
组合名不会用于文件查找，避免目录穿越。

### 持仓解释器

`--view holdings` 按文件顺序分块重放全部流水，复现
`sub_10013B90/sub_10015230` 的核心状态转移：

- 买入、配股和划入股票以 `数量×单价+费用` 加入移动平均成本；
- 卖出数量不再占用成本，但按“原生净收入合计－数量×当时平均成本”累计已实现盈亏；
- 现金分红从持仓成本中扣减；
- 送股和送衍生品种增加数量并摊薄平均成本；
- 划出股票减少数量但不确认卖出盈亏；
- 仓位归零时平均成本清零，已实现盈亏继续累计。

结果包含当前数量、平均成本、持仓成本、已实现盈亏、现金分红、费用、数量来源拆分，
以及全组合现金流入、流出和净流量。默认隐藏数量为零的已了结证券，
`--include-closed` 可显示。算法要求流水日期非递减、卖出/划出不能形成负持仓；发现
损坏或不完整记录时显式报错。`holdings` 仍不伪造市值；需要行情的派生值由独立
`valuation` 视图提供。

### 公开 L1 估值

`--view valuation --portfolio <名称>` 先使用同一个持仓解释器，再一次性批量取得
公开 `0x054C` 行情；`--quotes` 可改用调用方已有的
`tdx-market-snapshot-native-v1` 文件，从而离线复算。每个活动持仓输出：

- 当前价、昨收、涨跌幅和行情时间；
- 市值、毛浮盈、收益率、当日盈亏；
- 按同市场最长代码前缀 `trdpara.dat` 规则估算的卖出费用；
- 扣除预计卖出费后的净变现价值和浮盈；
- 使净卖出收入刚好覆盖当前移动平均持仓成本的含费保本价。

组合摘要分别报告行情覆盖、费率覆盖、已覆盖市值、已实现/未实现盈亏、重建现金余额
和总资产。只要有一个活动持仓缺行情，`complete=false`，`total_assets` 与
`total_profit` 就保持 `null`；部分覆盖值仍以 `covered_*` 字段明确给出。现金余额
是流水现金流入减流出，仅在 `.da0` 中确实记录了期初资金时才是完整账户现金，schema
对此显式说明。费率缺失不会猜默认值，相应净变现和保本价保持 `null`。

### `trdpara.dat`

TdxW 使用精确路径 `T0002\trdpara.dat`。文件由连续 79 字节小端记录组成：

| 偏移 | 类型 | 含义 |
| ---: | --- | --- |
| 0 | `int32` | 市场：0 深、1 沪、2 京 |
| 4 | `char[7]` | NUL 结尾证券代码前缀 |
| 11 | `char[20]` | NUL 结尾 GBK 市场名称 |
| 31 | `double` | 佣金比例 |
| 39 | `double` | 最低佣金 |
| 47 | `double` | 印花税比例 |
| 55 | `double` | 过户费比例 |
| 63 | `double` | 最低过户费 |
| 71 | `double` | 固定费用 |

`sub_10012020` 在相同市场内选择能匹配证券代码左侧的最长前缀。费用公式来自
`sub_1000D390`：

```text
成交额   = 价格 × 数量
佣金     = max(成交额 × 佣金比例, 最低佣金)
印花税   = 卖出 ? 成交额 × 印花税比例 : 0
过户费   = max(成交额 × 过户费比例, 最低过户费)
费用合计 = 佣金 + 印花税 + 过户费 + 固定费用
```

工具接受 `--market/--code` 查看选中的规则，并可同时传
`--price/--quantity/--side buy|sell` 复算费用。缺规则时不会套用猜测默认值。

## 路径与隐私

TdxW 通过 `SetPrivateInvestDir` 接收私有目录，配置来源是
`T0002/user.ini` 的 `[OTHER]/INVESTPATH`。统一工具采用同一优先级：

1. 显式 `--private-dir`；
2. `user.ini [OTHER]/INVESTPATH`，相对路径以 `T0002` 为基准；
3. `T0002/invest` 约定回退。

`--trdpara` 可单独覆盖费率文件，默认仍是 `T0002/trdpara.dat`。输出 schema 为
`tdx-investment-portfolio-v1`，并明确标记实际行情批次数、备注是否输出、交易字段
是否解码及未知字节永不输出。除 `valuation` 实时模式外，`network_requests=0`。

## C++ 组织

- `include/tdx/investment.hpp`：查询契约；
- `src/trading/investment_parse.cpp`：路径、加密文件和费率记录的严格解析；
- `src/trading/investment_holdings.cpp`：按原生顺序重放持仓；
- `src/trading/investment_fees.cpp`：最长前缀选规和费用公式；
- `src/trading/investment_valuation.cpp`：行情覆盖、估值和含费保本价；
- `src/trading/investment_query.cpp`：视图编排和隐私 schema；
- `src/trading/investment_command.cpp`：CLI 编排；
- `src/registry/registry_market_local.cpp`：本地私有数据命令注册；
- `tests/investment_tests.cpp`：标准密码向量及领域夹具。

既有港股 Blowfish 算法已提升为 `src/common/blowfish.cpp` 和独立初始表目录模块；
港股包装层仍保留原资源允许不足 8 字节尾部原样通过的兼容行为，新投资文件则要求
完整块边界。

## 验证

- 增量构建 `tdx-investment-tests`、`tdx-hk-actions-tests`、
  `tdx-hk-finance-tests` 和 `tdx-tool` 通过；
- 三个专项 CTest 3/3 通过；
- 最新候选构建 `serve --self-test` 返回 `feature_count=160, ok=true`；
- 标准 Blowfish 零密钥/零明文向量得到 `4EF997456198DD78`；
- 固定夹具验证 GBK 组合名、密码不出现在 JSON、`.da0` 头核验、买入/现金调整/
  分红/送股/卖出明细、业务名称、证券/现金分支、分页、备注 opt-in、持仓数量、
  移动平均成本、已实现盈亏、现金流、最长代码前缀、卖出费用、完整/缺失行情估值、
  净变现价值、含费保本价以及部分覆盖的 `null` 总值语义；
- 最新受影响目标增量构建通过，`tdx-investment-tests` 1/1（约 0.7 秒）；
- 真实 `C:\new_tdx` 返回组合 0、费率规则 0、网络请求 0；
- 正式 `127.0.0.1:8765` PID 24096 未重启、未替换。

机器可读真实空状态：

- `output/verify-investment-empty-20260812.json`

IDA 证据：

- `output/ida-invest-crypto-20260812.log`
- `output/ida-invest-layout-candidates-20260812.log`
- `output/ida-invest-record-layout-20260812.log`
- `output/ida-invest-strings-20260812.log`
- `output/ida-invest-da0-writers-20260812.log`
- `output/ida-invest-message-map-20260812.log`
- `output/native-probe-invest-resources-20260812.log`
- `output/ida-tdxw-invest-host-20260812.log`

## 剩余边界

`.da0` 的基础交易明细已经不依赖真实私密样本。仍需样本对照的是偏移
127..146、175..199 两段旧版/保留区，以及原版 UI 对市值、浮盈和保本价的显示舍入。
工具已用公开 L1 和已恢复费率形成可审计的估值，但不会把这些派生结果宣称为单条磁盘
字段或已经逐像素对齐原网格。命令只报告保留区是否出现非零字节，不输出其内容。
