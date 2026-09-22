# C 工程边界与验证

本目录独立构建，不依赖仓库中的 C++ 工具。历史协议和样本说明仍见 [C README](../README.md)。

## 模块边界

`tdx_base → tdx_protocol → tdx_domain → tdx_runtime → tdx_cli` 表示从底层到上层。
上层链接下层，底层不依赖 CLI；`tdxl1core` 是保留的聚合 CMake target。

- `tdx_base`：字节缓冲、JSON 语法与字符串写出、字符编码、日期、文件缓存、线程适配和 ZIP。
- `tdx_protocol`：端点、7709 帧与连接、目录、资源传输、JSN 表格及原始行 JSON renderer。
- `tdx_domain`：业务记录、计算、重放和各领域 JSON renderer。
- `tdx_runtime`：持久连接池、Hub 和 HTTP/SSE 服务实例。
- `tdx_cli`：每个命令自己的 options 类型、参数表和编排；入口只选择命令、调用并转换退出码。

业务计算不读 CLI 参数。`--member` 指定财务 ZIP 成员；原有 `--zip ... --kind ...` 作为兼容拼写保留。
未知命令参数、数字尾随垃圾、溢出和不存在的日期在执行前拒绝。`--security` 可重复，也可连续列出多个证券。

## 所有权

`tdx_buf`、`tdx_json_doc`、`tdx_jsn_document` 的输出对象必须先调用对应 `init` 或使用 `{0}`。
`init` 只用于首次初始化，不能对持有内存的对象再次调用。解码/解析清空逻辑内容并复用容量，
调用者最终 `free`；失败后的文档/响应没有可消费的半成品。借用的文本和行视图只在所属文档再次解析或释放前有效。

HTTP server 持有连接与线程；handler 不释放 server 拥有的对象。停止服务器后，必须等待所有连接线程退出，
再销毁 Hub。Hub 的 stop 等待当前同步 fetch 返回；fetch 必须有界。停止 Hub 后才可释放连接池、
fetch context 和证券 universe。并发 stop 可等待同一次关闭过程；destroy 需要调用方已经排空其他使用者。

## 行情交付契约

SSE 提供最新状态。队列为每只证券保留一个最新完整记录，后续变化替换尚未发出的记录并移至队尾。
`changed` 只描述最近一次上游变化，消费者应使用完整 `record` 更新状态；此接口不是无损逐笔日志。
订阅时缓存快照进入同一个队列，因此尚未读出的快照也可能被最新 `change` 记录替代。
`coalesced` 记录合并次数，`dropped` 记录实际丢失；`/status` 报告活跃订阅的合计，取消订阅后可下降。
`subscriber_queue_limit` 是容量下限，实际容量为 `max(订阅证券数 + 1, 配置值)`，多出的槽位用于心跳；
硬上限为 65536，因此单订阅最多容纳 65535 只证券。分配失败会结束该订阅，重新连接会重放缓存状态。
订阅数量和总 HTTP 连接数分别受限，请求头有总读取截止时间。

REST snapshot 读取缓存，不触发上游请求。原有 records 保持，新增加 `freshness`，包含证券的
`observed_at_unix_ms`、`age_ms` 和 `stale`。采集时间是本进程观察时间，不是交易所事件时间。
`records` 省略无缓存的证券，`freshness` 覆盖全部请求证券；任一证券缺失或过期时根级 `stale` 为真。
过期判断依据该证券的轮询节拍；缺失或 Hub 已停止也标为过期。
订阅裁剪可能停止采集其他证券，因此调用方应检查新鲜度。Hub 连续失败采用独立的有界退避和抖动，
成功后重置，`consecutive_failures` 与 `retry_in_ms` 可用于观察恢复过程。
状态中的 `polled` 是订阅裁剪后的候选数，包含尚未到期的冷档，不能当作本轮实际 fetch 数。
SSE 的 `wait_timeout_ms` 控制等待超时后发送 `: keep-alive` 注释的间隔，不会因空闲自动断开。

## 持久化和领域值

历史缓存先完成解码与证券身份验证，再提交。写入使用独占临时文件，检查写入及关闭结果，
以平台原子替换操作提交，失败保留旧缓存。损坏缓存可以重新下载；缺失和 I/O 错误有独立状态。
所有日期共用 Gregorian 日历校验。字段名 `_pct` 表示百分数，模型与 JSON 使用同一单位；
`has_*` 继续区分缺失和数值零。

## JSON 输出边界

CLI 负责取数、累计计数和写出完整行；`limit`、原始 `jsn`、债券附息日程及可转债连接摘要
由对应 renderer 生成 JSON。路径、资源名、列名和借用文本统一转义；借用文本按长度处理，
其中的 NUL 不会截断后续内容。输出结束通过统一边界检查 `fflush` 与 `fclose` 的失败。

原始 JSN 行保留 envelope 的 `type`、`resource`、`group`、`row`。同名数据列沿用 `_cell`
后缀；若该名字已经被原始列或先前输出列占用，则使用原始名字加 `_cell_2`、`_cell_3` 等，
直到唯一。普通原始列名优先保留，重复原始列也按同一规则区分。重命名是确定性的，
`columns_renamed` 继续累计各输出行中改名的字段数量。例如原始 `type` 与 `type_cell` 并存时，
前者成为 `type_cell_2`，后者保留 `type_cell`；没有冲突时仍输出 `type_cell`。

债券 schedule 的合法 `YYYYMMDD` 日期继续输出 JSON 数字；不是合法日期的原始文本现在输出
转义后的 JSON 字符串，避免旧实现直接拼接文本生成无效 JSON。这些异常输入的修复，以及
重复 JSN 列名的消歧，不保证与原有错误输出逐字节相同；正常字段名称、数值与日程结构保持。
债券列表中的控制字符也按长度转义；非有限值不能输出为 JSON 数字，普通数值字段输出 `null`，
混合类型的逗号列表保留原始字符串（包括 `inf`、`nan` 及溢出数值）。
`domain-json-independent` 使用 Python 标准库校验这些契约，并拒绝重复 JSON 键。

## 验证入口

```sh
cmake --preset linux-gcc -S c
cmake --build build/c-linux-gcc
ctest --test-dir build/c-linux-gcc --output-on-failure
python c/tools/generate.py --check
```

`TDX_ENABLE_SANITIZERS=ON` 在 POSIX GCC/Clang 上启用 AddressSanitizer 和 UndefinedBehaviorSanitizer。
`TDX_BUILD_FUZZERS=ON` 在 POSIX Clang 上构建 `tdx-fuzz-codecs`，可用 `-runs=20000` 做有界检查。
Python 独立解析器校验 JSON 字符串和多个离线 CLI 输出。HTTP 测试在回环接口启动真实服务，
覆盖五类路由、错误请求、半开连接、过载、端口占用和停止。Hub 测试包含真实线程的退避恢复、合并和新鲜度。

生成器、离线最小输入、来源和哈希清单见 [tools](../tools/README.md)。生成检查不依赖 `output/`、
本机终端安装或原 C++ 实现。真实文件缺失仅跳过对应样本；存在但无法解码必须使测试失败。
