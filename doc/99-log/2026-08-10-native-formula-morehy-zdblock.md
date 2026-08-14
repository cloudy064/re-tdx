# TCalc MOREHYBLOCK 行业模式与本地自定义板块闭环

## 结果

纯 C++ 公式解释器新增三个自动上下文符号：

- `MOREHYBLOCK`：按通达信当前行业模式返回普通行业或研究行业叶子名称；
- `ZDBLOCK`：返回证券所属自定义板块名称，精确保留 TCalc 的空格拼接结果；
- `ZDBLOCKNUM`：返回证券所属自定义板块数量。

真实 `SZ000001 平安银行` 在本机配置下返回
`MOREHYBLOCK=股份制银行`、`ZDBLOCK=" "`、`ZDBLOCKNUM=0`。这里单个空格不是
占位或错误：宿主没有找到成员板块时仍返回一个竖线，TCalc 再逐字节将竖线替换
为空格。实现不依赖 Python、Level2 或付费账号。

## MOREHYBLOCK 模式证据

`TCalc.dll` 的 opcode 1361 从 type-167 结构读取文字：模式字节 357 为 0 时读取
偏移 358，为 1 时读取偏移 379。继续追踪 `TdxW.exe` 的 `sub_8BB5D0` 得到：

- 宿主读取 `T0002/user.ini` 的 `[Other] UseTdxL3HY`；
- 缺省值为 2，超出无符号范围 0..2 时回落为 0；
- 配置值 2 写入 type-167 模式字节 1，选择研究行业叶子；其他有效值写入 0，
  选择普通行业叶子；
- `SZ000001` 的普通行业代码 `T1001` 映射为“银行”，研究行业代码
  `X500102` 的叶子映射为“股份制银行”。

本机 `user.ini` 未显式设置该键，所以使用缺省值 2。上下文元数据同时返回配置
值、模式字节、选择的行业族、来源代码和板块指数代码，方便网页与命令行解释
当前结果，而不是把两棵行业树混为一谈。

## ZDBLOCK/ZDBLOCKNUM 文件与字符串语义

`TdxW.exe` 的 command-8 category 2 按以下顺序构造目录：

1. 固定项“自选股”，内部键 `zxg`；
2. 固定项“临时条件股”，内部键 `tjg`；
3. `T0002/blocknew/blocknew.cfg` 的现代自定义记录；若不存在则读取旧版
   `T0002/block.cfg`。

现代目录每条 120 字节，名称位于偏移 0、最大 50 字节，键位于偏移 50、最大
50 字节；旧版每条 19 字节，前 4 字节为元数据，名称从偏移 4 起、键从偏移
14 起。成员文件统一为 `T0002/blocknew/<key>.blk`，逐行规范市场前缀和证券代码。

宿主为每个命中的目录项追加 `name|`，没有命中时返回 `|`；TCalc 的
`sub_10044720` 把每个 `|` 替换成空格且不 trim。因此网页和 API 会精确保留末尾
空格，数量则由 `ZDBLOCKNUM` 独立返回，不通过分割字符串猜测。

当前通达信目录没有 `blocknew.cfg` 或旧版 `block.cfg`，只有 `zxg.blk`、空的
`tjg.blk` 和 `zxgmore.dat`；平安银行不在 `zxg.blk`，所以结果是单个空格和 0。

## 纯 C++ 实现

`blocks.cpp` 新增只读配置与目录加载：

- 解析 `UseTdxL3HY`，复现缺省值和范围回落；
- 同时解析现代、旧版自定义板块目录并保持宿主顺序；
- 解析 `.blk` 成员、市场前缀与代码；
- 复现 ZDBLOCK 的尾随空格和空结果单空格行为。

`formula_context.cpp` 将 `MOREHYBLOCK` 接入现有普通/研究行业树，将
`ZDBLOCK/ZDBLOCKNUM` 直接绑定到本地目录，不要求加载行情板块数据。
`formula_engine.cpp` 将三项登记为自动依赖并输出来源元数据。

能力清单现为：支持函数 218，自动符号 69，type-167 文字符号 3，自定义板块
元数据符号 8。390 条静态注册表中的已识别并集由 267 增至 270，剩余 120。
完整覆盖证据为 `output/native-formula-coverage-type167-custom-block-v4.json`，下一批
差分队列为 `output/native-formula-registry-next-audit-v4.json`。二者 SHA-256 分别为
`ED5161023DE32A7CA5F97236A54CA58CBD1C2C8C8617D7EF0A2AC37360D1B0FE` 和
`32F5F4ABE47FAC5A6218A7EC45B244260E3233FA9489CAD48C97B8C648DB4770`。

## 契约与验证

固定契约同时验证五项依赖：`LEVEL1HYBLOCK`、`MAINBUSINESS`、
`MOREHYBLOCK`、`ZDBLOCK`、`ZDBLOCKNUM`。真实平安银行 120 根日线的末柱断言为
`L=1, M=1, H=1, Z=1, N=0`，并检查研究行业与自定义目录元数据。

当前批次已通过：

- 公式引擎专项测试；
- API 契约求值器专项测试；
- 生产目录增量一致性检查（`ninja: no work to do`）与 CTest 101/101；
- 临时服务完整 API 217/217；
- 将要发布的生产 EXE 在 8875 上通过健康/type-167 烟测 2/2；
- 正式 8765 上通过健康、Svelte 首页、公式渲染真实性和 type-167 实值契约 4/4。

完整临时报告为 `output/tdx-type167-v4-full-contracts.json`，专项报告为
`output/tdx-type167-v4-contracts.json`，SHA-256 分别为
`65A3C59FF52181B18CC90DD4133431E96134B4F98693A7AA055E62F5BE066ED1` 和
`3E11451AD5B326851509D9B7AB4CFAFCE0D85CF70FFD6B3C7A6A4A49CBCA6736`。
正式报告 `output/native-type167-custom-block-v4-formal-final-api.json` 的 SHA-256 为
`134F963DCC9B8A88EA2FFF7B59C9C7B41AE3B9F9818979CC25BC2473F54A1047`。

正式 EXE 为 43,001,028 字节，build/dist SHA-256 均为
`9DF1E5CD98684057383463DB6D02394FBC3D0519D07D7200E56328A95F4766A7`；服务 PID
35236，仅监听 `127.0.0.1:8765`，健康状态为 `native_cpp=true`、
`python_runtime=false`。旧正式版本保存在
`output/tdx-tool-type167-custom-block-v4-predeploy-rollback-20260810.exe`，大小
42,954,413 字节，SHA-256 为
`CA2B857A55D5F4057151D9D7BEC412F542D33207A029D3D5EE9E383B733A988F`。
临时 8875 已关闭。

## 剩余边界

- `ZHBLOCK`：已知 command-8 category 3 和 100013 字节运行时记录结构，但实际
  组合板块数据来自运行时接口，并非本地 `zhb.zip`；仍需闭合提供者与刷新生命周期。
- `SIMIBLOCK`：已知 command-8 category 0 进入专用缓存目录，但活动目录集、证券
  类别选择和显示顺序尚未闭合。
- `GNBKZSCODE/FGBKZSCODE/GETNAMEOFCODE`：适合与上述板块身份图一起继续追踪。
- `FINONE/GPJYONE/BKJYONE/SCJYONE/GPONEDAT`：作为下一层高收益的单点财务/
  交易序列入口继续处理。

Level2、账户交易状态、券商私有信号和插件回调继续排除。
