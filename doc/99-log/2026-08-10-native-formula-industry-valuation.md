# 2026-08-10 TCalc HYSYL/HYSJL 行业估值

## 结论

纯 C++ 公式解释器新增零参数函数 `HYSYL` 和 `HYSJL`，分别返回当前指数或
个股所属行业的市盈率、市净率（MRQ），并把单个结果按 float32 兼容值广播到
整条公式序列。两项都使用公开 HYZT 数据，不需要 Level2，也没有调用 Python。

| 函数 | opcode | TCalc 处理函数 | 宿主请求 | 宿主偏移 | 公开字段 |
|---|---:|---|---:|---:|---|
| `HYSYL` | 1328 | `TCalc.dll!sub_10016DF0` | type 120 | 60 | `hyPE` |
| `HYSJL` | 1344 | `TCalc.dll!sub_10016F90` | type 163 | 380 | `hyPB` |

原始帮助分别为“返回指数市盈率或个股所属行业的市盈率”和“返回指数市净率或
个股所属行业的市净率”。注册、处理函数、帮助字符串和 TdxW callback 四层证据
已经闭合，不再根据函数名称猜测行为。

## 原生处理链

两个处理函数都拒绝参数并请求宿主的当前行业分配。当前证券本身若是沪市
`880xxx/881xxx` 行业指数，则保留当前代码；其他证券由宿主返回的行业编号生成
`880%03d` 普通行业或 `881%03d` 研究行业代码。`HYSYL` 随后读取 type 120
返回结构偏移 60，`HYSJL` 读取 type 163 返回结构偏移 380；对应 TdxW 宿主字段
分别是 `$PE` 和 `$PBMRQ`。最终值按原生单精度数值广播。

TdxW 在 `sub_596EE0` 中把 `sub_61B630` 注册为 TCalc callback。相关处理体、
字段偏移和行业代码选择逻辑已保存为静态审计产物。

## 公开 HYZT 来源与边界

已扫描本地 600 份 JSN 资源，只有
`output/tdx-jsn/list/func_gx_hyzt101_1.jsn` 同时包含 `hyPE/hyPB`。该资源由
纯 C++ 下载器从 `110.41.147.114:7709` 刷新，当前 MD5 为
`b36c260ada2e8377f2bc8e00d1d7b8c5`，大小 8,904,400 字节，SHA-256 为
`DFC040F95DB5C5B52C2D104B6C8C5D1C8BCD60E8A008F460FCEB65A7B5F96054`。
解析后共有 5,539 条证券记录和 110 个普通叶子行业。

公开资源没有第二份 881 研究行业估值表，因此实现保留以下显式边界：

- 普通 `880xxx` 叶子行业直接使用 `hyPE/hyPB`；
- 个股配置为 `881xxx` 且其公开估值不存在时，回退到该股同时具备的普通
  `880xxx` 行业，并在元数据中同时给出 `configured_selected_code`、
  `selected_code`、`normal_industry_fallback=true` 和
  `native_host_family_exact=false`；
- 直接查询 `881xxx` 行业指数或宽基指数时不应用个股回退，返回 0 和明确的
  来源缺失元数据，不构造或外推估值。

纯 C++ HYZT 目录解析器会校验数值和同一行业记录的一致性，并按资源绝对路径、
文件大小和最后修改时间缓存；原子刷新后无需重启即可观察新资源。

## 真实样本

候选服务对 `SZ000001` 的 20 根真实日线求值结果为：

- 配置研究行业：`881388 股份制银行`；
- 有公开估值的有效行业：`880471 银行`；
- 成员数量：42；
- `HYSYL=5.2104997634887695`；
- `HYSJL=0.5302000045776367`。

首柱、末柱值一致，证明标量广播稳定；响应同时保留两项 opcode、宿主类型、偏移、
公开字段和回退原因，调用方可以区分精确宿主行业族与公开替代行业族。

## 验证与产物

- `output/ida-tcalc-hysyl-hysjl-handlers-v1.json`，SHA-256
  `6571F2DCDD8C9F50817F2255AB02491773EFCCF4542B1AB77281F49F1478940A`；
- `output/ida-tcalc-hysyl-hysjl-help-v1.json`，SHA-256
  `FE956A94744ABD3E86AF241291C5E3E7C9DDE15A14BF13411CF9F097C240A10E`；
- `output/ida-tdxw-tcalc-host-callback-v1.json`，SHA-256
  `6B104E9F40548ED9C5E65CACFDF5772F0F735549457AE29BE158BCBCC029C235`；
- `output/native-formula-industry-valuation-live-v1.json`，SHA-256
  `1545BA32952AEDB3A486C18620049035C14D3488C93725C3A74E326BCF1C891E`；
- `output/native-formula-coverage-industry-valuation-v16.json`，SHA-256
  `590AB5D2D2480BF9E03C04A8F7526379E486C94BC6B4055CDC67A862DF2DC68B`；
- `output/native-formula-registry-next-audit-v16.json`，SHA-256
  `78CD776C5B0E2A7491788643608C27F1795A0F821C52012713B4B081F8811F07`。

覆盖现为 379/379，支持函数 246、自动符号 77；390 条静态注册名已识别 301，
剩余 89。全量 CTest 为 102/102；候选服务专项契约 4/4、完整 API 契约 220/220，
对应报告 SHA-256 分别为
`C8CE01848ABECC984C71DD38DA4047B9A47280E754EAB8EB1C7F118BD6EEF5C6` 和
`7EF018BD5C188CC2C423C87B4DD8B215067879534405252B071561330E7A2BAE`。

正式端口又通过健康、首页、宿主汇总公式、公式覆盖和连板天梯五项契约，报告
`output/api-contract-hysyl-hysjl-v16-formal.json` 的 SHA-256 为
`EB4FB42EBEC835043C872FBDAAAD03AD6821080D0F88065C8D172E1934B136BF`。
最终发布 EXE 为 18,333,696 字节，SHA-256
`FA5C5356438325A1A0D7D12F707B8D6814F1AF2EFA2F37E8E6CFE97249816B91`；正式服务
PID 41156，仅监听 `127.0.0.1:8765`，健康接口为 `native_cpp=true`、
`python_runtime=false`，并显式使用 600 份 JSN 资源。替换前 v15 保存为
`output/tdx-tool-industry-valuation-v16-predeploy-rollback-20260810.exe`，大小
19,117,568 字节，SHA-256
`5BE3BE732B37467E3753BA4FF878FF8610F601C616B3CA9540619303D60EE1B7`。

下一批不预先指定函数名，先按处理函数、帮助和可获取数据源重新排序剩余 89 条
注册名，再选择非 L2 高收益项。
