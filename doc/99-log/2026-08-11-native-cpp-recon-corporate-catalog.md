# native C++ 可维护性重构：recon market_corporate 契约目录化

日期：2026-08-11
范围：`native/src/recon/recon_contract_market_corporate_{insights,company,offerings,ipo}.cpp`

## 动机

上一批（bond_reference_service_query 拆分）在按集中度排序时记录了一类待办形态：
`recon/recon_contract_*.cpp` 共 14 个文件约 5700 行，集中度 95–97%，全部是
`validate_*_contract()` 里一条按 `contract_id` 分发的长 `if/else` 链。

本批取其中 `market_corporate` 子族的 4 个文件、30 个分支，改写为**目录 + 校验函数**模式。
该模式在同目录的 `recon_contract_market_data_bonds_catalog.cpp` 已经落地，本批只是把它推广开：

- 每个分支体提为一个 `void validate_<id>(const Json& document, Json& result)` 自由函数；
- `constexpr std::array` 做 id→函数指针表；
- `static_assert(unique_contract_ids())` 校验 id 唯一；
- 分发器退化成一个循环。

前置确认：4 个文件的第三个形参（context）**均未命名**，任何分支都读不到它，因此 30 个校验函数
可以统一匹配 `recon_contract_market_corporate_internal.hpp` 里已有的
`MarketCorporateCatalogContractValidator = void (*)(const Json&, Json&)`；命名空间开头到分发器
之间也没有前导代码需要搬迁。

## 相比 if/else 链的实质收益

不只是"看起来整齐"。`static_assert` 会在编译期拒绝重复 id，而 `if/else` 链对重复 id 是**静默接受**的：
第二个同名分支永远走不到，成为死代码且无人报错。这一点做了变异验证（见下）。

## 方法：先取指纹，再动代码

改写前必须有行为基线。契约校验的 CLI 入口 `recon-contract` 默认 `--base-url
http://127.0.0.1:8765`，而 8765 是禁区，因此不能走 CLI。改走库内路径：

```cpp
// native/include/tdx/recon.hpp:13-17
Json evaluate_api_contract_response(const std::string& contract_id, int status,
                                    const std::string& content_type,
                                    const std::string& body,
                                    const Json& context = Json::object());
```

临时工具对**全部 224 个契约 id** × 4 种响应体（`empty-object`、`skeleton`、`one-row`、
`array-root`）逐一求值并转储报告，全程进程内，不发一个 HTTP 包。基线 64,348 行，
896 份报告，0 抛异常。

改写后重新取指纹，`diff` 为空——224 个契约的断言名、顺序、通过/失败逻辑全部逐字节一致。
这条比"测试通过"强得多：它把未被 ctest 覆盖的契约也钉住了。

结构面另做三项核对：

- 原分支数 = 新校验函数数 = 表项数（11/8/6/5，全部相等）；
- 30 个 id 与原文件**逐个相同且顺序相同**；
- 原文件里 8 空格缩进的 `return` 只有 1 处/文件，即 `} else { return false; }`，
  转换器在 `    } else {` 处终止，正确排除，没有分支体内的提前返回被吞掉。

## 变异验证

把 ipo 表里第二行的 id 改成与第一行重复（`ipo-review-live` → `ipo-guidance-live`）：

```
recon_contract_market_corporate_ipo.cpp:429:34: error: static assertion failed
```

编译期即拒绝，随后还原。同样的重复放进原来的 `if/else` 链则能正常编译并静默产生死分支。

## 结果

| 文件 | 改写前 | 改写后 |
| --- | --- | --- |
| insights | 549/568 = 96% | 131/636 = 20% |
| company | 457/476 = 96% | 80/532 = 15% |
| offerings | 421/440 = 95% | 82/488 = 16% |
| ipo | 381/400 = 95% | 97/444 = 21% |

`cmake --build` 零警告（`-Wall -Wextra -Wpedantic`），ctest 108/108。
公开 API 未动：导出的 `validate_market_corporate_*_contract` 四个函数签名与语义不变，
CLI、HTTP、JSON schema 均无变化。改动文件不含 8765 与 L2 相关引用。

## 遗留

`validate_financial_insights` 仍有 131 行（insights 中最大），内部结构是
schema 断言 / `minimums` 总量核对 / 一段 82 行的 14 标志位循环 / `exact_sources` 四块，
其中标志位循环可以提取。但该文件集中度已降到 20%，没有函数再掩盖文件，
优先级低于把同样的链式形态推广到余下 18 个 recon 文件（约 188 个分支），故留待后续。

## 环境陷阱：MinGW g++ 静默失败

本批开头卡在一个与代码无关的现象：`g++` 对**任何**输入（包括 `int main(){return 0;}`）
都返回 exit 1 且 stdout/stderr 全空，`cmake --build` 只报 `Error 1` 不给诊断。

根因是 DLL 搜索路径，不是编译错误。`cc1plus.exe` 位于
`mingw64/lib/gcc/x86_64-w64-mingw32/15.1.0/`，而它依赖的 `libmpfr-6.dll`、`libmpc-3.dll`、
`libisl-23.dll` 在 `mingw64/bin/`，需要靠 PATH 找到。直接运行 `cc1plus.exe --version` 才把
真正的错误暴露出来：

```
cc1plus.exe: error while loading shared libraries: libmpfr-6.dll: cannot open shared object file
```

shell 的 PATH 里确有 `/mingw64/bin`，但那是 **MSYS 风格路径**，Win32 加载器无法解析；
必须是 `/c/msys64/mingw64/bin`（会被翻译成 `C:\msys64\mingw64\bin`）。因此本仓库的
所有编译与 `cmake --build` 调用都要加前缀：

```sh
PATH="/c/msys64/mingw64/bin:$PATH" cmake --build . -j8
```

附带教训：`g++ ... 2>&1 | head -30` 会吞掉退出码，让静默失败看起来像"编译干净"。
判定成败必须显式取 `$?` 或 `${PIPESTATUS[0]}`，本批一开始就因此误判了一次。
