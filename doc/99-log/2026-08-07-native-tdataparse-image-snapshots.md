# TDataParse 盘口快照与纯 C++ 解码闭环

> 日期：2026-08-07  
> 范围：`ida/TDataParse.dll.i64`、`ida/TdxW.exe.i64`、缓存读取/解压调用链、
> `native/src/image_data.cpp` 和 32 位原 DLL 差分宿主。

## 本轮目标

把此前仅知道“输出每条 1032 字节”的 `TDataParse.dll!fn_TGetImageData`
推进到三个可验收结果：

1. 还原输入控制流和输出字段布局；
2. 在统一工具中以纯 C++ 独立实现，不转发 Python、不加载原 DLL；
3. 用原 DLL 对同一输入逐字节验证新实现。

## 静态结果

- `fn_TGetImageData` 转发到 `sub_10004E50`；
- 输入以 `0x03/0x02/0x04` 分隔两字节标签和文本值；
- 状态跨记录继承，同时间合并，相邻非时间载荷相同则去重；
- 输出是 1032 字节完整盘口快照，并非图片：
  - 时间、昨收、OHLC、最新价、持仓量、累计量额；
  - 买一至买十、卖一至卖十价量；
  - 买一/卖一各最多 50 项委托队列；
  - 168 字节文本区和八个尚待业务命名的辅助字段；
- `+64` 在去重后由相邻累计成交量差生成；
- `TdxW` 本地缓存包装头为 24 字节，`+8/+16` 分别是压缩/解压长度，
  zlib 流从 `+24` 开始；
- 历史和当日来源分别出现于 `hishf/date/...` 与 `todayhf/...`，落地缓存位于
  `T0002/zst_cache`。

完整偏移表见[本地盘口快照结构](../03-memory-layout/01-image-data-snapshot.md)。

## 原生实现

新增统一子命令：

```text
tdx-tool image-data decode --input FILE
  [--input-format auto|wrapped|raw]
  [--limit N]
  [--record-output FILE]
  [--output FILE]
  [--compact]
```

实现边界：

- 自动识别并解压缓存包装或直接读取原始控制流；
- 初版输出 `tdx-image-data-snapshots-v1` JSON；后续消费方标定后已升级为 v2；
- 可重新编码标准化的 1032 字节记录；
- 保留未知线标签；当前只有 `1C..1F` 继续使用中性名，`1G..1J` 已业务命名；
- 完全离线，不加载 `TDataParse.dll`。

新增文件：

- `native/include/tdx/image_data.hpp`
- `native/src/image_data.cpp`
- `native/tests/image_data_tests.cpp`

## 差分验收

`output/tdataparse_reference_check.cpp` 使用 x86 MSVC 编译，以与 32 位原 DLL
一致的位数运行。合成输入覆盖三条提交记录：第一条完整状态，第二条只更新
时间、最新价和累计量，第三条只更新时间以触发相邻重复去除；同时覆盖首档
买卖价量、两侧委托队列和未知标签。

验收结果：

```text
count_pass=3
decode_count=2
reference_bytes=2064
native_bytes=2064
diff_count=0
sha256=D7C99761D8648513AC729F9205A8B69850C91FA07E2FD8DC5A44423E9F44C540
```

这证明新实现生成的标准化记录与原 DLL 完全一致，而不只是按伪代码自洽。

测试还发现原 DLL 的容量边界：初始容量取 `source_length / 1032`；容量不足
触发扩容时，只复制 `record_count - 1` 条旧记录，可能产生全零洞。差分夹具把
源长度补到三条容量以避免参考实现自身缺陷影响字段验证；纯 C++ 版本使用动态
容器，不继承该缺陷。

## 尚未越界命名的部分

当前真实 `C:\new_tdx\T0002\zst_cache` 为空，故 `1C..1F` 和 `0D` 的完整
业务含义仍缺真实缓存/UI 对照。后续已从 `TdxW.exe` 消费方及 UI 标签静态确认
`1G/1H/1I/1J` 为买均、总买、卖均、总卖；其余字段不根据值域猜名。

## 构建与部署

- 原生测试：92/92 通过；
- 正式功能目录：137 项，包含 CLI-only `image-data decode`；
- 固定 API 契约：129/129 通过；
- 正式服务：`http://127.0.0.1:8765`，部署后 PID 11732；
- 正式 EXE SHA-256：
  `38740B7951D500187762E0918836B8B3514480F4E7D15CB038D72E277EE6340C`。
