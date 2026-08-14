# 交易所监管观察期纯 C++ 固定化

## 结论

客户端 `cloud_dax/JYJGFX.sp` 的“交易监管风险”并不是泛化的监管公告页。它由
`func_jysjk101`“当前监管个股”和 `func_jysjk102`“历史监管记录”组成，并同时
联动分时、K 线和异动监控。现已固定为纯 C++：

- CLI：`tdx-tool market exchange-supervision`
- API：`/api/v1/market/exchange-supervision`
- 视图：`current | history | security | catalog`
- Svelte：数据中心“交易监管”，个股“关注与事件”中的监管观察期面板

## 客户端证据与字段

`func_jysjk101.cfg` 声明监管开始/结束日、`price1`、异动公告和 PE(TTM)，并用
`($NOW-price1)/price1*100` 计算当前表现；`func_jysjk102.cfg` 额外返回
`price2`，用 `(price2-price1)/price1*100` 计算历史监管期间表现。现价、成交额、
市值和行业是宿主列，不能伪造成 JSN 原始字段。

2026-08-07 重新下载的资源：

| 资源 | MD5 | 行数 | 日期范围 | PDF |
| --- | --- | ---: | --- | ---: |
| `list/func_jysjk101_1.jsn` | `fabc2c1b9cdb2e9e7f29de6d577e5db5` | 22 | 监管开始 2026-07-24—08-06；结束 08-06—08-19 | 13 |
| `list/func_jysjk102_1.jsn` | `f7588745cf0c0ecc4410edcbcda3b9d3` | 85 | 监管开始 2026-04-21—07-22；结束 05-07—08-04 | 71 |

两表证券集合当前无交集。市场字段覆盖深、沪、北，证券既有股票，也有 ETF、
LOF 和转债，所以服务保留通用证券身份，不做 A 股专属假设。

## 功能边界

- `market exchange-supervision`：交易所给证券设置的当前/历史监管观察期。
- `market anomaly-risk`：异常波动偏离统计、可能停牌/复牌及触发阈值。
- `market research --view regulatory`：问询、监管措施、处分对象与市场禁入。

三者来源、时间粒度和法律/风险含义不同，未合并为一个含混字段。

当前视图用一次公开 `0x054C` 批量快照连接全部 22 个证券，22/22 成功；接口
输出起始价、当前价、当日涨跌、成交额及相对起始价收益。行情失败只会降级为
`records-only`，不会隐藏监管记录。历史视图只使用资源中的起止价，不混入当前
行情。公告链接可以为空，也可以直接打开深交所、上交所/信息披露站或北交所 PDF。

## 验证与部署

- 新增收益公式、日期、报价缺失与排序单元测试。
- CTest：60/60。
- Svelte：`check` 0 错误、0 警告；生产构建成功。
- 新增在线契约：2/2；正式 full 契约：63/63。
- JSN 覆盖：268/616 类型化，348 个通用独占；339 个下载文件，解析错误 0。
- 网页视觉冒烟：`output/probes/exchange-supervision-current/ui.png`。
- 正式服务：`http://127.0.0.1:8765`，PID 26936，功能数 105。
- 发布 EXE SHA-256：`B3F9523B4CC7A7A3E861F9C6E489163835D53FF43EBFA60421B8786AF682FC6E`。

主要证据位于 `output/probes/exchange-supervision-current/`。下一批优先审计
`hylhb101/102/104` 后续已确认是活跃龙虎榜而非行业榜，并已固定化；下一批评估
`gqgg101..104` 是否只是现有板块树的重复分组。
