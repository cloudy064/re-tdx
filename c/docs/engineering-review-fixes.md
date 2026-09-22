# C 工程评审修复清单

范围为 `c/` 及其 CI 工作流，未修改 C++ 源码。下面记录已落地的实现和回归入口；接口约定详见 [工程边界与验证](engineering.md)。测试名称均为 CTest 名称，模糊测试另列构建目标。

后续一致性复核、归档时的增量检查及原始证据哈希见 [阶段归档](../../doc/99-log/2026-09-22-c-engineering-review.md)。

| 组 | 已实现的修复与代码入口 | 对应验证 |
|---|---|---|
| 1. HTTP 生命周期 | [服务实例 API](../include/tdx_serve.h) 统一拥有连接和线程，handler 不再重复释放连接。连接在线程启动前登记，限制总连接数并设置请求头总读取截止时间；[停止过程](../src/tdx_serve.c) 中断客户端并等待全部线程结束。[CLI 清理](../src/cli/command_serve.c) 先停止 Hub，再释放连接池与 fetch context，覆盖监听失败路径。 | `l1stream-serve`、`l1stream-hub_lifecycle`：畸形请求、半关闭、慢请求头、连接上限、端口占用、带 SSE 的并发停止。 |
| 2. 缓冲所有权 | [缓冲契约](../include/tdx_bytes.h) 明确首次 `init`、后续 `clear` 复用、最终 `free`；[帧解码](../src/tdx_frame.c)、[JSON](../include/tdx_json.h)、[JSN](../include/tdx_jsn.h) 明确失败状态和借用视图有效期。[报价请求 builder](../src/tdx_quote.c) 保留调用方分配，修复多批次轮询泄漏；同步修复测试中的重复初始化。 | `l1stream-bytes`、`l1stream-frame`、`l1stream-json`、`l1stream-jsn`、`l1stream-quote`、`l1stream-pool`、`l1stream-auction`、`l1stream-timeline`：成功、失败及再次复用。 |
| 3. JSON 边界 | [共享字符串写出](../include/tdx_json_write.h) 按长度转义文本，保留内嵌 NUL 后的内容；[解析器](../src/tdx_json.c) 严格检查语法及资源上限。原始 JSN、限价、债券日程和联接摘要移入各自 renderer；[JSN 输出](../src/tdx_jsn_json.c) 确定性消除字段名冲突，[行情事件](../src/tdx_format.c) 输出完整 JSON 对象。 | `l1stream-json`、`l1stream-domain_renderers`、`l1stream-json-independent`、`l1stream-domain-json-independent`、`l1stream-cli-contracts`；独立 Python 解析检查 JSON，并检查重复键。 |
| 4. Hub 交付与新鲜度 | [Hub](../src/tdx_hub.c) 为每只证券保留一个待发的最新完整记录，合并更新并移至队尾，避免静默证券被挤掉，保持发送序列递增；新增 `coalesced`。快照保留原 `records`，新增 `freshness`、采集时间、年龄和 `stale`；[接口](../include/tdx_hub.h) 明确这是缓存状态与最新值流。 | `l1stream-hub`、`l1stream-hub_lifecycle`、`l1stream-serve`：慢消费者、静默证券、序列顺序、过期缓存及停止后的快照。 |
| 5. 失败退避与同步 | [Hub 调度](../src/tdx_hub.c) 对连续失败使用独立、有上限的指数退避和抖动，成功后复位；fetch 先写局部统计，再持锁发布，避免 `/status` 数据竞争。停止操作等待在途 fetch，并协调并发 stop；[线程适配](../src/tdx_thread.c) 显式管理初始化状态，POSIX 条件等待使用单调时钟。 | `l1stream-hub_lifecycle`、`l1stream-pool`：真实后台线程的失败恢复、并发查询和停止，以及同一连接池的多轮复用、失败后恢复。 |
| 6. CLI 分层 | [入口](../src/main.c) 仅分发命令；[类型化注册表](../src/cli/cli_registry.c)、[命令参数类型](../src/cli/cli_commands.h) 与各 `command_*.c` 分离。参数校验拒绝未知选项、数值尾随垃圾和溢出；JSON 渲染下沉到领域模块，[输出边界](../src/cli/cli_support.c) 检查刷新、关闭失败。 | `l1stream-cli`、`l1stream-cli-contracts`、`l1stream-domain_renderers`，以及 `l1stream-directory` 的目录渲染契约。 |
| 7. 日期与单位 | [日期模块](../include/tdx_date.h) 提供统一 Gregorian 校验，CLI、历史数据和债券计算共用。[定价模型](../include/tdx_pricing.h) 与 [renderer](../src/tdx_pricing_json.c) 统一 `_pct` 为百分数，消除模型和 JSON 的重复换算；继续用 `has_*` 区分缺失与零。 | `l1stream-date`、`l1stream-daily`、`l1stream-minute`、`l1stream-trades`、`l1stream-bond_math`、`l1stream-pricing`：闰日、非法日期和模型/JSON 单位一致性。 |
| 8. 缓存事务 | [文件边界](../src/tdx_cache.h) 区分缺失与 I/O 错误；[缓存写入](../src/tdx_cache.c) 使用独占临时文件，检查写入和关闭结果，再原子替换，失败保留旧缓存。[历史取数](../src/tdx_zst_day.c) 完成解码与证券身份验证后才提交，损坏缓存可重新下载。 | `l1stream-cache`、`l1stream-zst`：注入写入、关闭和替换失败，检查旧缓存保留、无效内容和身份不匹配。 |
| 9. 测试边界 | 增加 [真实回环 HTTP](../tests/test_serve.c)、[驻留 Hub](../tests/test_hub_lifecycle.c)、[持久 pool](../tests/test_pool.c)、[独立 JSON 验证](../tests/verify_domain_renderers.py) 与离线 CLI 契约；真实样本缺失只跳过对应样本，存在但解码失败会使测试失败。[模糊测试入口](../tests/fuzz_codecs.c) 覆盖字节和解析边界。 | 上述定向 CTest；全套 CTest 配合 ASan/UBSan；`tdx-fuzz-codecs` 使用有限输入次数和时长。 |
| 10. 生成、构建与跨平台 | [CMake](../CMakeLists.txt) 拆分 `base → protocol → domain → runtime → CLI`，保留聚合 target，并正确链接线程、平台网络及编码库。[生成入口](../tools/generate.py) 使用保留输入、[来源与哈希](../tests/fixtures/sources.json) 和 [产物清单](../tools/generated.json) 离线复现；不依赖本机终端、忽略目录或 C++ 源码。[预设](../CMakePresets.json) 覆盖 GCC、Clang、MinGW、MSVC。 | `l1stream-generated`、`tdx-verify-generated`，各编译器预设的构建与 CTest；[GitHub CI 配置](../../.github/workflows/c.yml) 已加入，但本次尚未在 GitHub 远程执行。 |

