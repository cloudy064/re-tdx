# TCalc type-167 一级研究行业与主营构成闭环

## 结果

纯 C++ 公式解释器新增两个证券文字裸符号：

- `LEVEL1HYBLOCK`：当前证券所属一级研究行业；
- `MAINBUSINESS`：通达信增强功能文件中的主营构成摘要。

二者可参与 `STRCMP/STRCAT/DRAWTEXT_FIX` 等字符串表达式。真实
`SZ000001 平安银行` 返回 `LEVEL1HYBLOCK=银行`、
`MAINBUSINESS=零售金融业务`，末柱比较值均为 1，文字图元为
“银行零售金融业务”。运行时没有 Python 转发，也不依赖 Level2 或付费账号。

通达信官方函数清单对两项的公开描述分别是“返回品种所属一级研究行业”和
“主营构成（PC 端增强功能集版本支持）”：
<https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html>。

## 原 DLL 与宿主证据

`output/ida_probe_tcalc_block_metadata.py` 的静态结果确认：

| 符号 | opcode | TCalc 处理函数 | 宿主请求 |
| --- | ---: | --- | --- |
| `LEVEL1HYBLOCK` | 1372 | `sub_10045460` | type 167，读取偏移 78 |
| `MAINBUSINESS` | 1355 | `sub_10045590` | type 167，读取偏移 0 |

TdxW 的 type-167 回调会先清零 400 字节结果区。继续追踪宿主写入路径得到：

- 偏移 0 由 `sub_4FA420(market,numeric_code,0)` 写入；该函数延迟读取
  `T0002/hq_cache/specgpext.txt`，市场和代码按 `atol` 比较，返回第一条匹配
  记录的第三个竖线分隔字段；
- 偏移 78 来自证券研究行业代码的前三字节，再通过 `tdxzs3.cfg` 映射一级研究
  行业名称；平安银行的研究行业赋值为 `X500102`，三级树为
  `银行 → 全国性银行 → 股份制银行`，因此一级名称为“银行”；
- 同一 type-167 结构的偏移 358/379 属于 `MOREHYBLOCK`，但会受偏移 357 的
  普通/研究行业模式字节控制，不能直接等同于当前网页展示的叶子行业。

主要离线证据：

- `output/ida-tcalc-block-metadata-v2.log`，SHA-256
  `153338EE6EF089C7A0487AE809AC241F2B8C725D748503BF00EA12C1CB392C3D`；
- `output/ida-tdxw-type167-text-fields-v3.log`，SHA-256
  `96DCDCECB5F74F67A44FA63CB4FE6B2B6C0DE095B2433DD7E502F4457926D650`。

## 纯 C++ 实现

`blocks.cpp` 增加 `specgpext.txt` 的 GBK 解析和只读缓存：

- 以 `(numeric market,numeric code)` 为键，保留第一条记录；
- 文件不存在或证券无记录时返回空字符串，复现宿主清零后的可选增强字段；
- 缓存以规范路径、修改时间和文件大小失效，用户更新通达信数据后无需重启；
- 当前文件解析出 5,550 个唯一市场/代码记录。

`formula_context.cpp` 直接复用现有 `tdxzs3.cfg + tdxhy.cfg` 研究行业树绑定一级
名称，并把名称、来源键和 `specgpext.txt` 路径/记录数放入
`context_metadata`。`formula_engine.cpp` 把两项登记为字符串自动上下文依赖，
但不伪造没有注册证据的零参数函数形式。

能力清单新增：

```text
custom_formula_type167_text_symbol_count = 2
custom_formula_type167_text_symbols = [LEVEL1HYBLOCK, MAINBUSINESS]
```

支持函数仍为 218，自动符号由 65 增至 67；390 条静态注册表中的已识别并集由
265 增至 267，剩余 123。完整公式覆盖证据为
`output/native-formula-coverage-type167.json`，SHA-256
`2AA265D528722DDBE6DDF88089C00884F541F62E7D9E55CB09A11872F3D17490`；
下一批差分队列为 `output/native-formula-registry-next-audit-v3.json`。

## 契约、测试与发布

新增 `formula-type167-text-inline-post` 固定契约，要求：

- 语义分析只报告 `LEVEL1HYBLOCK/MAINBUSINESS` 两项自动依赖；
- 平安银行 120 根真实日线的末柱比较值均为 1；
- 元数据精确指向研究行业 `X50` 与 `specgpext.txt` 第三字段来源。

验证结果：

- 公式引擎专项测试通过；
- API 契约求值器专项测试通过；
- CTest 101/101；
- 临时服务 full API 217/217，共 218 次网络请求；
- 正式服务健康、首页、公式覆盖、新 type-167 公式 4/4。

完整报告为 `output/native-type167-full-api.json`（SHA-256
`794A8E11E615C336EDB8CE1D350045005E86F01C695EF932F9A37C9FEEA2B4FF`）和
`output/native-type167-formal-final-api.json`（SHA-256
`E62B520DAAA56EF519E25C1511F20B960D5F802F73C4218672614BFF1215A0FB`）。

正式 EXE 为 42,954,413 字节，build/dist SHA-256 均为
`CA2B857A55D5F4057151D9D7BEC412F542D33207A029D3D5EE9E383B733A988F`；服务 PID
13648，只监听 `127.0.0.1:8765`，健康页保持 `native_cpp=true`、
`python_runtime=false`。旧版本另存为
`output/tdx-tool-type167-predeploy-rollback-20260810.exe`，SHA-256
`CD8CDC64A17FD28C927DBD5BC7422D3D91C34C0893A345CF2DF1727C4D8D1EF6`。
网页资源未改动，临时 8875 已关闭。

## 下一步与边界

继续优先处理非 L2 入口，但只有来源和显示选择都闭合后才进入运行时：

1. `MOREHYBLOCK`：已知 type-167 偏移 358/379，下一步证明偏移 357 模式来源；
2. `ZHBLOCK/ZDBLOCK/SIMIBLOCK`：已知 command 8 类别分别为 3/2/0，下一步闭合
   组合目录、自定义 `blocknew/*.blk` 名称目录和相似证券选择；
3. `GNBKZSCODE/FGBKZSCODE/GETNAMEOFCODE`：继续追踪代码到名称/指数代码映射；
4. `FINONE/GPJYONE/BKJYONE/SCJYONE/GPONEDAT`：在单点参数、末值和披露时点
   语义证明后复用现有类型化序列。

账户交易状态、券商私有信号、插件回调和 Level2 数据继续排除。
