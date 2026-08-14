# TCalc BLOCKSETNUM 板块成分数量闭环

## 结果与边界

纯 C++ 公式解释器新增 `BLOCKSETNUM('板块名')`。它返回指定板块的成分数量，并把
结果广播到公式全部柱。参数必须是静态字符串；动态板块名继续明确拒绝，不在运行时
猜测字符串池内容。

本地通达信数据的真实结果为：

- `BLOCKSETNUM('HY.银行')=42`；当前 `UseTdxL3HY` 模式为 2，解析到研究行业
  `881385/银行`；
- `BLOCKSETNUM('GN.跨境支付')=75`，解析到概念板块 `880609/跨境支付`；
- `BLOCKSETNUM('不存在')=0`，并在元数据中明确标记 `found=false`。

该路径不需要账号、Level2 或券商私有状态。运行时只使用 C++ 和本地通达信板块
目录；本批 IDA Python 脚本仍只用于离线证据提取。

## 原生调用链

静态注册表把 `BLOCKSETNUM` 映射到 opcode 1244 和
`TCalc.dll!sub_1000F3E0`。处理函数接受一个字符串参数，在末柱解析字符串池句柄，
清零输出序列并发送 command 8：

- 请求缓冲区 89 字节；板块名写入偏移 33，最大 50 字节；
- 偏移 86 写入 2；偏移 87 的 word 写入 5；
- 宿主返回区先清零，最终从偏移 1004 的 DWORD 读取数量并广播。

TdxW 的 command-8 分派先由 `sub_4DC610` 解析板块目录，再由 `sub_5A96E0`
枚举成员。当请求类型为 5 时，宿主释放临时成员列表，只返回数量。解析顺序为：

1. `HY.` 强制当前行业体系，`GN.` 强制概念，`MY.` 强制命名自定义板块；
2. 无前缀时依次匹配当前行业、概念、风格、指数和自定义目录；
3. 未命中保持宿主预清零结果 0。

离线证据及 SHA-256：

- `output/ida-tcalc-horizontal-aggregates-handlers.json`：
  `71D90386347AB26425A65165F7C9717704549B437E32A7BF43F34E4DD5C771C2`；
- `output/ida-tcalc-horizontal-aggregates-handlers-headless.log`：
  `6613AF66B0F0B6EDF29E56046A7B3EAD3A861A191FA588ED2DA0D149DF70FE8D`；
- `output/ida-tdxw-blocksetnum-helpers.json`：
  `9784230128518675B19D13E78CAEADFF77B4EB550F86709DEEC0538ED4E83042`；
- `output/ida-tdxw-blocksetnum-helpers-headless.log`：
  `FF7DBCBBA9F9E030E6240789D2D9600B2B18FB10E33A8DA35B2EAFECCEA76949`。

## 纯 C++ 实现

`blocks.cpp` 新增自定义板块目录计数，保留 command-8 的固定 `zxg/tjg`、配置文件
和旧版 `.blk` 顺序。`formula_engine.cpp` 收集
`BLOCKSETNUM#<静态板块名>` 标量绑定，求值时读取精确绑定并广播；该函数的字符串
参数不再被语义审计误标成数值降级。

`formula_context.cpp` 复用服务启动时建立的行业、概念、风格、指数数据，并按
`T0002/user.ini` 选择普通或研究行业。响应的 `context_metadata` 公开：

- `blocksetnum_mode`、`blocksetnum_industry_mode` 和绑定数量；
- 每个请求名对应的查找名、family、板块代码、来源、成员数和是否命中；
- 自定义板块目录来源及目录项数量。

固定契约 `formula-block-code-name-inline-post` 已扩展为同时核对 type-167 板块代码、
type-120 名称和 opcode-1244 成分数量，避免只在单元测试里验证内部绑定。

## 覆盖率与下一步

能力清单由 226 增至 227 个支持函数，自动符号仍为 69；390 条静态注册名称中
识别并集为 279，剩余 111。379 条内置公式继续保持源码、语法和数值安全
`379/379`，降级数值输出为 0。

下一组高价值候选是 `HORCALC/INSORT/INSUM`，处理函数分别为
`sub_10033710/sub_10041E80/sub_100431A0`。它们依赖目标证券集合、跨证券历史对齐、
活动公式目录和宿主行情回调缓存，必须联合闭合，当前不会用单票数据或静态板块计数
制造近似结果。`ZHBLOCK/SIMIBLOCK` 继续因运行时提供者未闭合而延后；展示函数
仍排在数值横截面能力之后。

## 验证、产物与部署

验证结果：

- CTest `101/101`；
- 候选服务健康、覆盖率和板块代码/数量真实契约 `3/3`；
- 正式 8765 同三项 `3/3`；
- 正式服务 `native_cpp=true`、`python_runtime=false`。

本批产物及 SHA-256：

- `output/native-formula-coverage-blocksetnum-v7.json`：
  `5A48E71EB74117F780582334C1A4EFFAC25FE20E2ACC485BD54F61920F8A9157`；
- `output/native-formula-registry-next-audit-v7.json`：
  `A24536160EFAEF6F8D34C6E06675CBA2C0D5FC6388609071B9E706F577249775`；
- `output/native-blocksetnum-v7-candidate-api.json`：
  `37D913191C1862C4E7975893DD6E31E7CDB1FC94435A12419DB2DFE8A7FAA37F`；
- `output/native-blocksetnum-v7-formal-final-api.json`：
  `E0D660594540822537228AAF9CB9689273BF92B71525B3F00EF8DBBE6FD2997A`。

正式 EXE 为 43,403,234 字节，build/dist SHA-256 均为
`DCA124143C44231D6AF057BFD2690D87C64E86CA048EBDB3BEE85FCD032E942C`。
服务 PID 21960，仅监听 `127.0.0.1:8765`。旧正式版已保存为
`output/tdx-tool-blocksetnum-v7-predeploy-rollback-20260810.exe`，大小
43,354,706 字节，SHA-256
`29F988A7F3C7FB7CA9181F2BCB42C118FC7FB83A1CFC4BE956D1DFF898FC81B3`。
临时 8875 已关闭。
