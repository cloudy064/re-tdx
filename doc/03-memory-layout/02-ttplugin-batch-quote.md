# TTPlugin 批量行情与 712 字节状态记录

> 本页记录 `QHPlugins/TTPlugin.dll` 的期货/扩展市场行情链。它与
> `tpbus.dll` 的 `FastHQ.Subscribe`、普通 7709 A 股 L1 不是同一协议。

## 模块边界

当前安装的 `TTPlugin.dll` 版本为 `13.2025.0224.1125`，SHA-256 为：

```text
8B549CEC549EAD6B3B5D07F71C6DE47AB24A0B7BF76D7189FD663344B564FBE2
```

插件创建 `HQDataService`，并通过 `CTAJob_Redirect` 桥接任务。新的交叉引用和
精确反编译已纠正早期含义：`RedirectData` 是结构化请求 JSON 到旧协议定长体的
翻译入口，`Target` 才是重定向任务的目标；它不是服务器地址 JSON。DLL 内另有
支持 `tcp/tcp6/ssl/http/socks4/socks5` 的 URL/代理解析器，但当前静态证据不能
把该解析器的输入等同于 `RedirectData`。插件目录同时包含 CTP 行情/交易前置
配置，因此在没有合法运行时连接信息前，不把本链声明为匿名公共行情入口。

## `RedirectData` 翻译契约

`HQDataService.SetOption("RedirectData", req, json)` 先以 DNameNode 解析 JSON；
当数值参数 `req` 为零时再读取 JSON 的 `req` 键。`sub_102979D0` 只翻译以下
六个请求号，其他编号明确返回失败：

| 请求号 | 结构符号 | 定长 | 结构化输入字段 |
|---:|---|---:|---|
| `4611` | `mp_f10cfg_req` | 14 | `setcode, code, reserved` |
| `4612` | `mp_f10txt_req` | 104 | `setcode, code, sFilePath, nOffset, nLength, whichjbm, reserved` |
| `4618` | `mp_infotitle_req` | 74 | `search_type, from_order, wantnum, setcode, code, fl_str, type_id` |
| `4630` | `mp_infotitle_req` | 74 | 同 4618 的另一功能号 |
| `4631` | `MP_FILE_REQ` | 114 | `flag, pos, wantlen, filename` |
| `4632` | `MP_XMLBLOCK_REQ` | 49 | `setcode, code, blocktype, blockstyle, blockid` |

所有请求体都在 `+0` 写入 `u16 req`，数值使用小端；字符数组使用 GBK、NUL
结尾和固定宽度，未覆盖尾部保持零。请求随后写入 `CTAJob_Redirect` 的
`Target/ReqNo/Body`。响应由 `sub_102983A0` 翻译成结构化对象，再包装为
`CTAJob_InetTQL`，其中 `Name=Local:HQDataService`、`Body` 为 JSON。

这条路径与 9221 批量行情构造点不是同一个入口；因此 9221 不应被塞进
`RedirectData` 翻译器。纯 C++ 命令可输出完整偏移与响应字段，也可严格编码
一份调用方 JSON：

```powershell
tdx-tool recon ttplugin-redirect `
  --request request.json `
  --binary-output output\ttplugin-request.bin `
  --output output\tdx-ttplugin-redirect-contract.json
