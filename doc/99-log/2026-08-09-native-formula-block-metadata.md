# TCalc 本地板块元数据与静态注册表复核

## 目标

继续从 TCalc 静态注册入口中选择不依赖 L2、交易账户或券商私有会话的高收益
能力。本轮选择板块归属簇，使自定义公式能直接判断当前证券属于哪个本地板块，
并取得概念、风格、指数数量及行业指数代码。

## 注册表证据修正

`output/ida_probe_tcalc_function_registry.py` 原先把 IDA 的 `word_*` 源操作数误按
一字节读取。名称尾字节因此可能缺失，只有一至两个字符的合法名称又被旧的标识符
过滤条件排除。修正操作数宽度、子寄存器赋值和寄存器相对目标寻址后，重新对
`TCalc.dll!sub_100C6870` 运行逐记录 JSONL 探针，得到：

- 记录数：390；
- 唯一名称数：390；
- 记录尺寸：71 字节（0x47）；
- 新确认的短名称：`TR/MA/IF/LN`；
- 权威清单：`output/ida-tcalc-function-registry-jsonl-v4.log`。

因此此前文档和 coverage 中的 386 已统一更正为 390。这个数字仍只代表静态注册
入口，不代表 390 项均已实现或均不需要外部上下文。

## 处理函数

新增 `output/ida_probe_tcalc_block_metadata.py`，对目标处理函数、反汇编和一级调用
函数统一取证，日志为 `output/ida-tcalc-block-metadata.log`。关键结果如下：

- `INBLOCK`：`sub_10005EC0`，opcode 1048；把末柱字符串句柄还原为块名，经
  command 8 查询一次，再将 0/1 广播到全序列；
- `GNBLOCKNUM/FGBLOCKNUM/ZSBLOCKNUM`：分别由 `sub_100062E0/10006300/
  10006320` 包装共享 `sub_100060C0`，command-8 类别为 1/4/5，返回值来自
  响应偏移 1 的有符号 16 位计数；
- `FGBLOCK/ZSBLOCK`：`sub_100448B0/100448D0` 包装共享 `sub_10044720`，
  使用相同类别取回以 `|` 分隔的名称并替换为空格；
- `HYZSCODE`：`sub_10044DF0` 从 type-120 行业归属记录构造 880/881 行业指数
  代码，并返回字符串池句柄。

`ZHBLOCK/ZDBLOCK` 及其数量依赖原客户端组合板块、自定义板块和用户配置，本轮
没有可证明完整的本地来源，因此没有用空字符串或固定零冒充实现。

## 纯 C++ 实现

运行时没有加载 TCalc.dll，也没有 Python 转发：

- `formula_context.cpp` 只在公式实际依赖这批入口时加载
  `infoharbor_block.dat/tdxzs3.cfg/tdxhy.cfg`；
- `FGBLOCK/ZSBLOCK` 物化为字符串上下文，三个 `*BLOCKNUM` 物化为常量数值；
- `INBLOCK` 使用完整块名集合，包含行业层级的直接和继承归属，做精确名称匹配；
- `HYZSCODE` 复用既有 type-120 行业层级选择；
- `INBLOCK` 的内部匹配集合放在私有字符串上下文，不回显到 API；
- coverage 新增一项板块函数、六项板块元数据符号，当前为 200 个支持函数、
  54 个自动符号和 390 条唯一静态注册证据。

## 真实样本

使用 `C:\new_tdx` 当前本地文件和平安银行 `sz:000001` 的 20 根真实日线验证：

- `INBLOCK('银行')=1`；
- `INBLOCK('跨境支付')=1`；
- `INBLOCK('证券')=0`；
- `GNBLOCKNUM+FGBLOCKNUM+ZSBLOCKNUM=29`；
- `HYZSCODE='880471'`；
- 风格和指数名称列表均非空并完整进入绘图字符串环境。

固定 API 契约 `formula-block-metadata-inline-post` 使用 120 根平安银行日线，锁定
分析分层、自动上下文依赖、正反 `INBLOCK`、数量和文本/行业代码。

## 验证与发布

全量 CTest 为 101/101；新二进制在临时端口以 `C:\new_tdx` 真实数据运行时，
full API 契约为 211/211（212 次网络请求），其中新增
`formula-block-metadata-inline-post` 已通过。补齐该案例的命令帮助后重新链接，
`tdx-recon-contract-tests` 再次通过；正式服务上的健康、coverage 与新增板块公式
专项为 3/3。

正式 EXE SHA-256 为
`9EF0A4CC2F7E3BF874D08455BDF8AC2CA3147DE6A29F9E0CABCC5DB479B1C31E`；服务
PID 14768，仅监听 `127.0.0.1:8765`。在线健康检查为 `native_cpp=true`、
`python_runtime=false`，coverage 报告 200 个支持函数、54 个自动符号、390 条
唯一静态注册证据，并公开板块能力分组。网页资源没有变化。
