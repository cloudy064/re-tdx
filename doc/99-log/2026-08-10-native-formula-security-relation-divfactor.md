# 2026-08-10 TCalc 证券关系与 DIVFACTOR

## 结论

纯 C++ 公式解释器新增五个非 Level2 注册入口：

| 名称 | opcode | 处理函数 | 原生语义 | 数据来源 |
|---|---:|---|---|---|
| `DPZSCODE` | 1323 | `sub_10044AA0` | 所属主要指数代码 | DLL 市场/代码分支 |
| `DPZSNAME` | 1357 | `sub_10044C00` | 所属主要指数名称 | DLL 六组 GBK 常量 |
| `UNDERCODE` | 1348 | `sub_10044D60` | 标的证券代码 | TdxW type 120 偏移 176/178 |
| `UNDERLYC` | 1320 | `sub_100266A0` | 日期时间对齐的标的收盘价 | 标的公开 K 线偏移 19 |
| `DIVFACTOR` | 1359 | `sub_10012E40` | 送转股复权因子 | `0x000F`/type 164 |

前四项支持裸符号和零参数调用。代码、名称使用公式字符串池语义，在 C++ 返回中
以 UTF-8 元数据物化；没有 Python 转发，也没有使用 Level2 数据。

## 所属大盘

`DPZSCODE/DPZSNAME` 的映射直接来自处理函数和 DLL 常量：

- 深市普通证券为 `399001/深圳成指`，创业板为 `399006/创业板指`；
- 沪市普通证券为 `999999/上证指数`，科创板为 `000688/科创50`；
- 北交所为 `899050/北证50`；港股相关市场为 `HSI/恒生指数`；
- 其他市场按原处理函数回落到 `999999/上证指数`，不沿用涨跌家数函数的期货
  `L9` 扩展逻辑。

六组字符串常量的离线证据为
`output/ida-tcalc-dpzs-name-constants-v1.json`，SHA-256
`F550CCF3250E3AD5EF2DE7990934FD33CD8BB6469A8A7794E4C600824BBF022F`。

## 标的证券边界

TdxW type 120 只在存在标的关系的证券类别填充返回缓冲：市场号位于偏移 176，
六位代码位于偏移 178。实现对股票期权复用通达信期权目录，对可转债读取安装目录
中的 `T0002/hq_cache/speckzzdata.txt`；普通股票等不适用品种保持空代码和零收盘
序列，不抛“缺少标的”错误，也不猜测关联证券。

`UNDERLYC` 按当前证券 K 线周期抓取标的 K 线，以 `日期|时间` 对齐并转为
float32；匹配值近零时按处理函数继承前一项，未匹配点保持 0。真实
`SH110075` 120 根日线全部对齐到 `SH600029`，日期范围为 2026-02-09 至
2026-08-10。结果文件
`output/native-formula-security-relation-divfactor-live-v18.json` 为 77,084 字节，
SHA-256 `0E61AB07F620F2554FB77028AAB73F8844F1768BC7515B05DC3DCF45034B253C`。

## DIVFACTOR 精确语义

反编译确认该函数不等价于完整价格复权公式。它只接受 category 1 除权记录，并
读取 29 字节记录偏移 21 的第三个 float，即“每 10 股送转数”。原生单次因子为：

```text
(bonus_transfer_per_10 + 10) / 10
```

现金分红、配股数量和配股价都不参与。`TYPE=1` 对事件日前的所有柱逐次除因子；
`TYPE=2` 从事件柱开始向后逐次乘因子；`TYPE=0` 按 `TQFLAG` 的 0/1/2 自适应；
非法类型返回全 1。中间值和累计操作均按原 DLL 保留 float32 舍入顺序。

真实 `SZ300502` 在 2026-06-11 每 10 股转 4 股：事件日前因子为
`0.714285731315613`，事件日开始恢复 1；后复权在事件日前为 1，事件日开始为
`1.39999997615814`。结果
`output/native-formula-divfactor-live-v18.json` 为 50,736 字节，SHA-256
`C724C320F47DAD2532EF48BDF3ABB1D1120CDF4FBFFE5896B0D896D5BD006E88`。

## 覆盖与验证

- TCalc 处理函数：`output/ida-tcalc-remaining-nonl2-handlers-v1.json`，SHA-256
  `4583EF117F482A08B00D2C76326EE01565F7912AE1F65B23ED98466A552D537E`；
- TdxW 宿主分派：`output/ida-tdxw-host-dispatch-v1.json`，SHA-256
  `FFCB9E5B3BEC8B0E4D5DBA09AD552A8BEFB271F4F9194030E1F3F7DA78C37FA0`；
- 覆盖报告：`output/native-formula-coverage-security-relation-divfactor-v18.json`，
  1,304,138 字节，SHA-256
  `729471ABDFE3676093A3E82A05BF02C68C9981C3D50CEAFBDE016EC1D6899615`；
- 注册表差分：`output/native-formula-registry-next-audit-v18.json`，4,361 字节，
  SHA-256 `022D9ED19FF14EBFB4352CE364537E730A8AC8A3844E570C55D3D7FF83B20FB2`；
- 候选专项 1/1，完整 API 222/222；完整报告 SHA-256
  `9413B54C4E26C580C68D6C019274BC02F42CABC4276F5343A9CE23C48FE6403F`；
- 全量 CTest 102/102。

能力清单现为 257 个支持函数、87 个自动符号；390 条静态注册名已识别 312，
剩余 78。379/379 内置公式继续语法支持且数值安全，退化数值输出为 0。

## 正式发布

正式健康、首页、公式覆盖、新公式和连板天梯 5/5 通过，报告
`output/api-contract-security-relation-divfactor-v18-formal.json` 的 SHA-256 为
`8201EB7C770B304B741100ABBFA39CB1F3D79B8984F8B1A8ACD8D1C0F165A111`。
最终 EXE 为 18,426,880 字节，SHA-256
`38D3D0B59A1CC14D91391BDE9B80614F4F17F7314714268A7A1592A98FE503B8`；正式服务
PID 24944，仅监听 `127.0.0.1:8765`，保持 `native_cpp=true`、
`python_runtime=false` 并加载 600 份 JSN 资源。

替换前 v17 已保存为
`output/tdx-tool-security-relation-divfactor-v18-predeploy-rollback-20260810.exe`，大小
18,376,820 字节，SHA-256
`CF95C92A0EBD706C49FC981791D3B3149EFCE03C9DB28078F2D8E6E30E09A042`。

下一批优先处理已有完整处理函数和帮助文本的 `ZIGA`，再核实 `RAND`、
`SAFESCORE/SHINESCORE`；高收益的 `CALCSTOCKINDEX` 需要先建立受限嵌套求值和
循环依赖保护。
