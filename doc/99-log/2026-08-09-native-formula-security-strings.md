# 2026-08-09：TCalc 证券字符串条件与 UPDOWN/NOT

## 目标

继续选择不依赖 L2、券商私有会话或云端授权的高收益 TCalc 注册入口，补齐自定义
选股公式常用的证券名称、代码和字符串判断，并把证券名从真实行情接口可靠地接到
解释器。本批不把 MACD/KDJ 等快捷指标误作行业或板块数据。

[通达信官方函数列表](https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html)
给出了 `NAMELIKE/CODELIKE/NAMEINCLUDE/FINDSTR/STR2CON/UPDOWN/NOT/STKNAME`
的公开用途；本轮继续用当前 `TCalc.dll` 的处理函数和 32 位原 DLL 探针锁定其未写明
的常量填充、失败转换、精确零及浮点容差边界。

## 静态恢复

离线 IDA 脚本为
[ida_probe_tcalc_security_strings.py](../../output/ida_probe_tcalc_security_strings.py)，
完整 Hex-Rays 日志为
[ida-tcalc-security-strings.log](../../output/ida-tcalc-security-strings.log)。定位如下：

| 入口 | TCalc 处理函数 | 恢复语义 |
| --- | --- | --- |
| `STKNAME` | `sub_10045670` | 通过宿主类型 120 取得证券名并写入字符串池 |
| `NAMELIKE` | `sub_10016AA0` | `strncmp(name,arg,strlen(arg)) == 0` |
| `CODELIKE` | `sub_10005B80` | `strncmp(code,arg,strlen(arg)) == 0` |
| `NAMEINCLUDE` | `sub_10005C60` | `strstr(name,arg) != nullptr` |
| `STR2CON` | `sub_1000EEB0` | 对最后一个字符串句柄调用 CRT `atof` |
| `FINDSTR` | `sub_1000F040` | `strstr(A,B) != nullptr` |
| `UPDOWN` | `sub_10016210` | 与前一柱比较，输出 -1/0/1 |
| `NOT` | `sub_1000B590` | 仅精确 0 输出 1，其余有限非零输出 0 |

前三个名称/代码匹配不是拼音、正则或模糊搜索：`NAMELIKE/CODELIKE` 是字节前缀，
`NAMEINCLUDE/FINDSTR` 是字节子串。UTF-8 中完整中文词的前缀/子串关系与原生 GBK
缓冲区一致。空模式沿用 CRT 行为，前缀和子串判断均成立。`atof` 对非法文本返回 0。

## 原 DLL 固定向量

32 位探针源码为
[native_probe_tcalc_security_numeric.cpp](../../output/native_probe_tcalc_security_numeric.cpp)，
原始结果为
[native-tcalc-security-numeric-probe.json](../../output/native-tcalc-security-numeric-probe.json)。
探针直接调用当前 `TCalc.dll+0x16210` 和 `TCalc.dll+0xB590`：

- `UPDOWN` 跳过前导缺失值，并让第一根有效柱保持缺失；
- 容差为 `abs(current) * 1.000000011686097e-7 + 9.999999747e-6`；
- 10 到 10.000005 判 0，10.000005 到 10.000020 判 1；
- 0 到 -0.000005 判 0，-0.000005 到 -0.000020 判 -1；
- `NOT(+/-1e-8)` 均为 0，`NOT(+0/-0)` 均为 1。

这同时修正了解释器旧 `NOT` 使用 `1e-12` 近似真值的偏差。

## 纯 C++ 接入

解释器新增 7 个支持函数：

- `NAMELIKE/CODELIKE/NAMEINCLUDE`；
- `FINDSTR/STR2CON`；
- `UPDOWN/NOT`。

自动字符串符号新增 `STKNAME`。字符串调用直接消费 AST 的字符串序列，不把 UTF-8
名称转成数值替代物；处理函数按原 DLL 读取最后一个字符串句柄并把结果填满序列。
能力清单新增 `custom_formula_security_string_functions` 7 项和
`custom_formula_security_string_symbols` 1 项，总计由 181 个函数/50 个自动符号增至
188/51。Svelte 公式库显示“证券/字符串核心”8 项。

真实契约首次运行发现公开 7709 K 线的 `name` 为空。服务现优先保留协议名称；为空
时从启动阶段已经加载的本地 TNF 证券主表按市场和代码补齐，并设置
`name_source=local-tnf-security-master`。公式单次执行、全库审计、扫描、回测、组合
策略和普通 K 线接口共用这一补全规则，不新增 Python、数据库或网络依赖。

## 验证与发布

- CTest：101/101；
- Svelte：0 错误、0 警告，生产构建成功；
- 临时服务 full API 契约：208/208，报告见
  [api-contracts-security-string-temp-full.json](../../output/api-contracts-security-string-temp-full.json)；
- 正式服务专项：4/4，报告见
  [api-contracts-security-string-formal-selected.json](../../output/api-contracts-security-string-formal-selected.json)；
- 新契约 `formula-security-string-inline-post` 在平安银行 120 根真实日线上验证名称/代码
  前缀、名称/字符串包含、转换失败、`UPDOWN/NOT` 以及 `STKNAME` 绘图文本；
- 正式服务 PID 22256，只监听 `127.0.0.1:8765`；
- EXE SHA-256：
  `65D3DDDBC0B1E540C058E596F8409CF1AEF679420647EF8D1D72188576B8D734`；
- 网页入口资源：`index-luExy9WX.js`、`index-BBbdHCcI.css`。

运行时和发布包仍为纯 C++；Python 只用于离线 IDA 自动化，不进入工具命令、服务或
网页调用链。