```

命令不加载原 DLL、不建立连接，也不读取登录或会话票据。

## 9221 批量请求

`sub_101AAD80` 每批最多放入 100 个证券。请求总长为 `14 + 10*N`：

| 偏移 | 长度 | 当前含义 |
|---:|---:|---|
| `0` | 1 | 低四位固定为 `1` |
| `3` | 2 | 固定 `41` |
| `6` | 2 | `4 + 10*N` |
| `8` | 2 | 同上 |
| `10` | 2 | 请求号 `9221` |
| `12` | 2 | 证券数 `N` |
| `14` | `10*N` | `u8 market + char code[9]`，代码零填充 |

`9211` 只在接收分支出现，当前未发现对应的主动构造点；不能仅凭相邻编号
把它命名为登录或首次订阅。

## 16 字节响应帧

`sub_100BD470` 先收满 16 字节，再按头中的长度收体：

| 偏移 | 长度 | 当前含义 |
|---:|---:|---|
| `0` | 4 | 未命名控制值 |
| `4` | 4 | 标志；`bit 0x10` 表示压缩 |
| `8` | 4 | 高 16 位是响应请求号 |
| `12` | 2 | 线上体长度 |
| `14` | 2 | 解码后体长度 |
| `16` | 变长 | 响应体 |

压缩分支调用 zlib 解压；非压缩分支直接使用体。`9211/9221` 随后进入同一
批量拆分器 `sub_100BD650`。

## 批量体

体首个 `u16` 同时编码记录数和扩展格式：

- `count = header % 1000`；
- `header >= 4000` 表示新版扩展格式；
- 每批上限 100。

每条基础记录为 `10 字节证券键 + 150 字节行情体`。新版再跟 4 字节扩展
值；旧版仅当基础体 `+146` 的 `u32` 非零时，再跟 140 字节扩展体。

基础体中已由插件日志逐项确认的字段为：

| 基础体偏移 | 类型 | 字段 |
|---:|---|---|
| `4` | `float` | 昨收 `Close` |
| `20` | `float` | 最新价 `Now` |
| `32` | `u32` | 累计成交量 `Volume` |
| `36` | `u32` | 最新成交量 `NowVol` |
| `56` | `u32` | 持仓量 `VolInStock` |
| `60..76` | `float[5]` | 买一至买五价格 |
| `80..96` | `u32[5]` | 买一至买五数量 |
| `100..116` | `float[5]` | 卖一至卖五价格 |
| `120..136` | `u32[5]` | 卖一至卖五数量 |
| `142` | `float` | 结算价 `ClearPrice` |

旧版 140 字节扩展体已确认：

| 扩展偏移 | 类型 | 字段 |
|---:|---|---|
| `0` | `float` | 均价 `AverPrice` |
| `4` | `float` | 前结算 `PreClear` |
| `12` | `u32` | 前持仓 `PreVolIn` |
| `16/20` | `float` | 历史高/低 |
| `53/57` | `float` | 涨停/跌停价 |
| `68` | `float` | 市场 8/9 的竞价价 |
| `97` | `u32` | 行情日期 |
| `101` | `u32` | 距零点秒数 `HqTime` |

新版 4 字节扩展只填入旧扩展体 `+101`，也就是行情时间。

## 规范状态记录

`sub_101ADAB0` 把线上字段归一为 712 字节（`0x2C8`）状态记录，并在临界区
内按市场和代码合并。已观察到的关键偏移包括：

- `+64` 行情市场，`+80` 代码；
- `+112/+120/+128` 主要价格，`+136` 数量；
- `+240` `HHMMSS`；
- `+248..280` 五档买价，`+328..364` 五档买量；
- `+408..440` 五档卖价，`+488..524` 五档卖量；
- `+584/+592` 涨跌额和涨跌幅；
- `+664..700` 本地版本/变更序列，`+685` 状态位。

合并时并非总是整块覆盖：部分响应只更新价量、持仓和状态位；完整响应才复制
整条 `0x2C8`。这说明该结构是可复用状态表，不是一次性 UI 参数。

## 证据与下一步

- `output/ida-ttplugin-hqdecode-xrefs.json`
- `output/ida-ttplugin-hqtransport-xrefs.json`
- `output/ida-ttplugin-hqsender-worker-xrefs.json`
- `output/ida-ttplugin-hqreqids.json`
- `output/ida-tpbus-hqstate-xrefs.json`
- `output/ida-ttplugin-redirect-chain.json`
- `output/ida-ttplugin-redirect-targets.json`
- `output/ida-ttplugin-redirect-strings.json`

下一步需要调用方合法运行时的 `Target`/当前连接信息和一帧 9221 响应，才能对
服务器来源、市场号和未命名字段做动态校验。`RedirectData` 的六种翻译结构已经
静态闭合；在端点和响应样本闭合前仍不开放未经验证的联网命令。
