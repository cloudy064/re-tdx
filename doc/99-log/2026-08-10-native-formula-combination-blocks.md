# TCalc ZHBLOCK/ZHBLOCKNUM/SIMIBLOCK 组合与相似板块闭环

## 结果与边界

纯 C++ 公式解释器新增三个自动上下文符号：

- `ZHBLOCK`：当前证券所属组合板块名称，按目录顺序连接并保留尾随空格；
- `ZHBLOCKNUM`：命中的组合板块数量；
- `SIMIBLOCK`：当前 TdxW 构建中的相似板块文字结果。

这三项沿 TCalc 的 command-8 宿主回调恢复，不依赖登录、L2、券商账户或插件。
当前安装的 `C:\new_tdx\T0002\lc` 为空，因此正式环境对任意证券得到
`ZHBLOCK=' '`、`ZHBLOCKNUM=0`、`SIMIBLOCK=' '`。单个空格不是猜测值：宿主无
结果时先返回 `|`，TCalc 再把分隔符逐个替换为空格。

此前把 `zhb.zip` 视为组合板块候选数据源的边界不正确。本轮已确认该统计资源与
command-8 category 3 的组合板块目录无关；组合板块唯一已证明的本地来源是
`T0002/lc/lcidx.lii` 及同目录 `.cis` 成员文件。

## 原生调用链与文件格式

TCalc 静态注册表和处理函数给出如下精确映射：

- `ZHBLOCK` opcode 1056，处理函数 `sub_100448F0`，最终请求 command 8、
  category 3；
- `ZHBLOCKNUM` opcode 1061，处理函数 `sub_10006340`，使用同一 category 3
  目录并返回命中数量；
- `SIMIBLOCK` opcode 1393，处理函数 `sub_10044930`，请求 command 8、
  category 0。

TdxW 的 command-8 字符串提供者为 `sub_4FB790(category, market, code, text)`。
category 3 按组合板块目录升序扫描；初始化器 `sub_731680`、目录加载器
`sub_734DA0/sub_7337C0` 和成员加载器 `sub_731BE0` 确认文件结构如下：

1. `lcidx.lii` 每条 320 字节，最多读取 600 条；偏移 4 是最多 6 字节的 ASCII
   文件键，偏移 11 是最多 8 字节的 GBK 显示名；
2. `<键>.cis` 长度必须大于 0 且为 16 的整数倍；每条偏移 0 是 little-endian
   `uint16` 市场号，偏移 2 是最多 8 字节的 ASCII 证券代码；
3. 原宿主物化时最多使用 25,000 个成员；命中名称保持目录原始顺序，以 `|`
   分隔并保留尾随 `|`，TCalc 随后将其转换为尾随空格的文本。

损坏、截断或不存在的目录/成员文件均按未命中处理，不把任意 `.blk`、网络主题
或 `zhb.zip` 混入组合板块。纯 C++ 解析器保留 600/25,000 上限和精确记录校验，
并在响应元数据中公开目录来源、目录数、可读成员文件数与命中数。

category 0 则具有更强的版本边界：本构建的查找、计数、缓存写入和主生产分支都
只处理 category 1..5，没有任何 category 0 的提供者。因此 `SIMIBLOCK` 在当前
TdxW 版本中精确为空结果，经 TCalc 转换后为一个空格；实现没有虚构相似度算法或
用行业/概念板块代替。

核心证据及 SHA-256：

- `output/ida-tdxw-zhblock-provider-v1.json`：
  `3742846B8D0A68665F580792272749148542654618DC670075834A7ECF040D74`；
- `output/ida-tdxw-zhblock-provider-xrefs-v1.json`：
  `EFB7B0CF31D7FCE64B11171780779EFEFFD41962F331ECF5B6F3FA9FC4138A27`；
- `output/ida-tdxw-zhblock-catalog-v1.json`：
  `D7FC73272B2C6799A853E0B016EED5BE2EBBBBF1A26EB6DCF92727A864EC5213`；
- `output/ida-tdxw-zhblock-loaders-v1.json`：
  `FD759EF4C12445B6F3FBFECF7AEE6AE6ECE70B7C0DDF83E344F8150CF91E8178`；
- `output/ida-tcalc-block-metadata-v2.log`：
  `153338EE6EF089C7A0487AE809AC241F2B8C725D748503BF00EA12C1CB392C3D`；
- `output/ida-tdxw-callback-dispatch.log`：
  `3FCE22B0FA327FAC4FEB6FAAECB11F8C7E1A758296A07F95993477E9F1BC6231`；
- `C:\new_tdx\TdxW.exe`：
  `F5F2E6025A4D80BB1AFBCB2A51C753D3C1909E09AA9D1BC8F7C3B701B081F74C`。

## 实现与测试

`native/src/blocks.cpp` 新增组合板块目录和成员解析，`formula_context.cpp` 将三个
符号绑定到每根 K 线；`formula_engine.cpp` 将它们纳入外部依赖、字符串类型和
板块元数据能力清单。测试用临时 `lcidx.lii` 与两个 `.cis` 构造出两个中文组合
板块，覆盖目录顺序、多重命中、单一命中、未命中哨兵、数量、来源元数据、依赖
分析与公式文本/数值联合求值。正式空目录也通过 HTTP 契约验证。

能力结果：

- 支持函数保持 230；
- 自动符号由 69 增至 71；
- 板块元数据符号由 8 增至 11；
- 390 条静态注册名的识别并集由 282 增至 285，剩余 105；
- 379 条内置公式继续保持源码、语法、数值安全和展示可信 `379/379`，降级数值
  输出为 0。

覆盖报告 `output/native-formula-coverage-combination-blocks-v10.json` 的 SHA-256
为 `3DACEB23B234F6A7595BA69CDA2ADF9DAA7D8185E7F274BDE8F36CC8E4F97268`；
后续注册表审计 `output/native-formula-registry-next-audit-v10.json` 为
`4C2B81F4B11DDCD4613FF50D591865699E5BD93A1D2880C9FCF7E68124F7BFD5`。

验证结果：

- CTest `102/102`；
- 候选专项契约 `3/3`；
- 候选 full API `219/219`；
- 正式 8765 专项契约 `3/3`。

三份 API 报告 SHA-256 依次为
`A0A4D70A7D125D9A9E3A1AC6016F7C393C8E4DABBA41BBDD6F49770DF55D60F5`、
`DF289E7272CA288975B1C18A09E27DD961E395C4778062E43EC8DD95B8B937E9`、
`6D4E8AB61CF6A1D0DAFA590D5BE4090D32A404EB036698B6256DC0E2B96948A0`。

## 部署与下一步

正式 EXE 为 22,009,802 字节，SHA-256
`457BC6C12DA6473A4B7CF31CF2064CB51ABD089DD76AA5109E751250A5DBCD3A`。
服务 PID 684，仅监听 `127.0.0.1:8765`，健康状态保持 `native_cpp=true`、
`python_runtime=false`，stderr 为 0 字节。上一正式版保存在
`output/tdx-tool-combination-block-v10-predeploy-rollback-20260810.exe`，SHA-256
`FA2ADB4A5A248B818BC8E09722E9B4979D8FBE108CEB585EF961A6A22C75C211`；候选
8875 已关闭。

下一批普通非 L2 高价值候选为 `EXTERNVALUE/EXTERNSTR`。必须先闭合外部文件的
命名空间、记录格式、数值/字符串类型转换、缺失哨兵和刷新归属；在证据不足前不
接受任意 JSON 或自造键值替代。纯展示指令 `DRAWSL/DRAWBMP/DRAWGBK/DRAWRECTREL`
可作为其后的独立批次推进。
