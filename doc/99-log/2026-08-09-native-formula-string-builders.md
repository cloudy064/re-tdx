# 2026-08-09：TCalc GBK 字符串构造与序列语义

## 目标

在证券名称/代码判断之后，继续补齐不依赖 L2、交易会话或私有云的高收益 TCalc
入口。本批聚焦文字标注和自定义选股公式常用的字符串长度、切片、数字格式化及拼接，
同时纠正既有 `CON2STR/STRCAT` 被误作逐 K 线序列的语义偏差。

[通达信官方函数列表](https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html)
明确区分 `CON2STR/STRCAT/STRCAT6` 的非序列计算和
`VAR2STR/VARCAT/VARCAT6` 的逐数据计算，并给出 `STRLEN('通达信')=6`、
`SUBSTR` 从位置 1 开始等公开定义。本轮再以当前 `TCalc.dll` 的处理函数和 32 位
原 DLL 直调锁定 GBK 字节、精度钳制、末柱广播和越界细节。

## 静态恢复

离线 IDA 脚本为
[ida_probe_tcalc_string_builders_status.py](../../output/ida_probe_tcalc_string_builders_status.py)，
完整 Hex-Rays 日志为
[ida-tcalc-string-builders-status.log](../../output/ida-tcalc-string-builders-status.log)。

| 入口 | TCalc 处理函数 | 恢复语义 |
| --- | --- | --- |
| `STRLEN` | `sub_1000EF70` | 读取最后一个字符串句柄，以 ANSI `strlen` 返回字节数并填满序列 |
| `SUBSTR` | `sub_10045720` | 读取最后一个字符串，位置从 1 开始，以字节为单位复制 N 字节并广播 |
| `STRCAT` | `sub_100458A0` | 取两参数末柱字符串，拼接一次后广播 |
| `STRCAT6` | `sub_10045A00` | 取六参数末柱字符串，拼接一次后广播 |
| `STRSPACE` | `sub_10045CE0` | 取末柱字符串追加一个 ASCII 空格后广播 |
| `VARCAT` | `sub_10045E30` | 每柱分别拼接两个字符串 |
| `VARCAT6` | `sub_10045FF0` | 每柱分别拼接六个字符串 |
| `CON2STR` | `sub_10046350` | 只格式化数值末柱并广播，小数位钳制为 0..4 |
| `VAR2STR` | `sub_10046490` | 每柱分别格式化数值，小数位钳制为 0..4 |

原 DLL 内部字符串是本地代码页字节，不是 UTF-8 码点。当前证券名“通达信”编码为
GBK 六字节，所以 `STRLEN` 返回 6；`SUBSTR('通达信',3,2)` 从第三个字节开始取
两个字节，得到“达”。位置小于 1 会钳到第一个字节，超过末尾会钳到最后一个
字节；长度小于等于 0 返回空串，超过剩余长度则截到末尾。

`CON2STR/VAR2STR` 使用 `float` 值和 `printf("%.*f")`，精度小于 0 按 0、超过
4 按 4；原生缺失值格式化为 `-`。解释器现在按同样规则处理，不再允许此前的
0..12 位扩展精度。

## 原 DLL 直调对照

32 位探针源码为
[native_probe_tcalc_string_builders.cpp](../../output/native_probe_tcalc_string_builders.cpp)，
结果为
[native-tcalc-string-builders-probe.json](../../output/native-tcalc-string-builders-probe.json)。
探针直接调用当前 DLL 的字符串池 `+0x445E0/+0x70DC0` 和上述处理器，结果为：

- `strlen_gbk=[6,6,6]`；
- `substr=["达","达","达"]`，`strspace=["达 ","达 ","达 "]`；
- `VAR2STR(10,11,12)` 得到 `10.0/11.0/12.0`；
- `CON2STR(10,11,12)` 三柱均为 `12.0`；
- `VARCAT` 保留 `10.0|/11.0|/12.0|`，`STRCAT` 三柱均为 `12.0|`；
- 六参数的序列与非序列拼接均得到预期 `ABCDEF`。

因此“VAR 逐柱、STR/CON 末柱广播”不是按函数名猜测，而是由同一 DLL 的真实
字符串句柄输入输出对照确认。

## 纯 C++ 接入

解释器新增 7 个支持函数：

- `STRLEN/SUBSTR/STRSPACE/STRCAT6`；
- `VAR2STR/VARCAT/VARCAT6`。

既有 `CON2STR/STRCAT` 同时修正为非序列末柱广播。公共 C++ 编码层新增 UTF-8 到
Windows CP936/GBK 字节转换，使长度和切片以原 DLL 字节口径执行；网页和 JSON 边界仍
使用 UTF-8。字符串生产器在 `StringEnvironment` 中完整物化，只有原 DLL 内部的
数值字符串池句柄继续用不可观察占位表示；将句柄错误地直接用于数值输出仍会被
语义污染审计拒绝，而 `STRLEN/STRCMP/STR2CON` 等精确消费者保持数值安全。

能力清单由 188 个函数/51 个自动符号增至 195/51，
`custom_formula_security_string_functions` 由 7 项增至 14 项，加上 `STKNAME`
共 15 项。Svelte 公式库现通过原有动态能力卡自动显示新计数，无需新增页面代码。

## 未猜测实现的证券状态入口

同一 IDA 批次也确认了四个注册入口，但本轮没有用名称或代码规则伪造：

| 入口 | 处理函数 | 真实依赖 |
| --- | --- | --- |
| `IST0CODE` | `sub_10016030` | 宿主 type 167 返回结构的字节 75/76 |
| `ISSTCODE` | `sub_100160B0` | 宿主 type 120 返回结构的字节 174 |
| `ISQUITCODE` | `sub_10016120` | 宿主 type 120 返回结构的字节 175 |
| `ISQHQQCODE` | `sub_10016190` | 宿主 type 105 返回结构的字节 40/48 |

这些字段可能来自证券主档、交易状态和扩展市场结构。只有继续恢复 TdxW 对应宿主
回调的字段填充来源，或取得等价公开主档后，才适合接入解释器。

## 验证与发布

- 原生 CTest：101/101；
- 临时服务专项：4/4；
- 临时服务 full API 合约：209/209，报告见
  [api-contracts-string-builders-temp-full.json](../../output/api-contracts-string-builders-temp-full.json)；
- 最终正式服务 full API 合约：209/209，报告见
  [api-contracts-string-builders-formal-full.json](../../output/api-contracts-string-builders-formal-full.json)；
- 正式服务专项：5/5，报告见
  [api-contracts-string-builders-formal-selected.json](../../output/api-contracts-string-builders-formal-selected.json)；
- 新契约 `formula-string-builders-inline-post` 在平安银行 120 根真实日线上同时验证
  GBK 长度/切片、嵌套字符串消费者、逐柱 `VAR*` 和末柱广播 `STR*/CON2STR`；
- 正式服务 PID 38980，只监听 `127.0.0.1:8765`；
- EXE SHA-256：
  `A9A199F04BF0AA3E3C8E57388EDA82920A2FDA2BF6792D1FA159B4D4A8F1F8F6`；
- 网页资源：`index-luExy9WX.js`、`index-BBbdHCcI.css`。

运行时、统一命令、HTTP 服务和网页调用链仍为纯 C++；Python 仅用于离线 IDA
自动化，不进入发布包功能实现。