## 完成验证（2026-09-22）

| 环境 | 最终结果 |
|---|---|
| Windows MinGW GCC 15.1.0 | 完整 CTest 51/51 通过 |
| Windows MSVC 19.50 | 完整 CTest 51/51 通过 |
| Linux Clang 22.1.3，ASan/UBSan，启用泄漏检测 | 完整 CTest 51/51 通过，无 sanitizer 报告 |
| Clang libFuzzer | JSON / JSN / frame 共用入口 20,000 次有界运行通过 |
| 一次真实行情抽样 | SZ000001 返回 1/1 条记录、买卖各五档，独立 JSON 解析通过 |

完整 CTest 包含离线生成一致性检查、独立 JSON 验证和五项离线 CLI 契约。
Linux 使用干净构建目录并核对收尾源码哈希；生成产物还在不含本机终端和 C++ 源码的临时目录中重建检查。
GitHub CI 已配置，但本次未在 GitHub 执行；上表记录的是本地 Windows 和 SSH Linux 的实测结果。

在 `c/` 目录执行以下命令；需要对应编译器、Ninja、zlib，以及用于独立输出检查和生成检查的 Python：

```sh
cmake --preset linux-clang-sanitize
cmake --build --preset linux-clang-sanitize
ctest --preset linux-clang-sanitize
python tools/generate.py --check
```

Windows 分别替换为 `windows-mingw` 或 `windows-msvc` 预设，在 UCRT64 或 Visual Studio 开发者环境执行；MSVC 的 zlib 工具链配置见 [生成与构建说明](../tools/README.md)。日常修改按受影响目标运行，例如：

```sh
ctest --test-dir ../build/c-linux-clang-sanitize --output-on-failure -R '^l1stream-(hub|hub_lifecycle|serve|pool)$'
```

这次涉及共享解析器、输出契约和构建边界，已完成上述全套验证。异机同步源码时使用新的构建目录，避免归档保留的源码时间导致旧对象被误复用。
