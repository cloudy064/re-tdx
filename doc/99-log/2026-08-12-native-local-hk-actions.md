# 本地加密港股公司行动与复权因子闭环

## 目标

补齐 `T0002/hq_cache/hkqxinfo.dat` 与 `hkqxinfo2.dat`，把客户端长期港股
分红、送股、供股、拆细/合股历史和原生复权因子提升为纯 C++ CLI/API；不修改
现有近期公开资料 `market hk-events` 的 schema，也不引入 Python/OpenSSL 运行时。

## 原生证据

- `TdxW!sub_51F310` 依次装载两份文件，构造 0x1048 字节 Blowfish 上下文，
  固定密钥为 `SECURE20090531_TDXSS`；
- `sub_42C270/sub_42CB30/sub_42CC90` 分别完成 4,168 字节 P/S 表初始化、
  标准密钥扩展和按 8 字节块解密。宿主按两个小端 32 位字消费块，文件尾不足
  8 字节的部分保持原样；
- `sub_51EA60` 解出固定五列：代码、日期、说明、累计乘数、累计偏移，并写入
  85 字节记录；消费者读取记录 `+73/+77` 的两个浮点因子；
- `sub_5135E0` 闭合了相邻事件的两种价格变换。设前一累计因子为
  `(m0,b0)`、当前为 `(m1,b1)`：
  - 除权日前价格：`p*m0/m1 - (b1-b0)/m1`；
  - 除权日及以后反向价格：`p*m1/m0 + (b1-b0)/m0`。

因此接口安全暴露原始累计因子，并派生单次事件
`event_share_multiplier=m1/m0` 与
`event_additive_adjustment=(b1-b0)/m0`。后者允许为负，供股样本不能按普通
现金分红约束拒绝。累计乘数也可能低至 `0.0000144`，解析层只要求严格大于零。

IDA 证据保存在：

- `output/ida-hk-local-loaders-xrefs-20260812.json`；
- `output/ida-hk-blowfish-core-xrefs-20260812.json`；
- `output/ida-hk-blowfish-constants-20260812.json`；
- `output/ida-hk-rights-record-offsets-20260812.json`；
- `output/ida-hk-rights-consumers-20260812.json`。

## 实现

新增 `market hk-actions` 与 `/api/v1/market/hk-actions`，schema 为
`tdx-market-hk-actions-native-v1`。实现按职责拆为：

- `common/blowfish_catalog.cpp`：原生 P/S 常量目录；
- `common/blowfish.cpp`：通用纯 C++ 密钥扩展与 ECB 块处理；
- `hk_actions_blowfish.cpp`：港股固定密钥、小端字及不足整块尾部兼容包装；
- `hk_actions_parse.cpp`：安全边界、GB18030 解码、五列解析、去重、跨文件累计
  因子衔接及按文件时间戳失效的进程内缓存；
- `hk_actions_query.cpp`：事件分类、日期/代码/文本筛选、分页、汇总和原生公式证据；
- `hk_actions_command.cpp`：CLI 编排；
- `hk_actions_adjustment.cpp`：复用相邻累计因子，按原生仿射公式生成港股
  qfq/hfq/fixed_qfq/fixed_hfq，并构造公式解释器所需的 `DIVFACTOR` 向量；
- `hk_actions_tests.cpp`：固定密文、跨文件因子、负供股偏移、分类、分页和拒绝契约。

事件保留多标签，`kind=dividend/bonus/rights/split/consolidation` 会同时命中
`mixed` 记录。分类兼容“8供1”“每5股合1股”“送1股”等省略“供股/合股/送股”
的客户端文本，不改变原始说明。

## 真实数据

当前安装两份文件合计 31,183 条、2,380 只港股，覆盖
`19730713..20260731`：

| 文件 | 行数 |
| --- | ---: |
| `hkqxinfo2.dat` | 14,740 |
| `hkqxinfo.dat` | 16,443 |

标签计数为分红 27,017、送股/以股代息 1,930、供股 1,136、拆细 325、
合股 1,092；多标签主分类 1,789 条。`00001` 返回 50 条，覆盖
`20011009..20250915`，首条“中期股息38港仙”派生加性调整为 `0.38`。

真实输出：

- `output/hk-actions-real-summary-20260812.json`；
- `output/hk-actions-real-00001-20260812.json`。

## K 线与公式解释器复用

港股扩展市场 `31/48` 的 K 线现在通过同一事件步进器完成复权。与 A 股只乘
比例因子的实现不同，港股保留 `scale + offset` 仿射变换，只改写 OHLC，并在
响应中同时返回 `adjustment_scale`、`adjustment_offset` 和兼容字段
`adjustment_factor`。固定复权模式把每根柱映射到显式锚点的累计因子基准。

`00001` 的 700 根真实日线覆盖 `20231004..20260811`，窗口内四次现金分红的
累计加性调整为 `4.687004`：原始首根收盘 `40.300003` 经 qfq 变为
`35.612999`，原始末根收盘 `72.600006` 经 hfq 变为 `77.287010`；两条响应
均报告 `tdx-hk-native-affine-v1` 和四个应用事件。

公式解释器的港股 `DIVFACTOR` 分支只读取单次股份乘数，仍然不把现金分红或
供股价格偏移混进送转因子。真实拆细样本 `31:02650` 在 `20260220` 每股拆为
五股：事件日前 `DIVFACTOR(1)=0.2`、`DIVFACTOR(2)=1`，事件日及以后为
`1/5`。HTTP 内联源码 `F:DIVFACTOR(1);B:DIVFACTOR(2);` 由
`tdx-source-interpreter-v1` 执行，自动上下文报告一个事件日和一个匹配交易日。

## 验证

- 增量构建 `tdx-hk-actions-tests` 与 `tdx-tool` 通过；
- 固定加密夹具专项测试通过；
- `serve --self-test` 通过，功能数为 153；
- 临时 18766 的五个聚焦 API 契约通过：三项成功查询、一项 400 参数拒绝和
  功能目录发现。冷查询 148.5 ms；缓存后全表供股筛选 30.7 ms、单票分页
  3.1 ms；
- 临时 18767 的港股 K 线/解释器契约通过：`00001` qfq、hfq，现金分红不进入
  `DIVFACTOR`，`02650` 五股拆细的前后向量，以及非法扩展市场 400 拒绝；
- 正式 8765 未操作；按增量验证预算没有运行完整 CTest 或全 API 契约。

聚焦契约证据：

- `output/hk-actions-api-contracts-20260812.json`；
- `output/hk-kline-divfactor-api-contracts-20260812.json`。

## 后续

港股 K 线与 `DIVFACTOR` 复用已经闭合。后续若继续处理 `hkcwdata` 等港股本地
财务文件，应保持“原始记录解析、证券/日期查询、公式上下文投影”三层边界，不能
把累计复权偏移冒充单次现金分红，也不能让港股扩展市场回落到 A 股 `0x000F`。
