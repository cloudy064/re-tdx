# TCalc 证券状态宿主字段闭环

日期：2026-08-09

## 结论

`IST0CODE`、`ISSTCODE`、`ISQUITCODE`、`ISQHQQCODE` 已从“只知道 TCalc
读取哪个宿主字节”推进到字段来源、装载规则、纯 C++ 自动上下文和固定 API
契约全部闭合。实现不按函数名称、市场号或网页标签猜测，也不需要 Level2、登录
票据或原 DLL 运行时。

四项宿主链如下：

| 公式项 | TCalc 读取 | TdxW 回调 | 精确来源与规则 |
|---|---:|---:|---|
| `IST0CODE` | 目标字节 75/76 的并集 | type 167 | 深市 `12x` 且第三位不小于 3；沪市 `11x` 且第三位不是 2/4/5；北证命中 `spblock.dat` 的 `#北证可转债`；再与 `#T+0基金` 并集 |
| `ISSTCODE` | 目标字节 174 | type 120 | 仅 A/B/科创/北证股票类别适用，证券名称包含区分大小写的 ASCII `ST`；因此 `*ST` 也命中 |
| `ISQUITCODE` | 目标字节 175 | type 120 | `T0002/hq_cache/infoharbor_spec.cfg` 的市场、代码、状态和第四字段启用日期；状态为 0 且日期为空/为 0/已经到达时置位，第五字段不参与该标记 |
| `ISQHQQCODE` | 目标字节 40/48 的并集 | type 105 | 证券记录类别 3（期货）或 12（期权）；A 股读取 360 字节 TNF 记录偏移 282，扩展市场读取 7727 `0x23F0/0x23F5` 品种目录的同一类别字段 |

关键静态证据保存在：

- `output/ida-probe-tdxw-aux-record-users.log`：`sub_5A4E70` 对
  `infoharbor_spec.cfg` 的实际解析、日期门槛和辅助记录 `+20` 写入；
- `output/ida-tcalc-string-builders-status.log`：四个 TCalc 处理函数读取的目标字节；
- `output/ida-probe-tdxw-security-category-282.log`：TdxW 主证券记录类别字段的消费方；
- `output/expansion-first-1000.json`：真实 7727 目录中类别 3 合约与类别 5
  标的指数的同页反例。

## 纯 C++ 实现

- 新增 `native/include/tdx/security_status.hpp` 和
  `native/src/security_status.cpp`，负责退出状态文件缓存、证券类别判定及四项原生
  布尔规则；
- `FinanceEligibilityResource` 继续复用同一个 `spblock.dat` 缓存，新增
  `T+0基金` 和 `北证可转债` 两个命名区段；
- `Security` 保留 TNF 偏移 282 的原始类别，不把它压成 A 股市场类型；
- 扩展市场按市场和代码精确分页查找单条品种记录并缓存类别，不下载后再按市场号
  推测；
- 公式静态分析把四项同时识别为 TCalc 注册函数和无括号宿主符号，自动上下文会
  填入 `symbols`，响应 `context_metadata` 明确返回数值、来源文件、类别和重建模式；
- 单次执行、公式审计、扫描、回测及扩展市场过滤共用同一依赖分类。

能力清单由 195 个支持函数、51 个自动符号提升到 199/51；
`custom_formula_security_string_functions` 由 14 项增至 18 项。新增
`formula-security-status-inline-post` 后完整固定契约由 209 项增至 210 项。

## 验证

协议与单元测试：

- CTest：101/101；
- 正式证券状态专项：1/1；
- 正式 full API：210/210，报告为
  `output/api-contracts-20260809-security-status.json`。

真实行情/目录对照：

- `sz000010 *ST美丽`：`ISSTCODE=1`；
- `sz159001 货币ETF易方达`：`IST0CODE=1`；
- `47:IFL9`：目录类别 3，`ISQHQQCODE=1`；
- `47:IF300`：同市场目录类别 5，`ISQHQQCODE=0`，证明没有按市场整体误判；
- `sz000001 平安银行`：四项均为 0，同时返回
  `spblock.dat + infoharbor_spec.cfg + TNF offset 282` 来源元数据。
- 原始北交所市场号 `44:920023` 会归一到本地北证主档，`ISSTCODE=1`。

正式发行 EXE SHA-256 为
`897D064B5BD28882933E2F79FE1105BA218D60B797D63950F19D6C004E8269C1`。
服务 PID 为 23232，仅监听 `127.0.0.1:8765`；运行时仍为纯 C++，网页资源沿用
`index-luExy9WX.js` 与 `index-BBbdHCcI.css`。

## 边界

`ISQUITCODE` 复现的是 TdxW 当前日期装载后的状态，不应倒推出任意历史日期的
退市状态；调用方若需要历史回测，应显式保存当日 `infoharbor_spec.cfg` 快照。
扩展市场类别依赖公开 7727 品种目录，网络失败时明确报错，不使用代码前缀或市场
ID 静默回退。四项均不是 Level2 能力，也没有扩大任何授权边界。
